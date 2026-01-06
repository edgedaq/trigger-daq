// transport_test.c - 测试传输层（模拟收发）
#include "transport.h"
#include "protocol/protocol.h"  // 使用buildFrame
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST_BUF_SIZE 4096

// 测试传输上下文
typedef struct {
    // 模拟接收缓冲区（模拟从PC来的数据）
    uint8_t rx_queue[TEST_BUF_SIZE];
    int rx_head;
    int rx_tail;
    
    // 连接状态
    int connected;
    
    // 测试计数器
    int recv_calls;
    int send_calls;
} test_context_t;

// 静态实例
static test_context_t g_test_ctx;

// 向测试缓冲区注入数据（模拟PC发送命令）
void test_inject_data(const uint8_t* data, int len) {
    test_context_t* ctx = &g_test_ctx;
    for (int i = 0; i < len; i++) {
        int next = (ctx->rx_tail + 1) % TEST_BUF_SIZE;
        if (next != ctx->rx_head) {
            ctx->rx_queue[ctx->rx_tail] = data[i];
            ctx->rx_tail = next;
        }
    }
    printf("[TEST] Injected %d bytes into rx queue\n", len);
}

// 初始化
static int test_init(void* ctx, const char* config) {
    test_context_t* t = (test_context_t*)ctx;
    memset(t, 0, sizeof(test_context_t));
    printf("[TEST_TRANSPORT] Init with config: %s\n", config ? config : "default");
    return 0;
}

// 等待连接
static int test_wait_connection(void* ctx) {
    test_context_t* t = (test_context_t*)ctx;
    if (!t->connected) {
        t->connected = 1;
        printf("[TEST_TRANSPORT] Simulated connection established\n");
        
        // 模拟PC发送PING命令 - 使用buildFrame正确封装
        uint8_t ping_frame[64];
        uint16_t frame_len = sizeof(ping_frame);
        
        int ret = buildFrame(0x01, 0, NULL, 0, ping_frame, &frame_len);
        if (ret == 0) {
            test_inject_data(ping_frame, frame_len);
            printf("[TEST] Injected PING frame: ");
            for (int i = 0; i < 10 && i < frame_len; i++) {
                printf("%02X ", ping_frame[i]);
            }
            printf("... (total %d bytes)\n", frame_len);
        }
    }
    return 0;
}

// 接收数据
static int test_recv(void* ctx, uint8_t* buf, int len) {
    test_context_t* t = (test_context_t*)ctx;
    int count = 0;
    
    while (count < len && t->rx_head != t->rx_tail) {
        buf[count++] = t->rx_queue[t->rx_head];
        t->rx_head = (t->rx_head + 1) % TEST_BUF_SIZE;
    }
    
    if (count > 0) {
        t->recv_calls++;
        printf("[TEST_TRANSPORT] Recv %d bytes (call #%d)\n", count, t->recv_calls);
    }
    
    return count;
}

// 发送数据
static int test_send(void* ctx, const uint8_t* buf, int len) {
    test_context_t* t = (test_context_t*)ctx;
    t->send_calls++;
    
    printf("[TEST_TRANSPORT] Send %d bytes (call #%d): ", len, t->send_calls);
    for (int i = 0; i < len && i < 16; i++) {
        printf("%02X ", buf[i]);
    }
    if (len > 16) printf("...");
    printf("\n");
    
    return len;
}

// 清理
static void test_cleanup(void* ctx) {
    test_context_t* t = (test_context_t*)ctx;
    printf("[TEST_TRANSPORT] Cleanup (recv_calls=%d, send_calls=%d)\n", 
           t->recv_calls, t->send_calls);
    memset(t, 0, sizeof(test_context_t));
}

// 创建测试传输
transport_t* transport_test_create(void) {
    static transport_t transport = {
        .init = test_init,
        .wait_connection = test_wait_connection,
        .recv = test_recv,
        .send = test_send,
        .cleanup = test_cleanup,
        .impl_ctx = &g_test_ctx
    };
    return &transport;
}

// 测试辅助函数：生成测试命令
void test_inject_command(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t payload_len) {
    printf("[TEST] Injecting command 0x%02X, seq=%d\n", cmd, seq);
    
    // 使用protocol.h的buildFrame正确封装
    uint8_t frame[512];
    uint16_t frame_len = sizeof(frame);
    
    int ret = buildFrame(cmd, seq, payload, payload_len, frame, &frame_len);
    if (ret == 0) {
        test_inject_data(frame, frame_len);
        printf("[TEST] Injected frame: ");
        for (int i = 0; i < 10 && i < frame_len; i++) {
            printf("%02X ", frame[i]);
        }
        if (frame_len > 10) printf("... ");
        printf("(total %d bytes)\n", frame_len);
    } else {
        printf("[TEST] Failed to build frame: %d\n", ret);
    }
}