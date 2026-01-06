# 常见问题 (FAQ)

[English](FAQ_en.md) | 简体中文

本文档回答有关通用数据采集系统的常见问题。

## 目录

- [安装和配置](#安装和配置)
- [连接和通信](#连接和通信)
- [数据采集](#数据采集)
- [触发模式](#触发模式)
- [性能相关](#性能相关)
- [故障排除](#故障排除)
- [开发相关](#开发相关)

---

## 安装和配置

### Q: 系统有哪些依赖要求？

**A:** 依赖分为两部分：

**data-gateway (Rust):**
- Rust 1.70或更高版本
- Cargo包管理器
- 操作系统：Linux、macOS、Windows

**设备模拟器 (C):**
- GCC或Clang编译器
- Make工具
- 标准C库

详细安装步骤请参考[CONTRIBUTING.md](../CONTRIBUTING.md#开发环境设置)。

### Q: 如何配置环境变量？

**A:** 复制`.env.example`到`.env`，然后修改配置：

```bash
cd data-gateway
cp .env.example .env
nano .env  # 或使用其他编辑器
```

主要配置项：
```bash
# 连接方式：serial（生产）或 socket（开发）
DEVICE_TYPE=socket

# Socket地址（开发模式）
SOCKET_ADDRESS=127.0.0.1:9001

# 串口设置（生产模式）
SERIAL_PORT=COM7          # Windows
# SERIAL_PORT=/dev/ttyUSB0  # Linux/macOS

# 服务端口
WEB_PORT=8080
WS_PORT=8081

# 数据存储
DATA_DIR=./data
TRIGGER_CACHE_SIZE=10
```

### Q: 如何选择连接方式（Serial vs Socket）？

**A:**

| 场景 | 推荐方式 | 配置 |
|------|---------|------|
| 开发和测试 | TCP Socket | `DEVICE_TYPE=socket` |
| 真实硬件部署 | USB-CDC Serial | `DEVICE_TYPE=serial` |
| 远程设备 | TCP Socket | 配置远程IP地址 |

**开发测试**使用Socket更方便，可以在同一台机器上运行模拟器和data-gateway。

**生产部署**使用Serial直连MCU设备，更稳定可靠。

---

## 连接和通信

### Q: data-gateway无法连接到设备怎么办？

**A:** 按以下步骤排查：

**1. 检查设备是否运行：**
```bash
# Socket模式：确认模拟器已启动
cd device-simulator-cli
make run

# Serial模式：确认设备已连接
ls /dev/ttyUSB*  # Linux/macOS
# 或在设备管理器中查看 COM 口 (Windows)
```

**2. 检查配置：**
```bash
# 查看当前配置
cat data-gateway/.env

# Socket模式：确认地址和端口
SOCKET_ADDRESS=127.0.0.1:9001

# Serial模式：确认串口名称
SERIAL_PORT=/dev/ttyUSB0
```

**3. 检查权限（Linux/macOS）：**
```bash
# 添加用户到dialout组（Serial模式）
sudo usermod -a -G dialout $USER
# 需要重新登录
```

**4. 查看日志：**
```bash
# 启动时查看详细日志
RUST_LOG=debug cargo run
```

### Q: WebSocket连接频繁断开怎么办？

**A:**

**可能原因和解决方案：**

1. **网络不稳定**
   - 使用有线网络替代WiFi
   - 检查防火墙设置
   - 确认没有代理干扰

2. **数据量过大**
   - 降低采样率
   - 减少活动通道数
   - 实现客户端数据缓冲

3. **客户端实现问题**
   - 实现自动重连机制
   - 定期发送ping保持连接
   ```javascript
   // 推荐的重连逻辑
   function connectWebSocket() {
       const ws = new WebSocket('ws://127.0.0.1:8081');

       ws.onclose = () => {
           setTimeout(connectWebSocket, 3000);  // 3秒后重连
       };
   }
   ```

### Q: 如何验证设备连接状态？

**A:** 使用状态API：

```bash
# 查看系统状态
curl http://127.0.0.1:8080/api/control/status | jq

# 检查device_connected字段
{
  "device_connected": true,
  "connection_type": "socket",
  "packets_processed": 1234
}
```

或在测试界面上查看连接状态指示器。

---

## 数据采集

### Q: 支持的最大采样率是多少？

**A:**

| 模式 | 最大采样率 | 限制因素 |
|------|-----------|---------|
| Socket模式（模拟器） | ~100kHz/通道 | CPU和网络带宽 |
| Serial模式（MCU） | ~100kHz/通道 | MCU性能和USB带宽 |
| 多通道总带宽 | ~1MB/s | 传输层限制 |

**优化建议：**
- 单通道高速采集比多通道更可靠
- 使用int16格式代替float32节省带宽
- 考虑使用触发模式而非连续流

### Q: 如何配置多通道采集？

**A:** 使用配置API：

```bash
curl -X POST http://127.0.0.1:8080/api/control/configure \
  -H "Content-Type: application/json" \
  -d '{
    "channels": [
      {"channel_id": 0, "sample_rate": 10000, "format": 1},
      {"channel_id": 1, "sample_rate": 10000, "format": 1},
      {"channel_id": 2, "sample_rate": 1000, "format": 1}
    ]
  }'
```

或在测试界面的配置面板中设置。

### Q: 数据格式有哪些选项？

**A:**

| 格式代码 | 类型 | 字节数 | 范围 | 适用场景 |
|---------|------|-------|------|---------|
| 1 | int16 | 2 | -32768~32767 | 通用，节省带宽 |
| 2 | int32 | 4 | -2^31~2^31-1 | 高精度整数 |
| 4 | float32 | 4 | IEEE-754 | 浮点运算 |

**推荐：**
- 大多数情况使用**int16**，足够精度且节省50%带宽
- 需要浮点运算时使用**float32**

---

## 触发模式

### Q: 触发模式和连续模式有什么区别？

**A:**

| 特性 | 连续模式 | 触发模式 |
|------|---------|---------|
| 数据流 | 持续发送 | 事件触发时发送 |
| 适用场景 | 实时监控 | 异常事件捕获 |
| 数据管理 | 实时保存/丢弃 | 批次缓存，用户选择保存 |
| 内存使用 | 低 | 中等（缓存批次） |
| 数据完整性 | 连续 | 包含事件前后上下文 |

**何时使用触发模式：**
- 关注特定事件（如振动异常、冲击）
- 需要事件前后的完整数据
- 想在保存前预览数据质量
- 存储空间有限，只保存重要数据

### Q: 如何使用触发模式？

**A:** 完整流程：

**1. 设置触发模式：**
```bash
# 1. 切换到触发模式
curl -X POST http://127.0.0.1:8080/api/control/trigger_mode

# 2. 启动采集
curl -X POST http://127.0.0.1:8080/api/control/start
```

**2. 等待触发事件：**
- 设备检测到触发条件时自动采集数据
- 通过WebSocket接收实时通知

**3. 查看触发批次：**
```bash
# 获取批次列表
curl http://127.0.0.1:8080/api/trigger/list

# 预览特定批次
curl http://127.0.0.1:8080/api/trigger/preview/{burst_id}
```

**4. 保存批次数据：**
```bash
curl -X POST http://127.0.0.1:8080/api/trigger/save/{burst_id} \
  -H "Content-Type: application/json" \
  -d '{
    "dir": "experiments/test1",
    "filename": "impact_001",
    "format": "csv",
    "description": "冲击测试数据"
  }'
```

### Q: 触发批次最多可以缓存多少个？

**A:**

默认缓存**10个**批次，可通过环境变量调整：

```bash
# 在.env中配置
TRIGGER_CACHE_SIZE=20  # 增加到20个
```

**注意事项：**
- 每个批次约占1-5MB内存
- 缓存满时，最旧的批次会自动删除
- 及时保存重要的批次数据

### Q: 如何判断触发数据的质量？

**A:**

系统自动评估每个批次，提供质量状态：

```json
{
  "quality": "Good",  // 或 "Warning" / "Error"
  "quality_summary": {
    "overall_quality": {"status": "Good"},
    "voltage_range": [0.1, 3.2],
    "anomaly_count": 0,
    "channel_stats": [...]
  }
}
```

**质量评估指标：**
- ✅ **Good**: 数据正常，可以保存
- ⚠️ **Warning**: 存在轻微问题（接近饱和、轻微平坦）
- ❌ **Error**: 严重问题（超出范围、饱和、数据缺失）

**常见问题：**
- **饱和**: 信号超出ADC量程，调整增益或衰减
- **平坦**: 传感器无响应，检查连接
- **超范围**: 电压异常，检查电源和接地

---

## 性能相关

### Q: 系统性能指标是多少？

**A:**

**延迟：**
- 端到端延迟：< 50ms
- 触发检测延迟：< 1ms
- 批次处理时间：< 10ms
- WebSocket推送延迟：< 5ms

**吞吐量：**
- 最大数据速率：1MB/s
- 触发批次处理：每秒多个事件
- 并发WebSocket客户端：建议< 10个

**资源使用：**
- 内存：基础40-100MB，每批次增加1-5MB
- CPU：单核10-30%（正常负载）
- 磁盘：按需写入，CSV约为二进制的2-3倍

### Q: 如何优化系统性能？

**A:**

**1. 采样配置优化：**
```bash
# 只启用需要的通道
# 使用合适的采样率（不要过高）
# 选择int16格式而非float32
```

**2. 网络优化：**
```bash
# 使用有线连接
# 限制WebSocket客户端数量
# 考虑使用数据压缩（未来版本）
```

**3. 存储优化：**
```bash
# 使用SSD存储数据
# 定期清理旧文件
# 使用binary格式节省空间
```

**4. 系统配置：**
```bash
# 增加触发缓存（如果内存充足）
TRIGGER_CACHE_SIZE=20

# 调整日志级别
RUST_LOG=info  # 生产环境用info而非debug
```

### Q: 长时间运行会有内存泄漏吗？

**A:**

v2.0已修复了已知的内存泄漏问题。系统包含以下保护机制：

- 自动限制触发批次缓存数量
- 智能垃圾回收旧数据
- 内存使用监控和报告

**监控内存使用：**
```bash
# 查看系统状态
curl http://127.0.0.1:8080/api/control/status | jq '.data.memory_usage_mb'
```

如果发现异常内存增长，请提交Issue报告。

---

## 故障排除

### Q: 收到的数据不完整怎么办？

**A:**

**检查清单：**

1. **验证CRC校验：**
   - 查看日志中是否有CRC错误
   - 检查传输线缆质量（Serial模式）

2. **检查采样率设置：**
   - 降低采样率重试
   - 确认总带宽不超过限制

3. **检查触发配置：**
   ```bash
   # 触发模式：确认pre/post样本数设置合理
   # 检查触发事件是否完整
   curl http://127.0.0.1:8080/api/trigger/preview/{burst_id}
   ```

4. **查看设备日志：**
   ```bash
   # 设备可能上报错误
   # 查看CMD_LOG_MESSAGE或NACK响应
   ```

### Q: 触发事件不产生怎么办？

**A:**

**1. 确认已启用触发模式：**
```bash
curl http://127.0.0.1:8080/api/control/status | jq '.data.current_mode'
# 应该返回 "trigger"
```

**2. 检查触发条件：**
- 信号幅度是否达到触发阈值
- 触发通道配置是否正确
- 设备固件是否支持触发功能

**3. 手动触发测试：**
```bash
# 使用模拟器的手动触发功能
# 或发送测试信号验证触发逻辑
```

**4. 查看设备信息：**
```bash
curl -X POST http://127.0.0.1:8080/api/control/device_info
# 检查设备是否支持触发模式
```

### Q: 保存文件失败怎么办？

**A:**

**常见原因：**

1. **路径权限问题：**
   ```bash
   # 确认数据目录存在且可写
   ls -ld data-gateway/data
   chmod 755 data-gateway/data
   ```

2. **磁盘空间不足：**
   ```bash
   df -h  # 检查可用空间
   ```

3. **文件名非法字符：**
   - 避免使用 `/ \ : * ? " < > |`
   - 使用ASCII字符和下划线

4. **批次不存在：**
   ```bash
   # 确认批次ID有效
   curl http://127.0.0.1:8080/api/trigger/list
   ```

### Q: 如何启用调试日志？

**A:**

```bash
# 方式1: 环境变量
export RUST_LOG=debug
cargo run

# 方式2: 在.env文件中
echo "RUST_LOG=debug" >> .env
cargo run

# 方式3: 仅特定模块
export RUST_LOG=data_gateway::device_communication=debug
```

**日志级别：**
- `error`: 仅错误
- `warn`: 警告和错误
- `info`: 一般信息（推荐生产环境）
- `debug`: 调试信息（开发环境）
- `trace`: 详细跟踪（性能影响大）

---

## 开发相关

### Q: 如何添加自定义传输层？

**A:**

参考`device-simulator/transport.h`接口：

```c
typedef struct {
    int (*init)(void);
    int (*send)(const uint8_t* data, size_t len);
    int (*receive)(uint8_t* data, size_t max_len);
    void (*close)(void);
} transport_interface_t;
```

实现这4个函数即可集成新的传输方式（如UART、SPI、以太网等）。

详细示例见：
- `transport_tcp_client.c` - TCP Socket实现
- `transport_test.c` - 测试数据实现

### Q: 如何扩展协议添加新命令？

**A:**

**1. 定义CommandID：**
```c
// 在protocol.h中添加
#define CMD_YOUR_NEW_COMMAND  0x50
```

**2. 实现数据处理器：**
```rust
// 在device_communication.rs中添加
0x50 => {
    // 处理你的命令
    self.handle_your_command(&payload).await?;
}
```

**3. 更新协议文档：**
- 在`docs/protocol_doc.md`中记录新命令

**4. 测试：**
- 编写单元测试
- 使用protocol-cli工具测试

详细参考[CONTRIBUTING.md](../CONTRIBUTING.md#协议扩展)。

### Q: 如何运行测试？

**A:**

**Rust测试：**
```bash
cd data-gateway
cargo test                 # 运行所有测试
cargo test test_name       # 运行特定测试
cargo test -- --nocapture  # 显示println输出
```

**集成测试：**
```bash
# 1. 启动模拟器
cd device-simulator-cli
make run

# 2. 启动data-gateway
cd data-gateway
cargo run

# 3. 运行测试脚本
cd scripts
python3 api-test.py
```

### Q: 如何贡献代码？

**A:**

请参考详细的[贡献指南](../CONTRIBUTING.md)，简要流程：

1. Fork项目
2. 创建功能分支：`git checkout -b feat/your-feature`
3. 编写代码和测试
4. 提交PR并描述变更
5. 响应代码审查反馈

---

## 其他问题

### Q: 支持哪些操作系统？

**A:**

| 组件 | Linux | macOS | Windows |
|------|-------|-------|---------|
| data-gateway | ✅ | ✅ | ✅ |
| device-simulator | ✅ | ✅ | ✅ (MinGW) |
| Serial模式 | ✅ | ✅ | ✅ |

所有主流平台都支持，推荐Linux用于生产部署。

### Q: 许可证是什么？

**A:**

项目采用**Apache License 2.0**开源许可证。

**简单说明：**
- ✅ 商业使用
- ✅ 修改
- ✅ 分发
- ✅ 私有使用
- ⚠️ 需要包含许可证和版权声明
- ⚠️ 修改的文件需要说明

详细信息请参阅[LICENSE](../LICENSE)文件。

### Q: 如何获取技术支持？

**A:**

**社区支持：**
- 📖 查阅文档：[docs/](.)
- 🔍 搜索已有Issue：[GitHub Issues](https://github.com/edgedaq/trigger-daq/issues)
- 💬 提问新Issue：详细描述问题和环境

**报告Bug：**
- 使用Bug报告模板
- 包含完整的复现步骤
- 附上日志和配置文件

**功能建议：**
- 使用功能请求模板
- 说明用例和价值
- 讨论实现方案

---

## 没有找到答案？

如果你的问题不在上述列表中：

1. 查看详细文档：
   - [协议文档](protocol_doc.md)
   - [API文档](api_doc.md)
   - [架构文档](ARCHITECTURE.md)

2. 搜索或创建Issue：
   - [Issues页面](https://github.com/edgedaq/trigger-daq/issues)

3. 参考示例代码：
   - `dashboards/dashboardv5.html` - Web界面示例
   - `scripts/api-test.py` - API调用示例

我们会持续更新FAQ，感谢你的反馈！
