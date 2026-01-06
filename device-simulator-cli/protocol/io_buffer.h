#ifndef IO_BUFFER_H
#define IO_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RX_BUFFER_SIZE (65535)
#define TX_BUFFER_SIZE 65535
#define MAX_FRAME_SIZE 8192

// 环形队列结构
typedef struct {
    uint8_t  buf[RX_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
} RxBuffer_t;

typedef struct {
    uint8_t  buf[TX_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
} TxBuffer_t;

// 初始化
void initRxBuffer(RxBuffer_t* r);
void initTxBuffer(TxBuffer_t* t);

// 向 Rx 环形队列压入数据(原始字节流)，返回实际存入的字节数
uint16_t feedRxBuffer(RxBuffer_t* r, const uint8_t* data, uint16_t len);

// 从 RxBuffer 中解析出尽可能多的完整帧，并调用回调函数处理
// 回调函数原型： void onFrame(const uint8_t* frame, uint16_t frameLen)
void tryParseFramesFromRx(
    RxBuffer_t* r,
    void (*onFrame)(const uint8_t* frame, uint16_t frameLen)
);

// ---------------------- Tx 相关 ----------------------

// 向 TxBuffer 中添加一帧(完整帧)。返回 0=成功, -1=空间不足
int enqueueTxFrame(TxBuffer_t* t, const uint8_t* frame, uint16_t frameLen);

// 从 TxBuffer 中取出一帧(若有)，返回帧长度，0=无数据
// outFrame 缓冲区由调用者提供
uint16_t dequeueTxFrame(TxBuffer_t* t, uint8_t* outFrame, uint16_t maxLen);

#endif