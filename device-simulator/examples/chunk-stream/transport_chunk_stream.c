// transport_chunk_stream.c — 通用“块流”传输（静态内存 + 环形队列）
// 不使用 osMemoryPoolNew/osMessageQueueNew；无堆分配。
// 依赖：CMSIS-RTOS v2（仅使用线程/线程标志），临界区用关中断保护。

#include "transport_chunk_stream.h"
#include <string.h>

/* ===================== 编译期上限（按需调整） ===================== */
#ifndef CS_RX_CHUNK_MAX_BYTES
#define CS_RX_CHUNK_MAX_BYTES   256    // 单个 RX 块最大字节数
#endif
#ifndef CS_RX_POOL_COUNT
#define CS_RX_POOL_COUNT        32     // RX 块池容量（可同时挂起的块数）
#endif
#ifndef CS_RX_READY_Q_LEN
#define CS_RX_READY_Q_LEN       32     // RX 就绪队列长度（元素是块指针）
#endif

#ifndef CS_TX_CHUNK_MAX_BYTES
#define CS_TX_CHUNK_MAX_BYTES   256    // 单个 TX 块最大字节数（send 会按此切块）
#endif
#ifndef CS_TX_POOL_COUNT
#define CS_TX_POOL_COUNT        32     // TX 块池容量
#endif
#ifndef CS_TX_SEND_Q_LEN
#define CS_TX_SEND_Q_LEN        32     // TX 发送队列长度
#endif

#ifndef CS_TX_THREAD_STACK
#define CS_TX_THREAD_STACK      768    // TX 后台线程栈（字节）
#endif
#ifndef CS_TX_THREAD_PRIO
#define CS_TX_THREAD_PRIO       osPriorityNormal
#endif

/* ===================== 临界区（关中断，ISR/任务均可用） ===================== */
#if defined(__ARMCC_VERSION) || defined(__GNUC__) || defined(__ICCARM__)
  #include "cmsis_compiler.h"
  static inline uint32_t cs_enter_crit(void){ uint32_t pm = __get_PRIMASK(); __disable_irq(); return pm; }
  static inline void     cs_exit_crit(uint32_t pm){ if (!pm) __enable_irq(); }
#else
  // 保底方案：仅示意
  static inline uint32_t cs_enter_crit(void){ __disable_irq(); return 1; }
  static inline void     cs_exit_crit(uint32_t pm){ (void)pm; __enable_irq(); }
#endif

/* ===================== 简单静态块类型 ===================== */
typedef struct {
    uint16_t len, rpos;
    uint8_t  data[CS_RX_CHUNK_MAX_BYTES];
    uint8_t  in_use;
} cs_rx_blk_t;

typedef struct {
    uint16_t len;
    uint8_t  data[CS_TX_CHUNK_MAX_BYTES];
    uint8_t  in_use;
} cs_tx_blk_t;

/* ===================== 静态上下文（单实例；需要多实例可扩展） ===================== */
typedef struct {
    void* hw;
    const chunk_stream_ops_t* ops;

    // ---- RX 静态资源 ----
    cs_rx_blk_t  rx_pool[CS_RX_POOL_COUNT];
    uint16_t     rx_free_stack[CS_RX_POOL_COUNT]; // 空闲栈存索引
    uint16_t     rx_free_top;                     // 指向下一个可用槽位（元素个数）
    cs_rx_blk_t* rx_ready_q[CS_RX_READY_Q_LEN];   // 就绪队列：块指针
    uint16_t     rx_q_head, rx_q_tail;           // 环形队列指针
    cs_rx_blk_t* app_rx_cur;                     // 正在被应用消费的块

    // ---- TX 静态资源 ----
    cs_tx_blk_t  tx_pool[CS_TX_POOL_COUNT];
    uint16_t     tx_free_stack[CS_TX_POOL_COUNT];
    uint16_t     tx_free_top;
    cs_tx_blk_t* tx_send_q[CS_TX_SEND_Q_LEN];
    uint16_t     tx_q_head, tx_q_tail;

    // TX 后台线程（静态栈）
    osThreadId_t tx_thread_id;
#if defined(__RTX) || defined(RTX_OS) || defined(RTE_CMSIS_RTOS2_RTX5)
    // 若可用，也可提供 CB 内存；这里仅提供静态栈即可
#endif
    uint32_t     tx_thread_stack[(CS_TX_THREAD_STACK + sizeof(uint32_t) - 1)/sizeof(uint32_t)];

    // 线程标志用于唤醒 TX 线程
    uint32_t     tx_flag_ready;

    // 运行时可调的目标块尺寸（不超过编译期上限）
    uint16_t     rx_chunk_bytes;
    uint16_t     tx_chunk_bytes;
} cs_ctx_t;

static cs_ctx_t   g_cs;
static transport_t g_tp;

/* ===================== 静态池/队列 helpers ===================== */
// RX pool alloc/free
static cs_rx_blk_t* rx_alloc_blk(cs_ctx_t* t){
    cs_rx_blk_t* blk = NULL;
    uint32_t pm = cs_enter_crit();
    if (t->rx_free_top > 0){
        uint16_t idx = t->rx_free_stack[--t->rx_free_top];
        blk = &t->rx_pool[idx];
        blk->in_use = 1;
        blk->len = blk->rpos = 0;
    }
    cs_exit_crit(pm);
    return blk;
}
static void rx_free_blk(cs_ctx_t* t, cs_rx_blk_t* blk){
    uint32_t pm = cs_enter_crit();
    uint16_t idx = (uint16_t)(blk - t->rx_pool);
    blk->in_use = 0;
    if (t->rx_free_top < CS_RX_POOL_COUNT){
        t->rx_free_stack[t->rx_free_top++] = idx;
    }
    cs_exit_crit(pm);
}
// RX ready queue push/pop
static int rx_ready_push(cs_ctx_t* t, cs_rx_blk_t* blk){
    int ok = 0;
    uint32_t pm = cs_enter_crit();
    uint16_t next = (uint16_t)((t->rx_q_tail + 1) % CS_RX_READY_Q_LEN);
    if (next != t->rx_q_head){
        t->rx_ready_q[t->rx_q_tail] = blk;
        t->rx_q_tail = next;
        ok = 1;
    }
    cs_exit_crit(pm);
    return ok;
}
static cs_rx_blk_t* rx_ready_pop(cs_ctx_t* t){
    cs_rx_blk_t* blk = NULL;
    uint32_t pm = cs_enter_crit();
    if (t->rx_q_head != t->rx_q_tail){
        blk = t->rx_ready_q[t->rx_q_head];
        t->rx_q_head = (uint16_t)((t->rx_q_head + 1) % CS_RX_READY_Q_LEN);
    }
    cs_exit_crit(pm);
    return blk;
}

// TX pool alloc/free
static cs_tx_blk_t* tx_alloc_blk(cs_ctx_t* t){
    cs_tx_blk_t* blk = NULL;
    uint32_t pm = cs_enter_crit();
    if (t->tx_free_top > 0){
        uint16_t idx = t->tx_free_stack[--t->tx_free_top];
        blk = &t->tx_pool[idx];
        blk->in_use = 1;
        blk->len = 0;
    }
    cs_exit_crit(pm);
    return blk;
}
static void tx_free_blk(cs_ctx_t* t, cs_tx_blk_t* blk){
    uint32_t pm = cs_enter_crit();
    uint16_t idx = (uint16_t)(blk - t->tx_pool);
    blk->in_use = 0;
    if (t->tx_free_top < CS_TX_POOL_COUNT){
        t->tx_free_stack[t->tx_free_top++] = idx;
    }
    cs_exit_crit(pm);
}
// TX send queue push/pop
static int tx_send_push(cs_ctx_t* t, cs_tx_blk_t* blk){
    int ok = 0;
    uint32_t pm = cs_enter_crit();
    uint16_t next = (uint16_t)((t->tx_q_tail + 1) % CS_TX_SEND_Q_LEN);
    if (next != t->tx_q_head){
        t->tx_send_q[t->tx_q_tail] = blk;
        t->tx_q_tail = next;
        ok = 1;
    }
    cs_exit_crit(pm);
    return ok;
}
static cs_tx_blk_t* tx_send_pop(cs_ctx_t* t){
    cs_tx_blk_t* blk = NULL;
    uint32_t pm = cs_enter_crit();
    if (t->tx_q_head != t->tx_q_tail){
        blk = t->tx_send_q[t->tx_q_head];
        t->tx_q_head = (uint16_t)((t->tx_q_head + 1) % CS_TX_SEND_Q_LEN);
    }
    cs_exit_crit(pm);
    return blk;
}

/* ===================== TX 后台线程 ===================== */
static void ChunkStreamTxThread(void* arg){
    cs_ctx_t* t = (cs_ctx_t*)arg;
    for(;;){
        // 先尝试尽可能把队列清空
        for(;;){
            cs_tx_blk_t* blk = tx_send_pop(t);
            if (!blk) break;
            (void)t->ops->tx_send_block(t->hw, blk->data, blk->len);
            tx_free_blk(t, blk);
        }
        // 没任务了，等待被唤醒
        (void)osThreadFlagsWait(t->tx_flag_ready, osFlagsWaitAny, osWaitForever);
    }
}

/* ===================== transport_t 接口实现 ===================== */
static int cs_init(void* ctx, const char* config){
    (void)config;
    cs_ctx_t* t = (cs_ctx_t*)ctx;

    // 初始化空闲栈
    for (uint16_t i = 0; i < CS_RX_POOL_COUNT; ++i) {
        t->rx_pool[i].in_use = 0;
        t->rx_free_stack[i]  = (uint16_t)(CS_RX_POOL_COUNT - 1 - i);
    }
    t->rx_free_top = CS_RX_POOL_COUNT;

    for (uint16_t i = 0; i < CS_TX_POOL_COUNT; ++i) {
        t->tx_pool[i].in_use = 0;
        t->tx_free_stack[i]  = (uint16_t)(CS_TX_POOL_COUNT - 1 - i);
    }
    t->tx_free_top = CS_TX_POOL_COUNT;

    t->rx_q_head = t->rx_q_tail = 0;
    t->tx_q_head = t->tx_q_tail = 0;

    t->rx_chunk_bytes = CS_RX_CHUNK_MAX_BYTES;
    t->tx_chunk_bytes = CS_TX_CHUNK_MAX_BYTES;

    t->tx_flag_ready = 0x00000001U; // 任意非零标志位

    // 启动 TX 线程（静态栈）
    const osThreadAttr_t attr = {
        .name = "cs_tx",
        .stack_mem  = t->tx_thread_stack,
        .stack_size = sizeof(t->tx_thread_stack),
        .priority   = CS_TX_THREAD_PRIO
        // .cb_mem / .cb_size 可按 RTX5 宏提供以完全静态化 CB（可选）
    };
    t->tx_thread_id = osThreadNew(ChunkStreamTxThread, t, &attr);
    return (t->tx_thread_id != NULL) ? 0 : -1;
}

static int cs_wait_connection(void* ctx){
    (void)ctx; // 无连接概念
    return 0;
}

static int cs_recv(void* ctx, uint8_t* buf, int len){
    cs_ctx_t* t = (cs_ctx_t*)ctx;
    if (!buf || len <= 0) return -1;

    int copied = 0;
    while (copied < len) {
        if (!t->app_rx_cur) {
            cs_rx_blk_t* nb = rx_ready_pop(t);
            if (!nb) break; // 当前无数据
            t->app_rx_cur = nb;
        }
        cs_rx_blk_t* b = t->app_rx_cur;
        uint16_t remain = (b->len > b->rpos) ? (uint16_t)(b->len - b->rpos) : 0;
        if (!remain) {
            rx_free_blk(t, b);
            t->app_rx_cur = NULL;
            continue;
        }
        int take = (remain < (uint16_t)(len - copied)) ? (int)remain : (len - copied);
        memcpy(buf + copied, &b->data[b->rpos], take);
        b->rpos += (uint16_t)take;
        copied  += take;
        if (b->rpos >= b->len) {
            rx_free_blk(t, b);
            t->app_rx_cur = NULL;
        }
    }
    return copied; // 可能为 0：无数据
}

static int cs_send(void* ctx, const uint8_t* buf, int len){
    cs_ctx_t* t = (cs_ctx_t*)ctx;
    if (!buf || len <= 0) return -1;

    int sent = 0;
    int enqueued_any = 0;
    while (sent < len) {
        int chunk = (len - sent > t->tx_chunk_bytes) ? t->tx_chunk_bytes : (len - sent);
        cs_tx_blk_t* blk = tx_alloc_blk(t);
        if (!blk) break; // 资源不足：停止入队
        blk->len = (uint16_t)chunk;
        memcpy(blk->data, buf + sent, chunk);

        if (!tx_send_push(t, blk)) {
            tx_free_blk(t, blk);
            break; // 队列满
        }
        sent += chunk;
        enqueued_any = 1;
    }

    if (enqueued_any && t->tx_thread_id) {
        (void)osThreadFlagsSet(t->tx_thread_id, t->tx_flag_ready);
    }
    // 语义：成功入队多少就返回多少；若一个也没入队，返回 -1
    return (sent > 0) ? sent : -1;
}

static void cs_cleanup(void* ctx){
    (void)ctx;
    // 如需退出线程/清队列，可在此补充
}

/* ===================== 对外 API ===================== */
transport_t* transport_chunk_stream_create_static(const chunk_stream_ops_t* ops, void* hw){
    memset(&g_cs, 0, sizeof(g_cs));
    g_cs.ops = ops;
    g_cs.hw  = hw;

    g_tp.init            = cs_init;
    g_tp.wait_connection = cs_wait_connection;
    g_tp.recv            = cs_recv;
    g_tp.send            = cs_send;
    g_tp.cleanup         = cs_cleanup;
    g_tp.impl_ctx        = &g_cs;
    return &g_tp;
}

void transport_chunk_stream_set_chunk_sizes(uint16_t rx_chunk, uint16_t tx_chunk){
    // 运行时可调，但不会超过编译期上限
    g_cs.rx_chunk_bytes = (rx_chunk && rx_chunk <= CS_RX_CHUNK_MAX_BYTES) ? rx_chunk : CS_RX_CHUNK_MAX_BYTES;
    g_cs.tx_chunk_bytes = (tx_chunk && tx_chunk <= CS_TX_CHUNK_MAX_BYTES) ? tx_chunk : CS_TX_CHUNK_MAX_BYTES;
}

/* 生产者投递 RX 数据块（ISR/任务均可；ISR 时 timeout_ms 必须为 0） */
int transport_chunk_stream_on_rx_chunk(const uint8_t* data, uint32_t len, uint32_t timeout_ms){
    (void)timeout_ms; // 静态实现中不阻塞，timeout 仅为兼容签名
    if (!data || !len) return 0;

    cs_ctx_t* t = &g_cs;
    uint32_t pushed = 0;

    while (pushed < len) {
        uint32_t chunk = len - pushed;
        if (chunk > t->rx_chunk_bytes) chunk = t->rx_chunk_bytes;

        cs_rx_blk_t* blk = rx_alloc_blk(t);
        if (!blk) break; // 块池暂满

        blk->len = (uint16_t)chunk;
        blk->rpos = 0;
        memcpy(blk->data, data + pushed, chunk);

        if (!rx_ready_push(t, blk)) {
            // 队列满：归还块并停止投递
            rx_free_blk(t, blk);
            break;
        }
        pushed += chunk;

        // 若在 ISR，建议每次仅投递一块，避免长时间占用
        if (osKernelGetState() == osKernelRunning && osKernelIsISR()) {
            break;
        }
    }
    return (int)pushed; // 可能 < len
}
