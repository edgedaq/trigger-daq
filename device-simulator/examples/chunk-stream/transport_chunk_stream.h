// transport_chunk_stream.h — 通用“块流”传输（静态内存版）
#pragma once
#include <stdint.h>
#include "transport.h"
#include "cmsis_os2.h"

/* 你的外设发送函数（阻塞直到 len 字节全部发送或失败）
 * 返回 0 成功；非 0 失败（上层可稍后重试）。
 */
typedef struct chunk_stream_ops_s {
    int (*tx_send_block)(void* hw, const uint8_t* data, uint32_t len);
} chunk_stream_ops_t;

/* 创建一个基于“块流”的 transport_t（全静态，不分配堆内存）
 * - ops: 你的阻塞式发送函数
 * - hw : 你的外设句柄（任意指针）
 */
transport_t* transport_chunk_stream_create_static(const chunk_stream_ops_t* ops, void* hw);

/* 生产者（ISR 或任务）投递一段已接收数据到 RX：
 * - 在 ISR 中调用：timeout_ms 必须为 0（本实现不会阻塞）
 * - 若块池或队列满，会尽力投递前若干块，返回已投递的字节数（可能小于 len）
 * 返回：成功投递的字节数；失败返回 -1
 */
int transport_chunk_stream_on_rx_chunk(const uint8_t* data, uint32_t len, uint32_t timeout_ms);

/* 运行时调整块尺寸（不大于编译期上限才会生效） */
void transport_chunk_stream_set_chunk_sizes(uint16_t rx_chunk, uint16_t tx_chunk);
