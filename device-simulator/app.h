// app.h - 应用层接口
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "transport.h"
#include "protocol/io_buffer.h"

// 设备模式
typedef enum {
    MODE_CONTINUOUS = 0,
    MODE_TRIGGER = 1
} device_mode_t;

// 流状态
typedef enum {
    STATUS_STOPPED = 0,
    STATUS_RUNNING = 1
} stream_status_t;

// 初始化应用层
void app_init(void);

// 清理应用层
void app_cleanup(void);

// 设置传输层（应用层需要用它发送数据）
void app_set_transport(transport_t* transport);

// 帧回调（由tryParseFramesFromRx调用）
void app_on_frame(const uint8_t* frame, uint16_t frameLen);

// 周期性任务（主循环调用）
void app_periodic_task(uint32_t current_time_ms);

// 处理接收缓冲区（由主循环调用）
void app_process_rx_buffer(void);

// 处理发送缓冲区（主循环调用）
void app_process_tx_buffer(void);

// 获取命令名称（调试用）
const char* app_get_cmd_name(uint8_t cmd);

#endif