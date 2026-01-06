# Device Simulator (Protocol V6)

A minimal, portable **device simulator** that speaks a framed binary protocol (V6) over pluggable transports.

* **TCP server** (`transport_tcp.c`) — non-blocking recv/send, listens for a client (e.g., your PC app).
* **Test transport** (`transport_test.c`) — in-memory loopback to inject test frames without sockets.
* **(Optional) STM32 HAL UART** (`transport_uart.c`) — polling, non-blocking `recv`, blocking `send`.

`protocol/` and `io_buffer.*` are **stable** and shouldn’t be modified.

---

## Repo layout

```
.
├── main.c                     # Entry: selects transport, drives the main loop
├── transport.h                # Transport interface (pluggable backend)
├── transport_tcp.c            # TCP server transport (non-blocking)
├── transport_test.c           # Test transport (inject frames, no sockets)
├── transport_uart.c           # (Optional) STM32 HAL UART transport
├── app.c / app.h              # Application layer (commands, streaming)
└── protocol/
    ├── protocol.c / .h        # Frame build/parse + CRC16 (stable)
    └── io_buffer.c / .h       # RX/TX ring buffers & frame extraction (stable)
```

---

## Build

### Requirements

* **Linux/macOS**: `gcc` or `clang`, `make`
* **Windows (MSYS2/MinGW)**: `gcc`, `make`
  （Makefile 已在 Windows 下自动链接 `-lws2_32`）

### Make

```bash
# Clean + build (outputs to ./build/)
make

# Debug build
make debug

# Clean
make clean
```

产物：`./build/device_simulator`（Windows 下为 `.exe`）。

---

## Run

提供两种模式：

* **TCP（默认）**：作为**服务端**监听并等待连接。
* **test**：使用内存测试传输，自动注入一段演示命令序列。

### 用法

```bash
# 默认：TCP 服务端，绑定 0.0.0.0:9001
./build/device_simulator

# 显式 TCP（可传地址/端口）
./build/device_simulator tcp
./build/device_simulator tcp 9001
./build/device_simulator tcp 0.0.0.0:9001
./build/device_simulator tcp tcp://0.0.0.0:9001

# 测试模式（无 socket，注入演示命令序列）
./build/device_simulator test
```

Windows（CMD/PowerShell）：

```bat
.\build\device_simulator.exe
.\build\device_simulator.exe tcp 0.0.0.0:9001
.\build\device_simulator.exe test
```

启动 TCP 模式后应看到：

```
[TCP] Listening on 0.0.0.0:9001 ...
[TCP] Client connected: <ip:port>
```

---

## How it works

主循环（`main.c`）每轮做：

1. `transport->recv(...)`（**非阻塞**）→ 将原始字节喂入 `RxBuffer`。
2. `tryParseFramesFromRx(...)` 从环形缓冲中抽取**完整帧**并回调 `app_on_frame(...)`。
3. `app_periodic_task(now)`（如连续/触发模式的数据发送节奏）。
4. `app_process_tx_buffer()` 出队待发帧并调用 `transport->send(...)` 发送。

**传输层约定**（各实现统一遵循）：

* `recv(...)` → **>0**：读到的字节数；**0**：当前无数据；**-1**：错误/断开。
* `send(...)` → 返回**实际发送字节**（应为整帧长度）；**-1**：错误。
* `wait_connection(...)` → 阻塞直到就绪（TCP 等客户端连接；test 模式模拟连接）。

---

## Protocol (V6) 简述

帧结构（小端）：

```
+-----------+---------+---------+--------+-----------+-----------+-----------+
| Head(2B)  | Len(2B) | Cmd(1B) | Seq(1B)| Payload N | CRC16(2B) | Tail(2B)  |
+-----------+---------+---------+--------+-----------+-----------+-----------+
   0xAA 55                           Len = 1+1+N+2                0x55 0xAA
```

* **CRC16**：MODBUS（poly `0xA001`，init `0xFFFF`），覆盖 `Cmd..Payload`。
* `MAX_FRAME_SIZE` = **8192**（见 `protocol.h`）。
* 工具函数：`buildFrame(...)` / `parseFrame(...)`（`protocol/protocol.c`）。

应用命令见 `app.c`（如 `PING/PONG`、`GET_DEVICE_INFO`、`SET_MODE_*`、`DATA_PACKET` 等）。

---

## TCP server transport

`transport_tcp.c` 特性：

* 默认绑定 `0.0.0.0:9001`（可通过 CLI 传入如 `"9001"` / `"0.0.0.0:9001"` / `"tcp://0.0.0.0:9001"`）。
* `wait_connection()` 阻塞等待客户端，随后将**客户端 socket 设为非阻塞**。
* `recv()` **非阻塞**，无数据立即返回 0。
* `send()` 在非阻塞套接字上循环直至发完整帧；若 `WOULDBLOCK`，短暂休眠后继续，确保与 `app_process_tx_buffer()` 的“整帧发送”假设一致。

> Windows 下需链接 **`-lws2_32`**（Makefile 已处理）。

---

## Test transport

`transport_test.c` 用于无网络场景调试：
`test_inject_command(cmd, seq, payload, len)` 会调用 `buildFrame(...)` 组帧并注入到 RX。

`test` 模式会演示注入：`GET_DEVICE_INFO` → `CONFIGURE_STREAM` → `SET_MODE_TRIGGER` → `START_STREAM`。

---

## STM32 HAL UART（可选）

`transport_uart.c` 为最小可用的串口实现：

* `recv` 使用 `HAL_UART_Receive(..., Timeout=0)` 轮询非阻塞读取。
* `send` 使用 `HAL_UART_Transmit(..., Timeout=T)` 阻塞直至发送完成。

用法示例：

```c
#include "transport.h"
extern UART_HandleTypeDef huart1;

transport_t* transport = transport_uart_create(&huart1);
transport->init(transport->impl_ctx, NULL);
app_set_transport(transport);
transport->wait_connection(transport->impl_ctx); // UART 无“连接”概念，会直接返回
```

> 后续可升级为 **DMA + IDLE** 环形缓冲，降低占用、提高吞吐。

---

## Add a new transport

实现一个 `transport_xyz.c`，满足：

```c
typedef struct transport_s {
  int  (*init)(void* ctx, const char* config);
  int  (*wait_connection)(void* ctx);
  int  (*recv)(void* ctx, uint8_t* buf, int len);
  int  (*send)(void* ctx, const uint8_t* buf, int len);
  void (*cleanup)(void* ctx);
  void* impl_ctx;
} transport_t;

transport_t* transport_xyz_create(void);
```

在 `main.c` 里按参数/宏选择即可。**保持非阻塞 `recv` 语义**。

---

## Troubleshooting

* **Windows 链接错误 `__imp_WSAXXX`**：确保链接 `-lws2_32`（Makefile 已处理）。
* **`bind()` 失败**：端口被占用，换端口或停止占用该端口的程序。
* **无法解析/无数据**：检查客户端是否按协议发送（`AA 55 ... 55 AA`、CRC 匹配）。
* **CPU 占用高**：主循环已 `sleep(1ms)`；可适当增加或基于事件优化。
* **帧被拆**：请保持 `send()` 对单帧“一次性发完”的语义；本仓库的 TCP 传输已处理。

---

## License
Apache-2.0
