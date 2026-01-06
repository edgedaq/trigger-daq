// transport_bm_stream.h — 裸机“块流”传输（中断/定时器驱动）
// 不依赖 RTOS；使用静态环形缓冲；与 transport_t 接口对齐。
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "transport.h"

/* === 硬件抽象（用户实现的非阻塞尝试写/读） ===========================
 * tx_try_write: 尝试把 data[0..len-1] 写入外设TX（非阻塞），返回实际写入字节数（可为0）。
 * rx_try_read : 可选；从外设RX拉取可用数据（非阻塞），返回实际读取字节数（可为0）。若使用中断投递，置为 NULL。
 */
typedef struct bm_stream_ops_s {
    size_t (*tx_try_write)(void* hw, const uint8_t* data, size_t len);
    size_t (*rx_try_read)(void* hw, uint8_t* buf, size_t max); // 可为 NULL
} bm_stream_ops_t;

/* 创建裸机块流 transport（全静态，无堆） */
transport_t* transport_bm_stream_create(const bm_stream_ops_t* ops, void* hw);

/* === 供外设回调/ISR调用的钩子 =========================================
 * 1) 在 RX 中断/回调里把新到的一段数据投递进来（不可阻塞）。
 *    返回成功写入环形缓冲的字节数（可能小于 len）。
 */
size_t transport_bm_on_rx_bytes(const uint8_t* data, size_t len);

/* 2) 在 TX 就绪中断里调用：把 TX 环形缓冲里的数据尽可能送给硬件。 */
void transport_bm_on_tx_ready_isr(void);

/* 3) 轮询/定时器 驱动：若没有TX中断或还想靠定时器推进收发，在 SysTick/定时器ISR 或主循环中调用。 */
void transport_bm_poll(void);

/* 运行期参数（不大于编译期上限才会生效） */
void transport_bm_set_buffer_sizes(uint16_t rx_ring, uint16_t tx_ring);
