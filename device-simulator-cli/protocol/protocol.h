#ifndef PROTOCOL_H
#define PROTOCOL_H
 
#include <stdint.h>
 
// 帧头和帧尾定义
#define FRAME_HEAD_0        0xAA
#define FRAME_HEAD_1        0x55
#define FRAME_TAIL_0        0x55
#define FRAME_TAIL_1        0xAA
 
// 允许的最大帧长度 (示例设为 1024，可按需调整)
#define MAX_FRAME_SIZE      8192
 
// CRC16 预留
uint16_t CRC16_Calc(const uint8_t* data, uint16_t length, uint16_t initVal);
 
// 打包帧
// 参数：
//   commandID: 命令码
//   seq: 序列号
//   payload: 负载指针
//   payloadLen: 负载长度
//   outBuf: 输出缓冲区
//   outBufLen: 输出缓冲区长度(传入), 返回实际帧字节数(传出)
// 返回值：0=成功，-1=输出缓冲区不够
int buildFrame(
    uint8_t commandID,
    uint8_t seq,
    const uint8_t* payload,
    uint16_t payloadLen,
    uint8_t* outBuf,
    uint16_t* outBufLen
);
 
// 解析帧
// 参数：
//   inBuf: 输入数据缓冲区
//   inLen: 输入数据长度
//   pCmd:  [输出]解析到的 CommandID
//   pSeq:  [输出]解析到的 Seq
//   pPayload: [输出]解析到的负载, 需调用方准备好足够空间
//   pPayloadLen: [输出]实际负载长度
// 返回值：0=成功，-1=帧头不对，-2=帧尾不对，-3=长度非法，-4=CRC错误
int parseFrame(
    const uint8_t* inBuf,
    uint16_t inLen,
    uint8_t* pCmd,
    uint8_t* pSeq,
    uint8_t* pPayload,
    uint16_t* pPayloadLen
);
 
#endif // PROTOCOL_H