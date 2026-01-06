// transport_bm_stream.c — 裸机“块流”传输（中断/定时器驱动，静态环形缓冲）
// 纯 C / C99，可用于无 RTOS 的 MCU 工程。
// 与 transport_t 接口对齐：recv 非阻塞；send 要求整帧一次性入队。

#include "transport_bm_stream.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* ================= 编译期上限（按需调整） =================
   注意：实际运行容量会在 init() 中被规范为“不超过上限的 2 的幂”，
   这样即可使用 & (cap-1) 做取模，避免除法。 */
#ifndef BM_RX_RING_MAX
#define BM_RX_RING_MAX   4096
#endif
#ifndef BM_TX_RING_MAX
#define BM_TX_RING_MAX   4096
#endif

/* ================= 关中断临界区 ================= */
#if defined(__ARMCC_VERSION) || defined(__GNUC__) || defined(__ICCARM__)
  #include "cmsis_compiler.h"
  static inline uint32_t bm_enter_crit(void){ uint32_t pm = __get_PRIMASK(); __disable_irq(); return pm; }
  static inline void     bm_exit_crit(uint32_t pm){ if (!pm) __enable_irq(); }
#else
  static inline uint32_t bm_enter_crit(void){ __disable_irq(); return 1; }
  static inline void     bm_exit_crit(uint32_t pm){ (void)pm; __enable_irq(); }
#endif

/* ================= 上/下取 2 的幂（C99 版本） ================= */
static uint32_t round_up_pow2_u32(uint32_t x)
{
    if (x <= 2U) return 2U;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static uint32_t round_down_pow2_u32(uint32_t x)
{
    if (x < 2U) return 2U;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return (x + 1U) >> 1;  // 最大的不超过 x 的 2 的幂
}

/* ================= 上下文 ================= */
typedef struct {
    void* hw;
    const bm_stream_ops_t* ops;

    // 静态环形缓冲（字节级）
    volatile uint32_t rx_head, rx_tail;
    volatile uint32_t tx_head, tx_tail;

    uint16_t rx_cap, tx_cap;     // 实际容量（2 的幂，<= 上限）
    uint8_t  rx_buf[BM_RX_RING_MAX];
    uint8_t  tx_buf[BM_TX_RING_MAX];
} bm_ctx_t;

static bm_ctx_t   g_bm;
static transport_t g_tp;

/* =============== 环形缓冲 helpers（并发安全） =============== */
static inline uint32_t ring_count(uint32_t head, uint32_t tail, uint32_t cap){
    return (head - tail) & (cap - 1U);
}
static inline uint32_t ring_space(uint32_t head, uint32_t tail, uint32_t cap){
    return (cap - 1U) - ring_count(head, tail, cap);
}

/* put 多字节到 TX 环（主/ISR 均可调用） */
static size_t tx_ring_write(bm_ctx_t* t, const uint8_t* data, size_t len){
    uint32_t pm = bm_enter_crit();
    uint32_t cap = t->tx_cap;
    uint32_t head = t->tx_head, tail = t->tx_tail;
    size_t space = ring_space(head, tail, cap);
    if (space < len) len = space;
    size_t n = len;

    while (len--){
        t->tx_buf[head] = *data++;
        head = (head + 1U) & (cap - 1U);
    }
    t->tx_head = head;
    bm_exit_crit(pm);
    return n;
}

/* 从 TX 环尽可能“吐”向硬件（非阻塞尝试写），返回写出的字节数 */
static size_t tx_drain_hw(bm_ctx_t* t){
    if (!t->ops || !t->ops->tx_try_write) return 0;
    uint32_t cap = t->tx_cap;
    size_t total = 0;

    for (;;) {
        uint32_t pm = bm_enter_crit();
        uint32_t head = t->tx_head, tail = t->tx_tail;
        size_t count = ring_count(head, tail, cap);
        if (count == 0) { bm_exit_crit(pm); break; }

        // 优先发送第一段连续区
        size_t first = (head >= tail) ? (size_t)(head - tail) : (size_t)(cap - tail);
        const uint8_t* p = &t->tx_buf[tail];
        bm_exit_crit(pm);

        size_t wrote = t->ops->tx_try_write(t->hw, p, first);
        if (wrote == 0) break; // 硬件忙

        pm = bm_enter_crit();
        t->tx_tail = (t->tx_tail + (uint32_t)wrote) & (cap - 1U);
        bm_exit_crit(pm);
        total += wrote;
    }
    return total;
}

/* 把一段写入 RX 环（ISR 友好），返回写入的字节数（可能小于 len） */
static size_t rx_ring_write(bm_ctx_t* t, const uint8_t* data, size_t len){
    uint32_t pm = bm_enter_crit();
    uint32_t cap = t->rx_cap;
    uint32_t head = t->rx_head, tail = t->rx_tail;
    size_t space = ring_space(head, tail, cap);
    if (space < len) len = space;
    size_t n = len;

    while (len--){
        t->rx_buf[head] = *data++;
        head = (head + 1U) & (cap - 1U);
    }
    t->rx_head = head;
    bm_exit_crit(pm);
    return n;
}

/* 从 RX 环取数据到 buf（主循环非阻塞读），返回拷贝字节数（可为 0） */
static size_t rx_ring_read(bm_ctx_t* t, uint8_t* buf, size_t maxlen){
    uint32_t pm = bm_enter_crit();
    uint32_t cap = t->rx_cap;
    uint32_t head = t->rx_head, tail = t->rx_tail;
    size_t avail = ring_count(head, tail, cap);
    if (avail > maxlen) avail = maxlen;

    // 先读第一段连续区，如需再读第二段
    size_t n1 = 0, n2 = 0;
    if (avail){
        if (head >= tail){
            n1 = avail;
            memcpy(buf, &t->rx_buf[tail], n1);
        } else {
            n1 = (size_t)(cap - tail);
            if (n1 > avail) n1 = avail;
            memcpy(buf, &t->rx_buf[tail], n1);
            n2 = avail - n1;
            if (n2){
                memcpy(buf + n1, &t->rx_buf[0], n2);
            }
        }
        t->rx_tail = (t->rx_tail + (uint32_t)avail) & (cap - 1U);
    }
    bm_exit_crit(pm);
    (void)n2; // 仅为阅读友好，避免未使用警告
    return avail;
}

/* =============== transport_t 实现 =============== */
static int bm_init(void* ctx, const char* config)
{
    (void)config;
    bm_ctx_t* t = (bm_ctx_t*)ctx;
    memset(t, 0, sizeof(*t));

    /* 取用户（运行时）想要的大小或默认上限 */
    uint32_t rx_req = (t->rx_cap == 0U) ? (uint32_t)BM_RX_RING_MAX : (uint32_t)t->rx_cap;
    uint32_t tx_req = (t->tx_cap == 0U) ? (uint32_t)BM_TX_RING_MAX : (uint32_t)t->tx_cap;

    /* 夹到上限范围 */
    if (rx_req > (uint32_t)BM_RX_RING_MAX) rx_req = (uint32_t)BM_RX_RING_MAX;
    if (tx_req > (uint32_t)BM_TX_RING_MAX) tx_req = (uint32_t)BM_TX_RING_MAX;

    /* 规范为 2 的幂（便于用 & (cap-1) 取模）
       - 先向上取幂；若超过上限，再向下取到不超过上限的最大 2 的幂 */
    uint32_t rx_p2 = round_up_pow2_u32(rx_req);
    if (rx_p2 > (uint32_t)BM_RX_RING_MAX) rx_p2 = round_down_pow2_u32((uint32_t)BM_RX_RING_MAX);

    uint32_t tx_p2 = round_up_pow2_u32(tx_req);
    if (tx_p2 > (uint32_t)BM_TX_RING_MAX) tx_p2 = round_down_pow2_u32((uint32_t)BM_TX_RING_MAX);

    t->rx_cap = (uint16_t)rx_p2;
    t->tx_cap = (uint16_t)tx_p2;

    t->rx_head = t->rx_tail = 0U;
    t->tx_head = t->tx_tail = 0U;

    return 0;
}

static int bm_wait_connection(void* ctx){
    (void)ctx; // 裸机无连接概念
    return 0;
}

static int bm_recv(void* ctx, uint8_t* buf, int len){
    if (!buf || len <= 0) return -1;
    bm_ctx_t* t = (bm_ctx_t*)ctx;
    size_t n = rx_ring_read(t, buf, (size_t)len);
    return (int)n; // 可能为 0（表示当前无数据）
}

static int bm_send(void* ctx, const uint8_t* buf, int len){
    if (!buf || len <= 0) return -1;
    bm_ctx_t* t = (bm_ctx_t*)ctx;

    /* 要求一次性接纳整帧；空间不足则返回 -1（让上层稍后重试） */
    size_t written = tx_ring_write(t, buf, (size_t)len);
    if (written < (size_t)len) {
        // 回退已写入（保持“要么全入队，要么不入队”的语义）
        uint32_t pm = bm_enter_crit();
        t->tx_head = (t->tx_head - (uint32_t)written) & (t->tx_cap - 1U);
        bm_exit_crit(pm);
        return -1;
    }

    // 尝试立刻吐给硬件（若硬件忙，会留在环里，等待 TX 就绪中断或下一次 poll）
    (void)tx_drain_hw(t);
    return len;
}

static void bm_cleanup(void* ctx){
    (void)ctx;
}

/* =============== Factory =============== */
transport_t* transport_bm_stream_create(const bm_stream_ops_t* ops, void* hw){
    memset(&g_bm, 0, sizeof(g_bm));
    g_bm.ops = ops;
    g_bm.hw  = hw;

    g_tp.init            = bm_init;
    g_tp.wait_connection = bm_wait_connection;
    g_tp.recv            = bm_recv;
    g_tp.send            = bm_send;
    g_tp.cleanup         = bm_cleanup;
    g_tp.impl_ctx        = &g_bm;
    return &g_tp;
}

void transport_bm_set_buffer_sizes(uint16_t rx_ring, uint16_t tx_ring){
    // 在 init() 之前调用：这里仅记录“期望值”，init 时会规范为 2 的幂且不超过上限
    if (rx_ring) g_bm.rx_cap = rx_ring;
    if (tx_ring) g_bm.tx_cap = tx_ring;
}

/* =============== 用户可在 ISR/定时器/主循环调用的函数 =============== */

// 在 RX 中断/回调里投递一段数据（不可阻塞）；返回实际写入字节数（可能 < len）
size_t transport_bm_on_rx_bytes(const uint8_t* data, size_t len){
    if (!data || len == 0) return 0;
    return rx_ring_write(&g_bm, data, len);
}

// 在 “TX 就绪/空” 中断里调用，把环里数据尽可能吐给硬件
void transport_bm_on_tx_ready_isr(void){
    (void)tx_drain_hw(&g_bm);
}

// 若没有中断，或想用定时器/主循环推进收发，在此处拉取/下推
void transport_bm_poll(void){
    bm_ctx_t* t = &g_bm;

    // 可选：如果提供了 rx_try_read，就从外设拉取数据
    if (t->ops && t->ops->rx_try_read){
        uint8_t tmp[128];
        size_t n = t->ops->rx_try_read(t->hw, tmp, sizeof(tmp));
        if (n) (void)rx_ring_write(t, tmp, n);
    }

    // 推进 TX
    (void)tx_drain_hw(t);
}
