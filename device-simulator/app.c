// app.c - 应用层实现
#include "app.h"
#include "protocol/protocol.h"
#include "protocol/io_buffer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// 配置
#define MAX_CHANNELS        8
#define SAMPLES_PER_PACKET  100
#define DATA_PAYLOAD_SIZE   2048
#define TX_FRAME_BUF_SIZE   8192
#define DATA_SEND_INTERVAL  10    // ms

// 协议命令定义
#define CMD_PING                    0x01
#define CMD_PONG                    0x81
#define CMD_GET_STATUS              0x02
#define CMD_STATUS_RESPONSE         0x82
#define CMD_GET_DEVICE_INFO         0x03
#define CMD_DEVICE_INFO_RESPONSE    0x83
#define CMD_SET_MODE_CONTINUOUS     0x10
#define CMD_SET_MODE_TRIGGER        0x11
#define CMD_START_STREAM            0x12
#define CMD_STOP_STREAM             0x13
#define CMD_CONFIGURE_STREAM        0x14
#define CMD_ACK                     0x90
#define CMD_NACK                    0x91
#define CMD_DATA_PACKET             0x40
#define CMD_EVENT_TRIGGERED         0x41
#define CMD_REQUEST_BUFFERED_DATA   0x42
#define CMD_BUFFER_TRANSFER_COMPLETE 0x4F
#define CMD_LOG_MESSAGE             0xE0

// 应用层上下文（静态分配）
typedef struct {
    // 设备状态
    device_mode_t mode;
    stream_status_t status;
    uint8_t seq_counter;  // 用于主动发送的序列号
    
    // 通道配置
    struct {
        uint8_t enabled;
        uint32_t sample_rate;
        uint8_t format;
    } channels[MAX_CHANNELS];
    uint8_t num_channels;
    
    // 触发模式状态
    struct {
        uint8_t armed;
        uint8_t occurred;
        uint8_t sending;
        uint32_t timestamp;
        int packets_to_send;
        int packets_sent;
        uint32_t next_trigger_time;
    } trigger;
    
    // 定时器
    uint32_t last_data_send_time;
    uint32_t start_time;
    
    // 静态缓冲区
    uint8_t tx_frame_buf[TX_FRAME_BUF_SIZE];
    uint8_t data_payload[DATA_PAYLOAD_SIZE];
    int16_t sample_data[MAX_CHANNELS][SAMPLES_PER_PACKET];
    
    // 接收和发送缓冲区
    RxBuffer_t rx_buffer;
    TxBuffer_t tx_buffer;
    
    // 传输层引用
    transport_t* transport;
} app_context_t;

// 全局应用上下文
static app_context_t g_app;

static uint8_t g_recv_raw_buf[4096];

// 前向声明
static void app_send_frame_with_seq(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t len);
static void app_send_frame(uint8_t cmd, const uint8_t* payload, uint16_t len);
static void app_handle_command(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t payload_len);
static void app_send_data_packet(uint32_t timestamp);
static void app_send_trigger_event(uint32_t timestamp);
static void app_send_trigger_data_packet(void);

// 初始化
void app_init(void) {
    memset(&g_app, 0, sizeof(g_app));
    
    g_app.mode = MODE_CONTINUOUS;
    g_app.status = STATUS_STOPPED;
    g_app.num_channels = 3;
    
    // 默认通道配置
    g_app.channels[0].enabled = 1;
    g_app.channels[0].sample_rate = 10000;
    g_app.channels[0].format = 0x01; // int16
    
    g_app.channels[1].enabled = 1;
    g_app.channels[1].sample_rate = 10000;
    g_app.channels[1].format = 0x01;
    
    // 默认通道配置
    g_app.channels[2].enabled = 1;
    g_app.channels[2].sample_rate = 10000;
    g_app.channels[2].format = 0x01; // int16
    // 初始化接收和发送缓冲区
    initRxBuffer(&g_app.rx_buffer);
    initTxBuffer(&g_app.tx_buffer);
    
    printf("[APP] Initialized (mode=CONTINUOUS, status=STOPPED)\n");
}


// 清理
void app_cleanup(void) {
    g_app.status = STATUS_STOPPED;
    printf("[APP] Cleanup complete\n");
}

// 设置传输层
void app_set_transport(transport_t* transport) {
    g_app.transport = transport;
}

// 帧回调
void app_on_frame(const uint8_t* frame, uint16_t frameLen) {
    uint8_t cmd = 0;
    uint8_t seq = 0;
    uint8_t payload[MAX_FRAME_SIZE];
    uint16_t payload_len = 0;
    
    // 解析帧
    int ret = parseFrame(frame, frameLen, &cmd, &seq, payload, &payload_len);
    if (ret == 0) {
        printf("[APP] RX: %s (0x%02X) seq=%d len=%d\n", 
               app_get_cmd_name(cmd), cmd, seq, payload_len);
        app_handle_command(cmd, seq, payload, payload_len);
    } else {
        printf("[APP] Frame parse error: %d\n", ret);
    }
}

// 处理命令
static void app_handle_command(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t payload_len) {
    switch (cmd) {
        case CMD_PING: {
            // 回复PONG - 使用请求的seq
            uint64_t device_id = 0x11223344AABBCCDDULL;
            app_send_frame_with_seq(CMD_PONG, seq, (uint8_t*)&device_id, sizeof(device_id));
            printf("[APP] Responded to PING with device ID\n");
            break;
        }
        
        case CMD_GET_STATUS: {
            // 回复状态 - 使用请求的seq
            uint8_t status_payload[8] = {0};
            status_payload[0] = g_app.mode;
            status_payload[1] = g_app.status;
            status_payload[2] = 0; // no error
            status_payload[3] = 0; // error code
            app_send_frame_with_seq(CMD_STATUS_RESPONSE, seq, status_payload, sizeof(status_payload));
            break;
        }
        
        case CMD_GET_DEVICE_INFO: {
            // 构建设备信息 - 使用请求的seq
            uint8_t info[256];
            uint16_t offset = 0;
            
            info[offset++] = 6; // protocol version
            *(uint16_t*)&info[offset] = 0x0201; // firmware v2.1
            offset += 2;
            info[offset++] = g_app.num_channels;
            
            // 添加通道信息
            for (int i = 0; i < g_app.num_channels; i++) {
                info[offset++] = i; // channel_id
                *(uint32_t*)&info[offset] = 100000; // max_sample_rate
                offset += 4;
                *(uint16_t*)&info[offset] = 0x03; // formats: int16 | int32
                offset += 2;
                
                const char* name = (i == 0) ? "Voltage" : "Current";
                uint8_t name_len = strlen(name);
                info[offset++] = name_len;
                memcpy(&info[offset], name, name_len);
                offset += name_len;
            }
            
            app_send_frame_with_seq(CMD_DEVICE_INFO_RESPONSE, seq, info, offset);
            break;
        }
        
        case CMD_SET_MODE_CONTINUOUS: {
            g_app.mode = MODE_CONTINUOUS;
            g_app.trigger.armed = 0;
            app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
            printf("[APP] Mode changed to CONTINUOUS\n");
            break;
        }
        
        case CMD_SET_MODE_TRIGGER: {
            g_app.mode = MODE_TRIGGER;
            g_app.trigger.armed = 1;
            g_app.trigger.occurred = 0;
            g_app.trigger.sending = 0;
            // 下次触发时间：当前时间 + 5-10秒
            g_app.trigger.next_trigger_time = g_app.start_time + 5000 + (rand() % 5000);
            app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
            printf("[APP] Mode changed to TRIGGER (next trigger in ~%d ms)\n",
                   g_app.trigger.next_trigger_time - g_app.start_time);
            break;
        }
        
        case CMD_START_STREAM: {
            g_app.status = STATUS_RUNNING;
            g_app.last_data_send_time = 0;
            app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
            printf("[APP] Stream STARTED\n");
            break;
        }
        
        case CMD_STOP_STREAM: {
            g_app.status = STATUS_STOPPED;
            g_app.trigger.sending = 0;
            app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
            printf("[APP] Stream STOPPED\n");
            break;
        }
        
        case CMD_CONFIGURE_STREAM: {
            if (payload_len < 1) {
                uint8_t err[2] = {0x01, 0x01}; // parameter error
                app_send_frame_with_seq(CMD_NACK, seq, err, 2);
                break;
            }
            
            uint8_t num_configs = payload[0];
            uint16_t offset = 1;
            
            for (int i = 0; i < num_configs && offset + 6 <= payload_len; i++) {
                uint8_t ch_id = payload[offset];
                uint32_t rate = *(uint32_t*)&payload[offset + 1];
                uint8_t fmt = payload[offset + 5];
                offset += 6;
                
                if (ch_id < g_app.num_channels) {
                    g_app.channels[ch_id].enabled = (rate > 0);
                    g_app.channels[ch_id].sample_rate = rate;
                    g_app.channels[ch_id].format = fmt;
                    printf("[APP] Channel %d: rate=%d fmt=0x%02X\n", ch_id, rate, fmt);
                }
            }
            
            app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
            break;
        }
        
        case CMD_REQUEST_BUFFERED_DATA: {
            if (g_app.mode != MODE_TRIGGER || !g_app.trigger.occurred) {
                uint8_t err[2] = {0x02, 0x02}; // status error
                app_send_frame_with_seq(CMD_NACK, seq, err, 2);
            } else {
                app_send_frame_with_seq(CMD_ACK, seq, NULL, 0);
                // 触发数据会在periodic_task中发送
            }
            break;
        }
        
        default: {
            printf("[APP] Unknown command: 0x%02X\n", cmd);
            uint8_t err[2] = {0x05, 0x00}; // command not supported
            app_send_frame_with_seq(CMD_NACK, seq, err, 2);
            break;
        }
    }
}

// 周期性任务
void app_periodic_task(uint32_t current_time_ms) {
    if (!g_app.start_time) {
        g_app.start_time = current_time_ms;
    }
    
    if (g_app.status != STATUS_RUNNING) {
        return;
    }
    
    // 触发模式处理
    if (g_app.mode == MODE_TRIGGER) {
        // 检查触发
        if (g_app.trigger.armed && !g_app.trigger.occurred && 
            current_time_ms >= g_app.trigger.next_trigger_time) {
            
            // 发送触发事件
            app_send_trigger_event(current_time_ms);
            g_app.trigger.occurred = 1;
            g_app.trigger.sending = 1;
            g_app.trigger.timestamp = current_time_ms;
            g_app.trigger.packets_to_send = 5 + (rand() % 6); // 5-10包
            g_app.trigger.packets_sent = 0;
            
            printf("[APP] TRIGGER EVENT! Will send %d packets\n", 
                   g_app.trigger.packets_to_send);
        }
        
        // 发送触发数据
        if (g_app.trigger.sending) {
            if (current_time_ms - g_app.last_data_send_time >= DATA_SEND_INTERVAL) {
                if (g_app.trigger.packets_sent < g_app.trigger.packets_to_send) {
                    app_send_trigger_data_packet();
                    g_app.trigger.packets_sent++;
                    g_app.last_data_send_time = current_time_ms;
                    
                    printf("[APP] Sent trigger packet %d/%d\n",
                           g_app.trigger.packets_sent, g_app.trigger.packets_to_send);
                } else {
                    // 发送完成信号
                    app_send_frame(CMD_BUFFER_TRANSFER_COMPLETE, NULL, 0);
                    printf("[APP] Trigger burst complete\n");
                    
                    // 重置状态
                    g_app.trigger.sending = 0;
                    g_app.trigger.occurred = 0;
                    g_app.trigger.next_trigger_time = current_time_ms + 10000 + (rand() % 5000);
                }
            }
        }
    }
    // 连续模式处理
    else if (g_app.mode == MODE_CONTINUOUS) {
        if (current_time_ms - g_app.last_data_send_time >= DATA_SEND_INTERVAL) {
            app_send_data_packet(current_time_ms);
            g_app.last_data_send_time = current_time_ms;
        }
    }
}

// 发送帧（使用指定的seq）
static void app_send_frame_with_seq(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t len) {
    uint16_t frame_len = TX_FRAME_BUF_SIZE;
    int ret = buildFrame(cmd, seq, payload, len, 
                        g_app.tx_frame_buf, &frame_len);
    
    if (ret == 0) {
        // 放入发送缓冲区
        if (enqueueTxFrame(&g_app.tx_buffer, g_app.tx_frame_buf, frame_len) == 0) {
            printf("[APP] TX queued: %s (0x%02X) seq=%d len=%d\n", 
                   app_get_cmd_name(cmd), cmd, seq, frame_len);
        } else {
            printf("[APP] TX buffer full, frame dropped: %s (0x%02X)\n", 
                   app_get_cmd_name(cmd), cmd);
        }
    } else {
        printf("[APP] Build frame failed: %d\n", ret);
    }
}

// 发送帧（使用自增seq，用于主动发送）
static void app_send_frame(uint8_t cmd, const uint8_t* payload, uint16_t len) {
    app_send_frame_with_seq(cmd, g_app.seq_counter++, payload, len);
}

// 处理接收缓冲区（由主循环调用）
void app_process_rx_buffer(void) {
    if (!g_app.transport) {
        return;
    }
    // 1. 接收数据
    int n = g_app.transport->recv(g_app.transport->impl_ctx, g_recv_raw_buf, sizeof(g_recv_raw_buf));
    if (n > 0) {
        // 喂给RxBuffer
        uint16_t fed = feedRxBuffer(&g_app.rx_buffer, g_recv_raw_buf, n);
        if (fed < n) {
            printf("[MAIN] Warning: RxBuffer overflow, lost %d bytes\n", n - fed);
        }
        
        // 尝试解析帧
        tryParseFramesFromRx(&g_app.rx_buffer, app_on_frame);
    } else if (n < 0) {
        printf("[MAIN] Transport error, exiting\n");
    }    

}

// 处理发送缓冲区（由主循环调用）
void app_process_tx_buffer(void) {
    if (!g_app.transport) {
        return;
    }
    
    uint8_t frame[MAX_FRAME_SIZE];
    uint16_t frame_len;
    int send_count = 0;
    
    // 循环处理所有待发送帧
    while ((frame_len = dequeueTxFrame(&g_app.tx_buffer, frame, sizeof(frame))) > 0) {
        int sent = g_app.transport->send(g_app.transport->impl_ctx, frame, frame_len);
        if (sent != frame_len) {
            printf("[APP] Send failed: sent %d of %d bytes\n", sent, frame_len);
            // 发送失败，可以选择重新入队或丢弃
            break;
        }
        send_count++;
    }
    
    if (send_count > 0) {
        // 只在有发送时打印，避免过多日志
        // printf("[APP] Sent %d frames\n", send_count);
    }
}

// 发送数据包
static void app_send_data_packet(uint32_t timestamp) {
    uint16_t offset = 0;
    uint8_t* payload = g_app.data_payload;
    
    // 时间戳
    *(uint32_t*)(payload + offset) = timestamp;
    offset += 4;
    
    // 通道掩码
    uint16_t ch_mask = 0;
    for (int i = 0; i < g_app.num_channels; i++) {
        if (g_app.channels[i].enabled) {
            ch_mask |= (1 << i);
        }
    }
    *(uint16_t*)(payload + offset) = ch_mask;
    offset += 2;
    
    // 样本数
    *(uint16_t*)(payload + offset) = SAMPLES_PER_PACKET;
    offset += 2;
    
    // 生成数据
    for (int ch = 0; ch < g_app.num_channels; ch++) {
        if (ch_mask & (1 << ch)) {
            for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
                // 简单的正弦波 + 噪声
                float t = (timestamp + i) * 0.001;
                int16_t value = (int16_t)(1000 * sin(2 * 3.14159 * 50 * t) + (rand() % 100 - 50));
                *(int16_t*)(payload + offset) = value;
                offset += 2;
            }
        }
    }
    
    app_send_frame(CMD_DATA_PACKET, payload, offset);
}

// 发送触发事件
static void app_send_trigger_event(uint32_t timestamp) {
    uint8_t payload[14];
    *(uint32_t*)&payload[0] = timestamp;
    *(uint16_t*)&payload[4] = 0; // channel 0
    *(uint32_t*)&payload[6] = 1000; // pre samples
    *(uint32_t*)&payload[10] = 1000; // post samples
    
    app_send_frame(CMD_EVENT_TRIGGERED, payload, 14);
}

// 发送触发数据包
static void app_send_trigger_data_packet(void) {
    uint32_t timestamp = g_app.trigger.timestamp + g_app.trigger.packets_sent * DATA_SEND_INTERVAL;
    app_send_data_packet(timestamp);
}

// 获取命令名称
const char* app_get_cmd_name(uint8_t cmd) {
    switch (cmd) {
        case CMD_PING: return "PING";
        case CMD_PONG: return "PONG";
        case CMD_GET_STATUS: return "GET_STATUS";
        case CMD_STATUS_RESPONSE: return "STATUS_RESPONSE";
        case CMD_GET_DEVICE_INFO: return "GET_DEVICE_INFO";
        case CMD_DEVICE_INFO_RESPONSE: return "DEVICE_INFO_RESPONSE";
        case CMD_SET_MODE_CONTINUOUS: return "SET_MODE_CONTINUOUS";
        case CMD_SET_MODE_TRIGGER: return "SET_MODE_TRIGGER";
        case CMD_START_STREAM: return "START_STREAM";
        case CMD_STOP_STREAM: return "STOP_STREAM";
        case CMD_CONFIGURE_STREAM: return "CONFIGURE_STREAM";
        case CMD_ACK: return "ACK";
        case CMD_NACK: return "NACK";
        case CMD_DATA_PACKET: return "DATA_PACKET";
        case CMD_EVENT_TRIGGERED: return "EVENT_TRIGGERED";
        case CMD_REQUEST_BUFFERED_DATA: return "REQUEST_BUFFERED_DATA";
        case CMD_BUFFER_TRANSFER_COMPLETE: return "BUFFER_TRANSFER_COMPLETE";
        case CMD_LOG_MESSAGE: return "LOG_MESSAGE";
        default: return "UNKNOWN";
    }
}