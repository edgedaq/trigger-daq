# 开源发布指南

本指南将帮助你完成项目的开源发布流程。

## ✅ 已完成的准备工作

- [x] 完整的文档体系
  - [x] CONTRIBUTING.md - 贡献指南
  - [x] CHANGELOG.md - 变更日志
  - [x] README.md + README_EN.md - 项目说明（中英文）
  - [x] docs/FAQ.md - 常见问题
  - [x] docs/ARCHITECTURE.md - 架构文档
  - [x] docs/api_doc.md - API文档
  - [x] docs/protocol_doc.md - 协议文档

- [x] GitHub模板
  - [x] Issue模板（Bug报告、功能请求、提问）
  - [x] PR模板

- [x] 项目命名
  - [x] GitHub组织：**edgedaq**
  - [x] 项目名称：**trigger-daq**
  - [x] 完整地址：`https://github.com/edgedaq/trigger-daq`

- [x] 许可证：Apache 2.0

## 📋 发布步骤

### 第1步：创建GitHub组织

1. 访问 https://github.com/organizations/plan
2. 选择 **"Create a free organization"**
3. 填写信息：
   - **Organization name**: `edgedaq`
   - **Contact email**: 你的邮箱
   - **This organization belongs to**: My personal account
4. 完成创建

### 第2步：配置组织信息（可选但推荐）

1. 进入组织设置页面：`https://github.com/edgedaq`
2. 点击 **Settings**
3. 填写组织资料：
   - **Display name**: EdgeDAQ
   - **Description**: Edge data acquisition tools and systems
   - **Website**: 如果有的话
   - **Email**: 公开联系邮箱
   - **Location**: 你的位置
   - **Profile picture**: 上传组织Logo（可选）

### 第3步：创建仓库

1. 在组织页面点击 **"New repository"**
2. 填写仓库信息：
   ```
   Repository name: trigger-daq

   Description:
   High-performance real-time data acquisition system with trigger burst management and Protocol V6 support

   Visibility: Public ✓

   ⚠️ 不要勾选以下选项（我们已有代码）：
   □ Add a README file
   □ Add .gitignore
   □ Choose a license
   ```
3. 点击 **"Create repository"**

### 第4步：准备本地仓库

在你的项目目录执行以下命令：

```bash
cd /mnt/c/Users/19100/Desktop/data-recorder

# 初始化Git仓库（如果还没有）
git init

# 添加所有文件
git add .

# 创建首次提交
git commit -m "feat: initial release v2.0

- Complete trigger burst management system
- Real-time data acquisition with Protocol V6
- REST API and WebSocket streaming interfaces
- Multi-format data export (JSON/CSV/Binary)
- Comprehensive documentation (中英文)
- Quality assessment and smart caching
- Development and production deployment support

🚀 Powered by Rust + C | Apache 2.0 License"

# 重命名主分支为main
git branch -M main
```

### 第5步：推送到GitHub

```bash
# 添加远程仓库
git remote add origin https://github.com/edgedaq/trigger-daq.git

# 推送代码
git push -u origin main
```

如果推送需要认证，使用以下方式之一：
- **HTTPS**: 使用Personal Access Token (推荐)
- **SSH**: 配置SSH密钥

#### 创建Personal Access Token

1. GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)
2. Generate new token (classic)
3. 设置权限：
   - [x] repo (完整仓库访问)
   - [x] workflow (如果需要CI/CD)
4. 生成并保存token
5. 推送时使用token作为密码

### 第6步：完善仓库设置

#### 6.1 设置仓库描述和标签

在仓库页面：
1. 点击右上角的 **⚙️ Settings**
2. 在 **About** 部分：
   - **Description**: 已自动填写
   - **Website**: 如果有项目网站
   - **Topics** (标签)，添加以下标签：
     ```
     data-acquisition
     daq
     rust
     embedded
     iot
     real-time
     trigger
     protocol
     industrial
     sensor-data
     websocket
     api
     ```

#### 6.2 启用功能

在 Settings → General → Features：
- [x] Issues
- [x] Projects (可选)
- [x] Wiki (可选)
- [x] Discussions (推荐 - 用于社区讨论)

#### 6.3 配置分支保护（可选）

Settings → Branches → Add branch protection rule：
- Branch name pattern: `main`
- [x] Require a pull request before merging
- [x] Require status checks to pass before merging (如果有CI)

### 第7步：创建首个Release

1. 进入仓库，点击右侧 **"Releases"**
2. 点击 **"Create a new release"**
3. 填写信息：
   ```
   Tag version: v2.0.0

   Release title: v2.0.0 - Trigger Burst Management

   Description:
   ## 🎉 首个公开版本

   这是 trigger-daq 的首个公开版本，提供完整的触发批次管理系统。

   ### ✨ 核心特性

   - **触发批次管理**: 智能缓存和管理触发事件数据
   - **实时数据采集**: 支持连续模式和触发模式
   - **Protocol V6**: 高效的二进制通信协议
   - **REST API & WebSocket**: 完整的Web接口
   - **多格式导出**: JSON/CSV/Binary格式支持
   - **质量评估**: 自动数据质量分析
   - **双模式部署**: 开发测试和生产部署

   ### 📦 组件

   - **data-gateway** (Rust): 核心数据处理器
   - **device-simulator** (C): 设备模拟器
   - **device-simulator-cli** (C): CLI工具
   - **protocol-cli** (C): 协议调试工具

   ### 📚 文档

   - [快速开始](README.md)
   - [API文档](docs/api_doc.md)
   - [协议规范](docs/protocol_doc.md)
   - [架构文档](docs/ARCHITECTURE.md)
   - [FAQ](docs/FAQ.md)
   - [贡献指南](CONTRIBUTING.md)

   ### 🚀 快速体验

   ```bash
   # 克隆仓库
   git clone https://github.com/edgedaq/trigger-daq.git
   cd trigger-daq

   # 启动模拟器
   cd device-simulator-cli && make run

   # 启动数据处理器
   cd data-gateway && cargo run --release

   # 打开测试界面
   # 浏览器访问 dashboards/dashboardv5.html
   ```

   ### 📄 许可证

   Apache License 2.0

   ---

   完整变更日志请查看 [CHANGELOG.md](CHANGELOG.md)
   ```
4. 选择 **"Set as the latest release"**
5. 点击 **"Publish release"**

### 第8步：添加README徽章（已完成）

README.md 顶部已包含以下徽章：
- License
- Version
- Rust版本
- Protocol版本

### 第9步：启用GitHub Discussions（推荐）

1. Settings → General → Features
2. 勾选 **Discussions**
3. 设置讨论分类：
   - 💬 General - 一般讨论
   - 💡 Ideas - 想法和建议
   - 🙏 Q&A - 问答
   - 📣 Announcements - 公告
   - 🎉 Show and tell - 展示你的应用

### 第10步：编写组织README（可选）

创建特殊仓库 `.github` 来展示组织信息：

1. 创建新仓库：`edgedaq/.github`
2. 添加 `profile/README.md`：

```markdown
# EdgeDAQ

Edge data acquisition tools and systems for industrial IoT applications.

## 🚀 Projects

### [trigger-daq](https://github.com/edgedaq/trigger-daq)
High-performance real-time data acquisition system with trigger burst management.

- ⚡ Real-time data streaming
- 🎯 Event-driven trigger mode
- 📊 Smart burst management
- 🔌 REST API & WebSocket
- 📦 Multi-format export

## 🌟 Features

- Built with Rust for performance and safety
- Protocol V6 for efficient communication
- Production-ready deployment
- Comprehensive documentation

## 📫 Contact

- Issues: [Report bugs or request features](https://github.com/edgedaq/trigger-daq/issues)
- Discussions: [Join community discussions](https://github.com/edgedaq/trigger-daq/discussions)
```

## 🎯 发布后的推广

### 1. 社交媒体分享

在以下平台分享你的项目：
- Twitter/X: 使用标签 #RustLang #IoT #DataAcquisition
- Reddit: r/rust, r/embedded, r/IOT
- LinkedIn: 专业技术社区
- 微信公众号/知乎: 中文技术社区

### 2. 提交到项目索引

- [Awesome Rust](https://github.com/rust-unofficial/awesome-rust) - 提交PR
- [crates.io](https://crates.io/) - 发布Rust包（如果需要）
- [Rust Weekly](https://this-week-in-rust.org/) - 提交项目
- [Open Source Alternatives](https://www.opensourcealternative.to/)

### 3. 编写介绍文章

考虑撰写以下内容：
- 技术博客：项目设计思路和技术选型
- 使用教程：实际应用案例
- 视频演示：快速开始和功能展示

### 4. 参与社区

- 及时回复Issues和PR
- 定期更新项目进展
- 收集用户反馈
- 考虑组织线上交流

## 📊 项目监控

使用以下工具监控项目：
- **GitHub Insights**: 查看Star、Fork、流量统计
- **GitHub Actions**: 设置CI/CD（未来）
- **Dependabot**: 自动依赖更新（可选）

## 🎉 完成！

恭喜！你的项目已经准备好开源了。

项目地址：**https://github.com/edgedaq/trigger-daq**

---

如有问题，欢迎在Issues中提问。祝你的开源项目成功！🚀
