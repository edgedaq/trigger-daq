// main.c - 设备模拟器主程序
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "transport.h"
#include "app.h"
#include "protocol/protocol.h"
#include "protocol/io_buffer.h"

// 平台相关
#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(x) Sleep(x)
    #define get_time_ms() GetTickCount()
#else
    #include <unistd.h>
    #include <sys/time.h>
    #define sleep_ms(x) usleep((x)*1000)
    
    static uint32_t get_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
#endif

// 全局变量
static volatile int g_running = 1;

// 信号处理
static void signal_handler(int sig) {
    printf("\n[MAIN] Received signal %d, shutting down...\n", sig);
    g_running = 0;
}

// 测试命令注入（用于测试模式）
static void inject_test_commands(void) {
    // 这个函数展示如何注入测试命令
    // 实际使用时，test_inject_command应该在transport_test.c中实现
    extern void test_inject_command(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t payload_len);
    
    printf("\n[TEST] Injecting test command sequence...\n");
    
    // 1. 先发GET_DEVICE_INFO
    sleep_ms(100);
    test_inject_command(0x03, 1, NULL, 0);
    
    // 2. 配置流
    sleep_ms(100);
    uint8_t config_payload[] = {
        0x02,  // 2个通道配置
        // Channel 0
        0x00,  // channel_id
        0x10, 0x27, 0x00, 0x00,  // 10000 Hz (小端)
        0x01,  // format int16
        // Channel 1  
        0x01,  // channel_id
        0x10, 0x27, 0x00, 0x00,  // 10000 Hz
        0x01   // format int16
    };
    test_inject_command(0x14, 2, config_payload, sizeof(config_payload));
    
    // 3. 设置触发模式
    sleep_ms(100);
    test_inject_command(0x11, 3, NULL, 0);
    
    // 4. 开始流
    sleep_ms(100);
    test_inject_command(0x12, 4, NULL, 0);
}

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   Device Simulator v2.1 (Protocol V6)\n");
    printf("========================================\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
#endif
    
    // 初始化随机数
    srand((unsigned int)time(NULL));
    
    // 初始化应用层
    app_init();
    printf("[MAIN] Application initialized\n");
    
    // 创建传输层（测试模式）
    transport_t* transport = transport_tcp_create();
    if (!transport) {
        printf("[MAIN] Failed to create transport\n");
        return 1;
    }
    
    // 初始化传输层
    const char* config = (argc > 1) ? argv[1] : "test_mode";
    if (transport->init(transport->impl_ctx, config) != 0) {
        printf("[MAIN] Failed to init transport\n");
        return 1;
    }
    
    // 设置应用层的传输接口
    app_set_transport(transport);
    
    // 等待连接
    printf("[MAIN] Waiting for connection...\n");
    transport->wait_connection(transport->impl_ctx);
    
    // 注入测试命令（仅测试模式）
    if (strcmp(config, "test_mode") == 0) {
        inject_test_commands();
    }
    
    printf("[MAIN] Entering main loop (Ctrl+C to exit)\n");
    printf("----------------------------------------\n");
    
    uint32_t loop_count = 0;
    uint32_t last_status_time = get_time_ms();
    
    // 主循环
    while (g_running) {
        uint32_t now = get_time_ms();

        // 1. 处理接收缓冲区
        app_process_rx_buffer();

        // 2. 应用层周期任务
        app_periodic_task(now);
        
        // 3. 处理发送缓冲区
        app_process_tx_buffer();
        
        // 4. 状态显示（每5秒）
        if (now - last_status_time >= 5000) {
            printf("[MAIN] Status: loop=%u, time=%u ms\n", loop_count, now);
            last_status_time = now;
        }
        
        // 5. 避免CPU占用过高
        sleep_ms(1);
        loop_count++;
    }
    
    printf("----------------------------------------\n");
    printf("[MAIN] Shutting down...\n");
    
    // 清理
    app_cleanup();
    transport->cleanup(transport->impl_ctx);
    
    printf("[MAIN] Exit\n");
    return 0;
}// main.c - 设备模拟器主程序