// transport.h - 传输层抽象接口
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>

// 传输层接口
typedef struct transport_s {
    // 初始化
    int (*init)(void* ctx, const char* config);
    
    // 等待连接（如果需要）
    int (*wait_connection)(void* ctx);
    
    // 接收数据（非阻塞），返回接收字节数，0表示无数据，-1表示错误
    int (*recv)(void* ctx, uint8_t* buf, int len);
    
    // 发送数据，返回发送字节数，-1表示错误
    int (*send)(void* ctx, const uint8_t* buf, int len);
    
    // 清理
    void (*cleanup)(void* ctx);
    
    // 实现上下文
    void* impl_ctx;
} transport_t;

// 创建测试传输（模拟）
transport_t* transport_test_create(void);

// 未来可以添加
transport_t* transport_tcp_create(void);
// transport_t* transport_usb_create(void);

#endif