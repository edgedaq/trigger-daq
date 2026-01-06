#include "protocol.h"
#include <string.h> // for memcpy
 
/*
* CRC16 (MODBUS) 常见实现
* 多项式 0x8005 或 0xA001 都常见，不同库对左右移、反转、初始值等可能有差异
* 这里给出一个常用示例，可根据具体项目修改
*/
uint16_t CRC16_Calc(const uint8_t* data, uint16_t length, uint16_t initVal)
{
    uint16_t crc = initVal; // 通常初值可设 0xFFFF, 0x0000, 等
 
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001; // 常见多项式
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
 
int buildFrame(
    uint8_t commandID,
    uint8_t seq,
    const uint8_t* payload,
    uint16_t payloadLen,
    uint8_t* outBuf,
    uint16_t* outBufLen
)
{
    // 计算本帧的 length (不含 FrameHead 和 FrameTail)
    // length = CommandID(1) + Seq(1) + Payload(N) + CheckSum(2)
    uint16_t lengthField = 1 + 1 + payloadLen + 2;
    uint16_t totalFrameSize = 2 + 2 + lengthField + 2;
    // 即：帧头(2) + Length(2) + lengthField + 帧尾(2)
 
    if (totalFrameSize > *outBufLen) {
        // 缓冲区不够
        return -1;
    }
 
    // 开始组帧
    uint16_t offset = 0;
 
    // 帧头
    outBuf[offset++] = FRAME_HEAD_0;
    outBuf[offset++] = FRAME_HEAD_1;
 
    // Length: 小端存储
    outBuf[offset++] = (uint8_t)(lengthField & 0xFF);
    outBuf[offset++] = (uint8_t)((lengthField >> 8) & 0xFF);
 
    // CommandID
    outBuf[offset++] = commandID;
    // Seq
    outBuf[offset++] = seq;
 
    // Payload
    if (payloadLen > 0 && payload != NULL) {
        memcpy(&outBuf[offset], payload, payloadLen);
        offset += payloadLen;
    }
 
    // 计算 CRC16
    // 计算范围：从 CommandID 到 Payload 末尾
    uint16_t crc = CRC16_Calc(&outBuf[4], (uint16_t)(2 + payloadLen), 0xFFFF);
    //   &outBuf[4] -> 是 CommandID 的地址
    //   长度 = CommandID(1) + Seq(1) + Payload(payloadLen)
    //   initVal = 0xFFFF (常用 MODBUS 初始值)
 
    // 写入校验
    outBuf[offset++] = (uint8_t)(crc & 0xFF);
    outBuf[offset++] = (uint8_t)((crc >> 8) & 0xFF);
 
    // 帧尾
    outBuf[offset++] = FRAME_TAIL_0;
    outBuf[offset++] = FRAME_TAIL_1;
 
    *outBufLen = offset; // 实际帧长度
    return 0;
}
 
int parseFrame(
    const uint8_t* inBuf,
    uint16_t inLen,
    uint8_t* pCmd,
    uint8_t* pSeq,
    uint8_t* pPayload,
    uint16_t* pPayloadLen
)
{
    // 基本长度检查
    if (inLen < 8) {
        // 连最小的帧结构都放不下
        return -3;
    }
 
    // 检查帧头
    if ( (inBuf[0] != FRAME_HEAD_0) || (inBuf[1] != FRAME_HEAD_1) ) {
        return -1;
    }
 
    // 检查帧尾
    if ( (inBuf[inLen - 2] != FRAME_TAIL_0) || (inBuf[inLen - 1] != FRAME_TAIL_1) ) {
        return -2;
    }
 
    // 解析 Length
    //   inBuf[2] = Length 低字节
    //   inBuf[3] = Length 高字节
    uint16_t lengthField = (uint16_t)(inBuf[2]) | ((uint16_t)(inBuf[3]) << 8);
 
    // lengthField 应 = (CommandID(1) + Seq(1) + Payload + CheckSum(2))
    // 实际帧大小 = 2 + 2 + lengthField + 2 = 6 + lengthField
    uint16_t expectedFrameSize = 6 + lengthField;
    if (expectedFrameSize != inLen) {
        // 长度不一致
        return -3;
    }
 
    // 提取 CommandID 和 Seq
    uint16_t offset = 4;
    *pCmd = inBuf[offset++];
    *pSeq = inBuf[offset++];
 
    // 计算 payload 的大小
    // lengthField = 1(cmd) + 1(seq) + N(payload) + 2(checksum)
    uint16_t payloadLen = lengthField - 4;
    // 其中 4 = cmd(1) + seq(1) + checksum(2)
 
    // 读取 payload
    if (pPayload && payloadLen > 0) {
        memcpy(pPayload, &inBuf[offset], payloadLen);
    }
    if (pPayloadLen) {
        *pPayloadLen = payloadLen;
    }
    offset += payloadLen;
 
    // 读取校验
    uint16_t recvCRC = (uint16_t)(inBuf[offset])
                     | ((uint16_t)(inBuf[offset + 1]) << 8);
    
    // 计算校验
    uint16_t calcCRC = CRC16_Calc(&inBuf[4], (uint16_t)(2 + payloadLen), 0xFFFF);
    // 同 buildFrame 对齐：
    //   &inBuf[4] => CommandID处
    //   长度 = CommandID(1) + Seq(1) + payloadLen
 
    if (recvCRC != calcCRC) {
        return -4;
    }
 
    return 0; // 成功
}