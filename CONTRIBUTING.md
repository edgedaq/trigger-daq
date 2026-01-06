# 贡献指南

感谢你考虑为通用数据采集系统做出贡献！我们欢迎所有形式的贡献，包括但不限于：

- 报告bug
- 提出新功能建议
- 改进文档
- 提交代码补丁
- 优化性能

## 开发环境设置

### 必需工具

#### Rust开发环境
```bash
# 安装Rust (推荐使用rustup)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 确认版本 (需要 1.70+)
rustc --version

# 安装格式化工具
rustup component add rustfmt clippy
```

#### C语言开发环境
```bash
# Linux/macOS
sudo apt-get install build-essential  # Ubuntu/Debian
# 或
xcode-select --install  # macOS

# Windows
# 安装MinGW-w64或Visual Studio Build Tools
```

### 克隆仓库

```bash
git clone https://github.com/your-username/trigger-daq.git
cd trigger-daq
```

### 构建项目

#### data-gateway (Rust)
```bash
cd data-gateway
cargo build
cargo test
```

#### device-simulator (C)
```bash
cd device-simulator
make
```

#### device-simulator-cli (C)
```bash
cd device-simulator-cli
make
```

## 代码规范

### Rust代码规范

我们遵循官方Rust代码风格指南：

```bash
# 格式化代码
cargo fmt

# 运行linter
cargo clippy -- -D warnings

# 运行测试
cargo test
```

**代码风格要点：**
- 使用4个空格缩进（不使用tab）
- 行宽限制在100字符
- 使用有意义的变量名和函数名
- 为公共API添加文档注释（///）
- 优先使用`Result`和`Option`而不是panic

### C代码规范

我们遵循GNU C代码风格：

```bash
# 使用indent格式化（如果可用）
indent -gnu -i4 -l100 your_file.c
```

**代码风格要点：**
- 使用4个空格缩进
- 函数左花括号另起一行
- 指针星号靠近变量名：`int *ptr`
- 为函数添加注释说明用途
- 避免使用全局变量

### 提交信息规范

我们使用约定式提交（Conventional Commits）：

```
<类型>(<范围>): <简短描述>

<详细描述>

<脚注>
```

**类型：**
- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档变更
- `style`: 代码格式调整（不影响功能）
- `refactor`: 重构（既不是新增功能也不是修复bug）
- `perf`: 性能优化
- `test`: 添加或修改测试
- `chore`: 构建过程或辅助工具的变动

**示例：**
```
feat(trigger): 添加触发批次自动清理功能

实现了基于时间和内存使用的自动清理策略，防止长时间运行导致的内存溢出。

Closes #123
```

## 开发流程

### 1. 创建Issue

在开始工作前，请先创建或找到相关的Issue，讨论你的想法：

- 对于bug：详细描述复现步骤和预期行为
- 对于新功能：解释用例和预期实现方式
- 对于文档改进：说明需要改进的部分

### 2. Fork并创建分支

```bash
# Fork项目到你的GitHub账号

# 克隆你的fork
git clone https://github.com/your-username/trigger-daq.git

# 添加上游仓库
git remote add upstream https://github.com/edgedaq/trigger-daq.git

# 创建功能分支
git checkout -b feat/your-feature-name
# 或
git checkout -b fix/bug-description
```

### 3. 进行开发

- 保持提交原子化（一个提交做一件事）
- 编写有意义的提交信息
- 定期从上游同步代码：
  ```bash
  git fetch upstream
  git rebase upstream/main
  ```

### 4. 编写测试

**Rust测试：**
```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_your_feature() {
        // 测试代码
        assert_eq!(result, expected);
    }
}
```

**C语言测试：**
- 在`tests/`目录下添加测试文件
- 或在函数中使用assert验证

### 5. 运行测试

```bash
# Rust项目
cd data-gateway
cargo test
cargo clippy

# C项目
cd device-simulator-cli
make test  # 如果有测试目标
```

### 6. 提交Pull Request

在GitHub上提交PR前：

1. 确保所有测试通过
2. 更新相关文档
3. 在CHANGELOG.md中添加条目（如果是重要变更）
4. 确保代码已格式化

**PR标题格式：**
```
feat: 添加触发批次自动保存功能
fix: 修复WebSocket连接断开问题
docs: 更新API文档示例
```

**PR描述应包含：**
- 变更的动机和上下文
- 解决的Issue编号（`Closes #123`）
- 测试方法和结果
- 截图或演示（如果适用）

### 7. 代码审查

- 维护者会审查你的PR
- 根据反馈进行修改
- 保持讨论专业和友好
- 通过`git commit --amend`或新提交进行修改

## 项目结构说明

```
trigger-daq/
├── data-gateway/           # Rust数据处理器
│   ├── src/
│   │   ├── main.rs        # 入口
│   │   ├── config.rs      # 配置管理
│   │   ├── device_communication.rs  # 设备通信
│   │   ├── data_processing.rs       # 数据处理和触发管理
│   │   ├── web_server.rs           # REST API
│   │   ├── websocket.rs            # WebSocket服务
│   │   └── file_manager.rs         # 文件管理
│   └── Cargo.toml
├── device-simulator/       # C语言设备模拟器
│   ├── main.c
│   ├── app.c/h
│   ├── protocol/          # 协议实现
│   └── transport/         # 传输层
├── device-simulator-cli/   # C语言CLI工具
│   ├── main.c
│   ├── device_simulator.c/h
│   └── protocol/
├── protocol-cli/          # 协议调试工具
├── docs/                  # 文档目录
├── dashboards/            # Web界面
└── scripts/               # 测试脚本
```

## 测试策略

### 单元测试

- 为核心逻辑编写单元测试
- 测试边界条件和错误情况
- 保持测试独立且可重复

### 集成测试

- 测试模块间交互
- 模拟真实使用场景
- 测试协议通信完整流程

### 手动测试

在提交PR前，请执行以下手动测试：

1. **基础功能测试**
   ```bash
   # 启动模拟器
   cd device-simulator-cli
   make run

   # 启动data-gateway
   cd data-gateway
   cargo run

   # 打开测试界面
   # 浏览器访问 dashboards/dashboardv5.html
   ```

2. **触发模式测试**
   - 设置触发模式
   - 验证触发事件检测
   - 检查数据批次完整性
   - 测试保存功能

3. **连续模式测试**
   - 切换到连续模式
   - 验证实时数据流
   - 检查WebSocket连接稳定性

## 文档贡献

文档同样重要！你可以：

- 修正拼写和语法错误
- 改进现有文档的清晰度
- 添加缺失的文档
- 翻译文档到其他语言
- 添加代码示例和教程

**文档位置：**
- 项目说明：`README.md`
- API文档：`docs/api_doc.md`
- 协议规范：`docs/protocol_doc.md`
- 模块文档：各子目录的`README.md`

## 报告Bug

好的bug报告应该包含：

1. **环境信息**
   - 操作系统和版本
   - Rust版本 / C编译器版本
   - data-gateway版本

2. **复现步骤**
   - 详细的操作步骤
   - 配置文件内容
   - 命令行参数

3. **预期行为**
   - 你期望发生什么

4. **实际行为**
   - 实际发生了什么
   - 错误信息和日志
   - 截图（如果适用）

5. **附加信息**
   - 是否稳定复现
   - 相关的配置和数据文件

## 提出新功能

在提出新功能建议时，请考虑：

1. **用例说明**
   - 这个功能解决什么问题？
   - 目标用户是谁？

2. **设计思路**
   - 如何实现？
   - 是否影响现有功能？

3. **替代方案**
   - 是否可以用现有功能实现？
   - 有无其他实现方式？

## 社区准则

- 尊重所有贡献者
- 保持讨论专业和建设性
- 欢迎新手，耐心回答问题
- 接受建设性批评
- 关注项目目标和用户需求

## 许可证

通过贡献代码，你同意你的贡献将采用Apache 2.0许可证授权。

## 获取帮助

如果你有任何问题：

- 查看[FAQ文档](docs/FAQ.md)
- 搜索现有的[Issues](https://github.com/edgedaq/trigger-daq/issues)
- 创建新的Issue提问
- 参考[API文档](docs/api_doc.md)和[协议文档](docs/protocol_doc.md)

## 致谢

感谢所有为这个项目做出贡献的开发者！

---

再次感谢你的贡献！
