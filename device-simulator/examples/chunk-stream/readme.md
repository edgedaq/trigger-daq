# examples/chunk-stream（静态内存版）README

这个示例提供一个**与外设无关**的“**块传输（chunk-stream）**”传输层实现，用于在 **CMSIS-RTOS v2**（例如 FreeRTOS 的 CMSIS 适配层）环境中，把“**驱动一次收到/要发送的一段数据**”对接到你项目的 `transport_t` 接口。示例采用**全静态内存**（不分配堆内存、不用 RTOS 的动态消息队列/内存池），便于在裸机/MCU/受限系统中使用。

> 适配对象举例：UART、SPI、I2S、USB CDC、专有链路、网口 MAC 驱动……只要你的驱动能够在**回调/中断**里告诉你“我收到**一段**数据”，或能以**阻塞方式**发出**一段**数据，就可以把这套示例接进去。

---

## 文件清单

放入你仓库的 `examples/` 目录（或任意你喜欢的位置）：

```
examples/
└─ chunk-stream/
   ├─ transport_chunk_stream.h        # 头文件（对上层暴露的 API）
   ├─ transport_chunk_stream.c        # 源文件（全静态，环形队列+静态块池+后台发送线程）
   └─ README.md                       # 本说明
```

---

## 核心理念：什么是“块传输（chunk-stream）”？

* **块（chunk）**：一次**连续的**数据段（比如 120 字节、240 字节……），由驱动在 ISR/回调中“交付”，或由上层“需要发送”的一段。
* **RX 路径**：驱动在 ISR/回调中一拿到**一段**数据，就调用 `transport_chunk_stream_on_rx_chunk()` 投递该段到**静态就绪队列**；上层 `transport->recv()` **非阻塞**地从就绪队列取块并拷贝上去，再交给你的协议解析（`feedRxBuffer` / `tryParseFramesFromRx`）。
* **TX 路径**：上层 `transport->send()` 把待发数据切成**一段或多段**放入**静态发送队列**；**后台 TX 线程**醒来后逐块调用\*\*你提供的“阻塞式发送函数”\*\*发出去。

这种抽象把“一次一段”的驱动世界，优雅地映射为 `transport_t` 的语义：

* `recv()`：**>0** 有数据、**0** 暂无、**-1** 错误；
* `send()`：成功**入队**多少就返回多少（至少入队一段，否则 **-1**），后台线程负责实际发出。

---

## 快速开始（Quick Start）

### 0) 你需要准备

* 已有项目的 `transport.h` / `app.c` / `protocol/` 等（本示例与项目解耦，只实现一个新的传输层）。
* CMSIS-RTOS v2 可用（用于创建一个**后台 TX 线程**和设置**线程标志**）。

### 1) 把示例文件加入工程

把 `transport_chunk_stream.h/.c` 放到你的工程（或直接保留在 `examples/chunk-stream/`，在工程里加上包含路径），编译它。

### 2) 提供一个“阻塞式发送”函数（ops）

你需要实现**一**个函数，保证把 `len` 字节**全部发送**（或返回失败）。例如用 HAL 的阻塞发送 API：

```c
#include "transport_chunk_stream.h"

static int my_tx_send_block(void* hw, const uint8_t* data, uint32_t len)
{
    // 示例：把 data[0..len-1] 以阻塞方式发出（按你的驱动替换）
    // 比如：HAL_UART_Transmit(huart, (uint8_t*)data, len, timeout_ms);
    // 或 SPI/I2S/USB CDC 的阻塞发送……
    (void)hw;
    int ok = 0;  // 0=成功，非0=失败
    return ok;
}

static const chunk_stream_ops_t s_ops = {
    .tx_send_block = my_tx_send_block
};
```

> 如果你的驱动只有“非阻塞+完成回调”，你也可以在 `my_tx_send_block()` 里启动发送并**等待一个事件**（信号量/通知/标志）后返回，从而**对上传递“阻塞发送完成”的语义**。

### 3) 用法（集成到你的主循环/App）

```c
// 创建并初始化 transport（静态内存、无堆分配）
transport_t* tp = transport_chunk_stream_create_static(&s_ops, /*hw*/ NULL);
tp->init(tp->impl_ctx, NULL);
app_set_transport(tp);
tp->wait_connection(tp->impl_ctx); // 无“连接”概念，会直接返回

for (;;) {
    // 1) 非阻塞收：从“就绪队列”取块拷贝到 raw
    uint8_t raw[512];
    int n = tp->recv(tp->impl_ctx, raw, sizeof(raw));
    if (n > 0) {
        // 2) 喂给协议环形缓冲并尝试解析完整帧
        uint16_t fed = feedRxBuffer(app_get_rx_buffer(), raw, (uint16_t)n);
        if (fed < (uint16_t)n) { /* 溢出处理（增大缓冲或放慢对端） */ }
        tryParseFramesFromRx(app_get_rx_buffer(), app_on_frame);
    }

    // 3) 应用周期任务（连续/触发模式的工作流、心跳等）
    app_periodic_task(osKernelGetTickCount());

    // 4) 应用发送缓冲：会调 tp->send() 把整帧入队；
    //    后台 TX 线程被唤醒后逐段“阻塞发送”
    app_process_tx_buffer();

    osDelay(1);
}
```

### 4) 在**驱动的回调/中断**里投递“收到的一段数据”

* ISR/回调**必须非阻塞**，所以 `timeout_ms` 传 `0`：

```c
void MyDriver_OnRxISR(const uint8_t* buf, uint16_t len)
{
    // 一次性投递 len 字节（内部会按块大小拆分入队）
    (void)transport_chunk_stream_on_rx_chunk(buf, len, 0);
}
```

* 如果你在一个**普通任务**里周期性取驱动收上来的大段数据，也可以投递（可选择带超时，但静态实现并不会阻塞）：

```c
size_t got = /* 从驱动拿到一段数据 */;
transport_chunk_stream_on_rx_chunk(buf, got, 0 /* or some ms */);
```

---

## 配置与参数

所有资源均为**静态数组**，通过以下宏配置**编译期上限**（见 `transport_chunk_stream.c` 顶部宏）：

```c
// 单块最大字节数（收/发）
#define CS_RX_CHUNK_MAX_BYTES   256
#define CS_TX_CHUNK_MAX_BYTES   256

// 块池容量（可同时挂起多少块）
#define CS_RX_POOL_COUNT        32
#define CS_TX_POOL_COUNT        32

// 环形队列长度（就绪队列/发送队列）
#define CS_RX_READY_Q_LEN       32
#define CS_TX_SEND_Q_LEN        32

// 后台 TX 线程栈与优先级
#define CS_TX_THREAD_STACK      768
#define CS_TX_THREAD_PRIO       osPriorityNormal
```

运行时也可**下调**（不能超过编译期上限）：

```c
// 让收块=128字节、发块=256字节（若超过编译期上限则取上限）
transport_chunk_stream_set_chunk_sizes(/*rx*/128, /*tx*/256);
```

> **背压策略**：当“块池”或“队列”用尽时：
>
> * `transport_chunk_stream_on_rx_chunk()` 只会投递**部分**数据（返回的字节数 < `len`），上层可**稍后重试**或**限流**；
> * `transport->send()` 只要成功入队至少一块就返回入队的总字节数；若一块都入不了，返回 `-1`，上层可稍后重试。

---

## 线程安全与实时性

* 所有“池/队列”操作使用**关中断临界区**保护，**ISR 与任务**并发安全。
* ISR 投递时建议**一次仅投递一块**（本实现满足：若在 ISR 环境，会在投递一块后提前退出），避免 ISR 占用过长时间。
* TX 后台线程使用**线程标志**唤醒，不忙轮询，功耗与占用较低。

---

## 数据流示意

### RX（收）路径

```
驱动（ISR/回调）       transport_chunk_stream_on_rx_chunk()
       │                     │
       │  buf,len            │  把大段数据按 rx_chunk_bytes 切片
       ▼                     ▼
   [ISR上下文]    ┌─> [静态 RX 块池] 分配块
                  └─> [静态就绪队列] 入队（环形）
                                    │
                                    ▼
                          transport->recv()（非阻塞）
                              │  从就绪队列取块 → 拷贝到 raw
                              ▼
                      feedRxBuffer → tryParseFramesFromRx → app_on_frame
```

### TX（发）路径

```
app_process_tx_buffer()         transport->send()
         │                             │
         └─“整帧字节流”→  按 tx_chunk_bytes 切块 → [静态 TX 块池] 分配块
                                           │
                                           └→ [静态发送队列] 入队（环形）
                                                       │
                                                       ▼
                                          [TX 后台线程] 唤醒
                                                       │
                                                       └─> ops->tx_send_block() 阻塞发送该块
                                                            （直到整块发完或失败）
```

---

## 与 `transport_t` 的契合点

* **统一的非阻塞收**：
  `recv()`：**>0** 有数据；**0** 无数据；**-1** 参数错。
  （就绪队列为空时立即返回 0，不会阻塞你的主循环）

* **“整帧发送”的上层契约**：
  你的 `app_process_tx_buffer()` 通常希望“**一帧一次发完**”。本示例的 `send()` 会把帧**切片入队**，TX 线程会**连续**逐块调用你的**阻塞发送**函数，实践里对对端而言仍表现为**连续输出**。
  若你必须“系统调用层也一次性 write 完整帧”，可在 ops 层自行把帧拼成单次阻塞发送（把 `tx_chunk_bytes` 设得**足够大**以容纳一帧，注意内存与 DMA 限制）。

---

## 可选扩展

* **多实例**：当前示例为**单实例**（`g_cs`）。如需多实例：

  * 把上下文改为**动态/静态多份**结构体，通过 `transport_chunk_stream_create_static()` 返回不同实例的 `transport_t`；
  * 同时把 `transport_chunk_stream_on_rx_chunk()` 改为**带实例句柄**的版本，或构造一个“当前实例注册表”。
* **零拷贝**：驱动可直接提供“只读块指针+长度”到队列，应用消费完再归还以避免 `memcpy`。需要你管理好**覆盖/生命周期**（避免 DMA/驱动覆盖还未消费的数据）。
* **DMA/HW 半/全中断**：如果你的驱动是 DMA 双半区（Half/Full），可在半/全回调里各调用一次 `on_rx_chunk(half_ptr, half_len, 0)`，等效于“每半区一块”，与本示例完全兼容。

---

## 常见问题（FAQ）

* **为什么 `send()` 不是阻塞到底？**
  按 `transport_t` 语义，`send()` 成功**入队**就返回字节数，后台线程负责真正阻塞发送，这样能**降低上层任务的阻塞时间**，更贴合“事件驱动 + 环形缓冲”的风格。

* **收不到数据 / 数据断断续续**
  检查：

  1. 驱动回调/ISR 是否确实在**拿到数据时**调用了 `on_rx_chunk()`；
  2. `CS_RX_POOL_COUNT/CS_RX_READY_Q_LEN` 是否**过小**导致频繁丢块；
  3. 上层是否**及时调用** `recv()` 并把数据喂入协议解析。

* **发送失败返回 -1**
  通常是**发送队列或块池已满**；扩大 `CS_TX_POOL_COUNT/CS_TX_SEND_Q_LEN`，或让上层**稍后重试**、降低发送速率。

* **CPU 占用高**
  可把主循环 `osDelay(1)` 稍增大；或在 `recv()` 返回 0 时再休眠；TX 线程已经用“线程标志”唤醒，不会忙轮询。

---

## 许可

按照你项目的主仓库许可发布（建议 MIT/BSD/Apache-2.0 之一）。如果需要，我可以附一份标准许可文本。
