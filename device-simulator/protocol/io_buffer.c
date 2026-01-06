#include "io_buffer.h"
#include <string.h>

// ========== RxBuffer ==========

void initRxBuffer(RxBuffer_t* r)
{
    r->head = 0;
    r->tail = 0;
}

static uint16_t rxBufferFreeSpace(const RxBuffer_t* r)
{
    // 典型环形队列剩余空间计算
    // (tail + size - head - 1) % size
    // 这里 size = RX_BUFFER_SIZE
    return (uint16_t)((r->head + RX_BUFFER_SIZE - r->tail - 1) % RX_BUFFER_SIZE);
}

uint16_t feedRxBuffer(RxBuffer_t* r, const uint8_t* data, uint16_t len)
{
    uint16_t freeSpace = rxBufferFreeSpace(r);
    uint16_t toWrite = (len <= freeSpace) ? len : freeSpace;

    // 一次写入可能需要两段拷贝(若环形队列“折返”)
    for (uint16_t i = 0; i < toWrite; i++) {
        r->buf[r->tail] = data[i];
        r->tail = (r->tail + 1) % RX_BUFFER_SIZE;
    }
    return toWrite;
}

// 根据前面 protocol.c 的最小帧长度，定义 MIN_FRAME_LEN=8
#define MIN_FRAME_LEN 8

static bool tryExtractOneFrame(RxBuffer_t* r, uint8_t* outFrame, uint16_t* outFrameLen)
{
    // 思路：从 r->head 开始，寻找帧头(0xAA,0x55)，找到后再看能否凑齐一帧最小长度
    // 若凑齐一帧，拷贝到 outFrame 并返回 true，否则返回 false

    uint16_t available = (uint16_t)((r->tail + RX_BUFFER_SIZE - r->head) % RX_BUFFER_SIZE);
    if (available < MIN_FRAME_LEN) {
        return false; // 不够最小长度
    }

    // 用临时索引遍历
    uint16_t idx = r->head;
    while (available >= MIN_FRAME_LEN) {
        // 检查帧头
        uint8_t b0 = r->buf[idx];
        uint8_t b1 = r->buf[(idx+1) % RX_BUFFER_SIZE];
        if (b0 == 0xAA && b1 == 0x55) {
            // 可能是帧头，先至少需要 4 字节读到 length
            if (available < MIN_FRAME_LEN) {
                return false;
            }
            // 读取 length
            uint8_t l0 = r->buf[(idx+2) % RX_BUFFER_SIZE];
            uint8_t l1 = r->buf[(idx+3) % RX_BUFFER_SIZE];
            uint16_t lengthField = (uint16_t)l0 | ((uint16_t)l1 << 8);
            uint16_t frameSize = 2 + 2 + lengthField + 2; // = 6 + lengthField

            if (frameSize > MAX_FRAME_SIZE) {
                // 帧长度超过单帧最大值 -> 可能是垃圾数据，跳过此帧头
                idx = (idx + 1) % RX_BUFFER_SIZE;
                available--;
                continue;
            }
            if (frameSize > available) {
                // 数据还不够整帧
                return false;
            }

            // 拿到一个完整帧，拷贝
            for (uint16_t i = 0; i < frameSize; i++) {
                outFrame[i] = r->buf[(idx + i) % RX_BUFFER_SIZE];
            }
            *outFrameLen = frameSize;

            // head 前进 frameSize
            r->head = (uint16_t)((idx + frameSize) % RX_BUFFER_SIZE);
            return true;
        } else {
            // 不是帧头，继续往后找
            idx = (idx + 1) % RX_BUFFER_SIZE;
            available--;
        }
    }

    return false;
}

void tryParseFramesFromRx(
    RxBuffer_t* r,
    void (*onFrame)(const uint8_t* frame, uint16_t frameLen)
)
{
    uint8_t tempFrame[MAX_FRAME_SIZE];
    uint16_t frameLen;

    // 尝试不断提取帧头 -> 复制完整帧 -> 调用回调
    while (1) {
        bool got = tryExtractOneFrame(r, tempFrame, &frameLen);
        if (!got) {
            break;
        }
        // 找到完整帧，调用回调
        onFrame(tempFrame, frameLen);
    }
}

// ========== TxBuffer ==========

void initTxBuffer(TxBuffer_t* t)
{
    t->head = 0;
    t->tail = 0;
}

static uint16_t txBufferFreeSpace(const TxBuffer_t* t)
{
    return (uint16_t)((t->head + TX_BUFFER_SIZE - t->tail - 1) % TX_BUFFER_SIZE);
}

int enqueueTxFrame(TxBuffer_t* t, const uint8_t* frame, uint16_t frameLen)
{
    // 这里为了区分多帧，可以先写 2 字节表示本帧长度，再写帧内容
    // [frameLenLow][frameLenHigh][frameData...]

    // 需要 frameLen + 2 字节
    uint16_t needed = frameLen + 2;
    if (needed > txBufferFreeSpace(t)) {
        return -1; // 空间不足
    }

    // 写入 frameLen(小端)
    t->buf[t->tail] = (uint8_t)(frameLen & 0xFF);
    t->tail = (t->tail + 1) % TX_BUFFER_SIZE;
    t->buf[t->tail] = (uint8_t)(frameLen >> 8);
    t->tail = (t->tail + 1) % TX_BUFFER_SIZE;

    // 写入frame本身
    for (uint16_t i = 0; i < frameLen; i++) {
        t->buf[t->tail] = frame[i];
        t->tail = (t->tail + 1) % TX_BUFFER_SIZE;
    }
    return 0;
}

uint16_t dequeueTxFrame(TxBuffer_t* t, uint8_t* outFrame, uint16_t maxLen)
{
    // 如果没有足够的数据来读出 2 字节长度，说明无数据
    uint16_t available = (uint16_t)((t->tail + TX_BUFFER_SIZE - t->head) % TX_BUFFER_SIZE);
    if (available < 2) {
        return 0;
    }

    // 读出 frameLen
    uint8_t l0 = t->buf[t->head];
    t->head = (t->head + 1) % TX_BUFFER_SIZE;
    uint8_t l1 = t->buf[t->head];
    t->head = (t->head + 1) % TX_BUFFER_SIZE;
    uint16_t frameLen = (uint16_t)l0 | ((uint16_t)l1 << 8);

    // 如果剩余可读数据 < frameLen，说明数据不完整 -> 回退指针(简单处理)
    available -= 2;
    if (frameLen > available) {
        // 回退指针
        t->head = (uint16_t)( (t->head + TX_BUFFER_SIZE - 2) % TX_BUFFER_SIZE );
        return 0;
    }

    // 如果 caller 提供的 outFrame 不够存放该帧，则丢弃或只读取部分(看需求)
    uint16_t copyLen = (frameLen <= maxLen) ? frameLen : maxLen;

    for (uint16_t i = 0; i < copyLen; i++) {
        outFrame[i] = t->buf[t->head];
        t->head = (t->head + 1) % TX_BUFFER_SIZE;
    }
    // 如果 frameLen > copyLen，剩余的字节也需要在环形队列里跳过
    for (uint16_t i = copyLen; i < frameLen; i++) {
        t->head = (t->head + 1) % TX_BUFFER_SIZE;
    }

    return frameLen;
}