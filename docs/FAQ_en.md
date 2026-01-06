# Frequently Asked Questions (FAQ)

English | [简体中文](FAQ.md)

This document answers common questions about the universal data acquisition system.

## Table of Contents

- [Installation and Configuration](#installation-and-configuration)
- [Connection and Communication](#connection-and-communication)
- [Data Acquisition](#data-acquisition)
- [Trigger Mode](#trigger-mode)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [Development](#development)

---

## Installation and Configuration

### Q: What are the system dependencies?

**A:** Dependencies are divided into two parts:

**data-gateway (Rust):**
- Rust 1.70 or higher
- Cargo package manager
- OS: Linux, macOS, Windows

**device-simulator (C):**
- GCC or Clang compiler
- Make tool
- Standard C library

For detailed installation steps, see [CONTRIBUTING.md](../CONTRIBUTING.md#development-environment-setup).

### Q: How to configure environment variables?

**A:** Copy `.env.example` to `.env`, then modify:

```bash
cd data-gateway
cp .env.example .env
nano .env  # or use another editor
```

Main configuration items:
```bash
# Connection: serial (production) or socket (development)
DEVICE_TYPE=socket

# Socket address (development mode)
SOCKET_ADDRESS=127.0.0.1:9001

# Serial port (production mode)
SERIAL_PORT=COM7          # Windows
# SERIAL_PORT=/dev/ttyUSB0  # Linux/macOS

# Service ports
WEB_PORT=8080
WS_PORT=8081

# Data storage
DATA_DIR=./data
TRIGGER_CACHE_SIZE=10
```

### Q: How to choose connection method (Serial vs Socket)?

**A:**

| Scenario | Recommended | Configuration |
|----------|-------------|--------------|
| Development & Testing | TCP Socket | `DEVICE_TYPE=socket` |
| Real Hardware Deployment | USB-CDC Serial | `DEVICE_TYPE=serial` |
| Remote Device | TCP Socket | Configure remote IP |

**Development testing** with Socket is more convenient - run simulator and data-gateway on the same machine.

**Production deployment** with Serial directly connects to MCU device - more stable and reliable.

---

## Connection and Communication

### Q: data-gateway cannot connect to device, what to do?

**A:** Troubleshoot step by step:

**1. Check if device is running:**
```bash
# Socket mode: confirm simulator is started
cd device-simulator-cli
make run

# Serial mode: confirm device is connected
ls /dev/ttyUSB*  # Linux/macOS
# or check COM port in Device Manager (Windows)
```

**2. Check configuration:**
```bash
# View current configuration
cat data-gateway/.env

# Socket mode: confirm address and port
SOCKET_ADDRESS=127.0.0.1:9001

# Serial mode: confirm serial port name
SERIAL_PORT=/dev/ttyUSB0
```

**3. Check permissions (Linux/macOS):**
```bash
# Add user to dialout group (Serial mode)
sudo usermod -a -G dialout $USER
# Requires re-login
```

**4. View logs:**
```bash
# Start with detailed logs
RUST_LOG=debug cargo run
```

### Q: WebSocket connection frequently drops, what to do?

**A:**

**Possible causes and solutions:**

1. **Network unstable**
   - Use wired network instead of WiFi
   - Check firewall settings
   - Ensure no proxy interference

2. **Data volume too large**
   - Reduce sample rate
   - Decrease active channel count
   - Implement client data buffering

3. **Client implementation issue**
   - Implement auto-reconnect mechanism
   - Send ping periodically to keep connection alive
   ```javascript
   // Recommended reconnect logic
   function connectWebSocket() {
       const ws = new WebSocket('ws://127.0.0.1:8081');

       ws.onclose = () => {
           setTimeout(connectWebSocket, 3000);  // Reconnect after 3s
       };
   }
   ```

### Q: How to verify device connection status?

**A:** Use status API:

```bash
# View system status
curl http://127.0.0.1:8080/api/control/status | jq

# Check device_connected field
{
  "device_connected": true,
  "connection_type": "socket",
  "packets_processed": 1234
}
```

Or check connection status indicator on test interface.

---

## Data Acquisition

### Q: What is the maximum sample rate?

**A:**

| Mode | Max Sample Rate | Limiting Factor |
|------|----------------|-----------------|
| Socket mode (simulator) | ~100kHz/channel | CPU and network bandwidth |
| Serial mode (MCU) | ~100kHz/channel | MCU performance and USB bandwidth |
| Multi-channel total bandwidth | ~1MB/s | Transport layer limit |

**Optimization suggestions:**
- Single channel high-speed acquisition is more reliable than multi-channel
- Use int16 format instead of float32 to save bandwidth
- Consider trigger mode instead of continuous streaming

### Q: How to configure multi-channel acquisition?

**A:** Use configuration API:

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

Or set in configuration panel on test interface.

### Q: What data format options are available?

**A:**

| Format Code | Type | Bytes | Range | Use Case |
|------------|------|-------|-------|----------|
| 1 | int16 | 2 | -32768~32767 | General, saves bandwidth |
| 2 | int32 | 4 | -2^31~2^31-1 | High precision integer |
| 4 | float32 | 4 | IEEE-754 | Floating point operations |

**Recommendation:**
- Use **int16** for most cases - sufficient precision and saves 50% bandwidth
- Use **float32** when floating point operations needed

---

## Trigger Mode

### Q: What's the difference between trigger mode and continuous mode?

**A:**

| Feature | Continuous Mode | Trigger Mode |
|---------|----------------|--------------|
| Data flow | Continuous sending | Send when event triggered |
| Use case | Real-time monitoring | Abnormal event capture |
| Data management | Save/discard in real-time | Burst cache, user selects save |
| Memory usage | Low | Medium (caches bursts) |
| Data completeness | Continuous | Includes pre/post event context |

**When to use trigger mode:**
- Focus on specific events (e.g., vibration anomalies, impacts)
- Need complete data before and after event
- Want to preview data quality before saving
- Limited storage space, only save important data

### Q: How to use trigger mode?

**A:** Complete workflow:

**1. Set trigger mode:**
```bash
# 1. Switch to trigger mode
curl -X POST http://127.0.0.1:8080/api/control/trigger_mode

# 2. Start acquisition
curl -X POST http://127.0.0.1:8080/api/control/start
```

**2. Wait for trigger events:**
- Device automatically collects data when trigger conditions detected
- Receive real-time notifications via WebSocket

**3. View trigger bursts:**
```bash
# Get burst list
curl http://127.0.0.1:8080/api/trigger/list

# Preview specific burst
curl http://127.0.0.1:8080/api/trigger/preview/{burst_id}
```

**4. Save burst data:**
```bash
curl -X POST http://127.0.0.1:8080/api/trigger/save/{burst_id} \
  -H "Content-Type: application/json" \
  -d '{
    "dir": "experiments/test1",
    "filename": "impact_001",
    "format": "csv",
    "description": "Impact test data"
  }'
```

### Q: How many trigger bursts can be cached?

**A:**

Default cache is **10** bursts, adjustable via environment variable:

```bash
# Configure in .env
TRIGGER_CACHE_SIZE=20  # Increase to 20
```

**Notes:**
- Each burst uses ~1-5MB memory
- When cache full, oldest burst auto-deleted
- Save important burst data promptly

### Q: How to judge trigger data quality?

**A:**

System automatically assesses each burst, providing quality status:

```json
{
  "quality": "Good",  // or "Warning" / "Error"
  "quality_summary": {
    "overall_quality": {"status": "Good"},
    "voltage_range": [0.1, 3.2],
    "anomaly_count": 0,
    "channel_stats": [...]
  }
}
```

**Quality assessment indicators:**
- ✅ **Good**: Data normal, can save
- ⚠️ **Warning**: Minor issues (near saturation, slight flatness)
- ❌ **Error**: Serious issues (out of range, saturated, data missing)

**Common issues:**
- **Saturation**: Signal exceeds ADC range, adjust gain or attenuation
- **Flatness**: Sensor no response, check connections
- **Out of range**: Abnormal voltage, check power and grounding

---

## Performance

### Q: What are system performance metrics?

**A:**

**Latency:**
- End-to-end latency: < 50ms
- Trigger detection latency: < 1ms
- Burst processing time: < 10ms
- WebSocket push latency: < 5ms

**Throughput:**
- Max data rate: 1MB/s
- Trigger burst processing: Multiple events per second
- Concurrent WebSocket clients: Recommend < 10

**Resource usage:**
- Memory: Base 40-100MB, +1-5MB per burst
- CPU: 10-30% single core (normal load)
- Disk: Write on demand, CSV ~2-3x larger than binary

### Q: How to optimize system performance?

**A:**

**1. Sampling configuration optimization:**
```bash
# Only enable needed channels
# Use appropriate sample rate (not too high)
# Choose int16 format instead of float32
```

**2. Network optimization:**
```bash
# Use wired connection
# Limit WebSocket client count
# Consider data compression (future version)
```

**3. Storage optimization:**
```bash
# Use SSD for data storage
# Clean old files regularly
# Use binary format to save space
```

**4. System configuration:**
```bash
# Increase trigger cache (if sufficient memory)
TRIGGER_CACHE_SIZE=20

# Adjust log level
RUST_LOG=info  # Use info instead of debug in production
```

---

## Troubleshooting

### Q: Received data incomplete, what to do?

**A:**

**Checklist:**

1. **Verify CRC checksum:**
   - Check logs for CRC errors
   - Check transmission cable quality (Serial mode)

2. **Check sample rate settings:**
   - Reduce sample rate and retry
   - Confirm total bandwidth within limits

3. **Check trigger configuration:**
   ```bash
   # Trigger mode: confirm pre/post samples reasonably set
   # Check if trigger event complete
   curl http://127.0.0.1:8080/api/trigger/preview/{burst_id}
   ```

4. **View device logs:**
   ```bash
   # Device may report errors
   # Check CMD_LOG_MESSAGE or NACK responses
   ```

### Q: Trigger events not generated, what to do?

**A:**

**1. Confirm trigger mode enabled:**
```bash
curl http://127.0.0.1:8080/api/control/status | jq '.data.current_mode'
# Should return "trigger"
```

**2. Check trigger conditions:**
- Signal amplitude reaches trigger threshold?
- Trigger channel configured correctly?
- Device firmware supports trigger function?

**3. Manual trigger test:**
```bash
# Use simulator's manual trigger function
# Or send test signal to verify trigger logic
```

**4. View device info:**
```bash
curl -X POST http://127.0.0.1:8080/api/control/device_info
# Check if device supports trigger mode
```

### Q: File save fails, what to do?

**A:**

**Common causes:**

1. **Path permission issue:**
   ```bash
   # Confirm data directory exists and writable
   ls -ld data-gateway/data
   chmod 755 data-gateway/data
   ```

2. **Insufficient disk space:**
   ```bash
   df -h  # Check available space
   ```

3. **Illegal characters in filename:**
   - Avoid using `/ \ : * ? " < > |`
   - Use ASCII characters and underscores

4. **Burst doesn't exist:**
   ```bash
   # Confirm burst ID valid
   curl http://127.0.0.1:8080/api/trigger/list
   ```

### Q: How to enable debug logs?

**A:**

```bash
# Method 1: Environment variable
export RUST_LOG=debug
cargo run

# Method 2: In .env file
echo "RUST_LOG=debug" >> .env
cargo run

# Method 3: Specific modules only
export RUST_LOG=data_gateway::device_communication=debug
```

**Log levels:**
- `error`: Errors only
- `warn`: Warnings and errors
- `info`: General info (recommended for production)
- `debug`: Debug info (development)
- `trace`: Detailed tracing (high performance impact)

---

## Development

### Q: How to add custom transport layer?

**A:**

Refer to `device-simulator/transport.h` interface:

```c
typedef struct {
    int (*init)(void);
    int (*send)(const uint8_t* data, size_t len);
    int (*receive)(uint8_t* data, size_t max_len);
    void (*close)(void);
} transport_interface_t;
```

Implement these 4 functions to integrate new transport methods (UART, SPI, Ethernet, etc.).

Detailed examples:
- `transport_tcp_client.c` - TCP Socket implementation
- `transport_test.c` - Test data implementation

### Q: How to extend protocol with new commands?

**A:**

**1. Define CommandID:**
```c
// Add in protocol.h
#define CMD_YOUR_NEW_COMMAND  0x50
```

**2. Implement data processor:**
```rust
// Add in device_communication.rs
0x50 => {
    // Handle your command
    self.handle_your_command(&payload).await?;
}
```

**3. Update protocol docs:**
- Document new command in `docs/protocol_doc_en.md`

**4. Test:**
- Write unit tests
- Test with protocol-cli tool

See [CONTRIBUTING.md](../CONTRIBUTING.md) for details.

### Q: How to run tests?

**A:**

**Rust tests:**
```bash
cd data-gateway
cargo test                 # Run all tests
cargo test test_name       # Run specific test
cargo test -- --nocapture  # Show println output
```

**Integration tests:**
```bash
# 1. Start simulator
cd device-simulator-cli
make run

# 2. Start data-gateway
cd data-gateway
cargo run

# 3. Run test scripts
cd scripts
python3 api-test.py
```

### Q: How to contribute code?

**A:**

See detailed [Contributing Guide](../CONTRIBUTING.md), brief process:

1. Fork project
2. Create feature branch: `git checkout -b feat/your-feature`
3. Write code and tests
4. Submit PR with description
5. Respond to code review feedback

---

## Other Questions

### Q: Which operating systems are supported?

**A:**

| Component | Linux | macOS | Windows |
|-----------|-------|-------|---------|
| data-gateway | ✅ | ✅ | ✅ |
| device-simulator | ✅ | ✅ | ✅ (MinGW) |
| Serial mode | ✅ | ✅ | ✅ |

All major platforms supported, Linux recommended for production deployment.

### Q: What is the license?

**A:**

Project uses **Apache License 2.0** open source license.

**In brief:**
- ✅ Commercial use
- ✅ Modification
- ✅ Distribution
- ✅ Private use
- ⚠️ Must include license and copyright notice
- ⚠️ Modified files need changelog

See [LICENSE](../LICENSE) file for details.

### Q: How to get technical support?

**A:**

**Community support:**
- 📖 Read docs: [docs/](.)
- 🔍 Search existing Issues: [GitHub Issues](https://github.com/edgedaq/trigger-daq/issues)
- 💬 Create new Issue: Describe problem and environment in detail

**Report bugs:**
- Use bug report template
- Include complete reproduction steps
- Attach logs and config files

**Feature suggestions:**
- Use feature request template
- Explain use case and value
- Discuss implementation approach

---

## Didn't Find Your Answer?

If your question isn't in the list above:

1. Check detailed documentation:
   - [Protocol Documentation](protocol_doc_en.md)
   - [API Documentation](api_doc_en.md)
   - [Architecture Documentation](ARCHITECTURE_en.md)

2. Search or create Issue:
   - [Issues Page](https://github.com/edgedaq/trigger-daq/issues)

3. Refer to example code:
   - `dashboards/dashboardv5.html` - Web interface example
   - `scripts/api-test.py` - API call example

We'll continue updating FAQ, thanks for your feedback!
