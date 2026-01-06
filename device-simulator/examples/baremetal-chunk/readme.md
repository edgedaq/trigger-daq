# Bare-metal chunk-stream（中断/定时器驱动、静态环形缓冲）

本示例实现一个**无 RTOS**、**全静态内存**的块传输层，抽象“驱动一次收/发的一段数据”为 `transport_t` 语义：

- `recv()` 非阻塞：有数据返回 >0；无数据返回 0；错误返回 -1  
- `send()` 一次性接纳整帧：空间不足返回 -1（上层稍后重试）；入队成功后尽可能立刻吐给硬件

> 适用于 UART / SPI / I2S / USB CDC / 专有链路等，只要你能在**中断里**拿到“到了一段数据”，或者能在**定时器/主循环**非阻塞拉取/写入。

---

## 文件

```

examples/
└─ baremetal-chunk/
├─ transport\_bm\_stream.h
├─ transport\_bm\_stream.c
└─ README.md

````

> 你的主工程 Makefile 已排除 `examples/`，这些文件默认**不会被编译入主工程**。按需复制到工程或单独建 demo。

---

## 快速使用

### 1) 提供硬件适配（非阻塞尝试写 / 可选尝试读）

```c
#include "transport_bm_stream.h"

// 尝试把 data[0..len-1] 写入外设TX（非阻塞），返回写了多少（0=硬件忙）
static size_t my_tx_try_write(void* hw, const uint8_t* data, size_t len){
    // 例：若 TX 寄存器空，就写入若干字节；若忙返回 0
    size_t wrote = 0;
    while (wrote < len) {
        if (!HW_TxReady(hw)) break;
        HW_WriteTxByte(hw, data[wrote++]);
    }
    return wrote;
}

// 可选：从外设RX非阻塞读（用于定时器/轮询方案；若使用 RX 中断投递可置 NULL）
static size_t my_rx_try_read(void* hw, uint8_t* buf, size_t max){
    size_t got = 0;
    while (got < max && HW_RxAvail(hw)) {
        buf[got++] = HW_ReadRxByte(hw);
    }
    return got;
}

static const bm_stream_ops_t s_ops = {
    .tx_try_write = my_tx_try_write,
    .rx_try_read  = my_rx_try_read  // 或 NULL（配合 RX 中断投递）
};
````

### 2) 在应用中创建 transport

```c
transport_t* tp = transport_bm_stream_create(&s_ops, /*hw*/ my_hw_handle);
transport_bm_set_buffer_sizes(/*rx*/1024, /*tx*/1024); // 可选，默认上限
tp->init(tp->impl_ctx, NULL);
app_set_transport(tp);
tp->wait_connection(tp->impl_ctx); // 裸机无连接概念，直接返回
```

### 3A) **中断驱动**（推荐）

* **RX 中断**：在你的外设 RX 中断/回调里把新到数据段投递进来（不可阻塞）：

```c
void HW_RxIRQ_Handler(void){
    uint8_t chunk[64];
    size_t n = HW_PopRxBurst(chunk, sizeof(chunk)); // 取本次中断收到的一段
    if (n) (void)transport_bm_on_rx_bytes(chunk, n);
}
```

* **TX 空闲中断**：在“TX 寄存器空 / TX FIFO 半空”等中断里推进发送：

```c
void HW_TxReadyIRQ_Handler(void){
    transport_bm_on_tx_ready_isr(); // 从内部 TX 环中吐数据到硬件
}
```

* **主循环**：

```c
for(;;){
    uint8_t raw[256];
    int n = tp->recv(tp->impl_ctx, raw, sizeof(raw));
    if (n > 0) {
        uint16_t fed = feedRxBuffer(app_get_rx_buffer(), raw, (uint16_t)n);
        if (fed < (uint16_t)n) { /* 溢出处理 */ }
        tryParseFramesFromRx(app_get_rx_buffer(), app_on_frame);
    }
    app_periodic_task(Bsp_GetTickMs());
    app_process_tx_buffer(); // 调 tp->send() 把整帧放入 TX 环；由中断继续吐
}
```

### 3B) **定时器/轮询驱动**（无中断也可用）

* 在 SysTick/定时器 ISR 或主循环里周期调用：

```c
void SysTick_Handler(void){
    transport_bm_poll(); // 拉取RX、推进TX
}
```

* 如果你使用 `my_rx_try_read=NULL`，则 `transport_bm_poll()` 只会推进 TX；你仍可在外部用 `transport_bm_on_rx_bytes()` 投递数据（例如 DMA Half/Full 回调）。

---

## 行为说明

* **非阻塞收**：`recv()` 只拷贝目前环里可用的字节，没数据就立刻返回 0。
* **整帧入队**：`send()` 要求 TX 环有足够空间才能返回成功；否则 **-1**，上层稍后再试（保持和你现有 `app_process_tx_buffer()` 契约一致）。
* **立即吐给硬件**：`send()` 成功后会尝试调用 `tx_try_write()` 立刻写出一部分；剩余的依靠 **TX 就绪中断**或 **poll** 继续吐。

---

## 参数与内存

* 编译期上限（见 `transport_bm_stream.c` 顶部）：

  * `BM_RX_RING_MAX` / `BM_TX_RING_MAX`：环形缓冲大小上限（字节）
* 运行时可下调：

  * `transport_bm_set_buffer_sizes(rx, tx)`（自动调整为不超过上限的 2 的幂）

> 环大小建议 ≥ 最大“峰值到达的数据段”和“最大帧尺寸”，避免丢字节。

---

## 小贴士

* **DMA Half/Full 回调**：在每次半段/整段完成时调用 `transport_bm_on_rx_bytes(ptr, len)` 即可，无需逐字节。
* **零拷贝**：如你能保障生命周期，也可把 RX 环换为“指针+长度”的块队列以省一次 memcpy（本示例为通用起见用了字节环）。
* **多实例**：当前为单实例（`g_bm`）。多实例时把上下文改为结构体指针（保存到 `transport_t.impl_ctx`），并把 `on_rx_bytes/on_tx_ready_isr/poll` 改成带句柄的版本或维护实例注册表。

---

## 许可

按你的主仓库许可发布（建议 MIT/BSD/Apache-2.0）。
