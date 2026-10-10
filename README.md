# iOS ceserver (ARM64)

[![Platform](https://img.shields.io/badge/platform-iOS%2012.0%2B-blue.svg)](https://developer.apple.com)
[![Architecture](https://img.shields.io/badge/arch-ARM64-orange.svg)]()
[![Build System](https://img.shields.io/badge/build-Theos-success.svg)](https://theos.dev)
[![Version](https://img.shields.io/badge/version-1.1.0-brightgreen.svg)]()

运行在越狱 iOS (ARM64) 设备上的 **Cheat Engine Server (`ceserver`)** 服务端程序，基于 Theos 工具链构建与打包，支持桌面端 Cheat Engine（Windows / macOS）通过 TCP 网络直接连接并调试 iOS 进程。

---

## ✨ 核心特性与底层适配

- **权限与进程附加**：
  - 声明专用 `entitlements.plist`（包含 `task_for_pid-allow`、`platform-application`、`com.apple.system-task-ports`），支持跨进程安全获取 `task port`。
- **内存读写与 W^X 保护绕过**：
  - 支持安全写只读代码段：修改内存保护位写入后自动恢复，并通过 `vm_machine_attribute(..., MATTR_VAL_ICACHE_FLUSH)` 同步刷新 ARM64 指令缓存（I-Cache），防止指令修改后 Crash 或不生效。
- **虚拟内存 Submap 穿透**：
  - 实现递归物理叶子页查询（`query_leaf_region`），可穿透 Darwin 系统的多级虚拟内存 Submap，获取真实的内存页权限与物理分布。
- **高版本 dyld 模块解析**：
  - 适配现代 iOS dyld SPI（`_dyld_process_info_create` 与 `_dyld_process_info_for_each_image`），配合 Apple Block 语法（`-fblocks`）稳定遍历进程已加载模块。
- **分块压缩传输**：
  - 链接系统 `zlib`（`-lz`），支持大块内存数据与符号流的分块压缩传输，显著提升网络吞吐效率。
- **优雅退出与常驻守护**：
  - 采用 **Self-Pipe Trick** 监听 `SIGTERM`/`SIGINT`，保证服务停止或重启时主动释放监听套接字，杜绝 `Address already in use` 端口泄漏。
  - 忽略 `SIGPIPE`，防止客户端异常断开导致服务端崩溃。
- **跨越狱环境兼容**：
  - 兼容现代 Rootless 越狱（Dopamine / Palera1n）及标准 Rootful 越狱环境，支持无沙盒权限提权运行与 `task_for_pid` 调试。

---

## 📂 源码结构

```text
.
├── api.c / api.h           # 内存读写、模块遍历、符号解析及虚拟内存操作
├── ceserver.c / ceserver.h # 主程序入口、网络协议分发与连接生命周期
├── debugger.c / debugger.h # 调试器断点与异常处理相关
├── porthelp.c / porthelp.h # 跨平台网络与系统调用适配
├── symbols.c / symbols.h   # 符号表导出与处理
├── entitlements.plist      # task_for_pid 与系统级权限声明
├── layout/                 # 安装包根目录结构（包含 LaunchDaemon 配置文件）
├── Makefile                # Theos 构建配置文件
├── build.sh                # 便捷构建打包脚本
└── BUILD_NOTES.md          # 详细底层踩坑与编译记录
```

---

## 🛠️ 编译与打包

项目使用 [Theos](https://theos.dev) 进行编译与打包。

### 1. 便捷一键构建
项目根目录提供了 `build.sh`，自动清理并生成 Debian 软件包：

```bash
# 全新打包（默认自动执行 make clean && make package）
sh ./build.sh

# Release 正式包（去除调试符号）
FINALPACKAGE=1 sh ./build.sh

# 仅清理构建缓存
sh ./build.sh clean
```

### 2. 使用原生 Theos 命令

```bash
# 调试构建与打包
make clean && make package

# 遇到 Clang ModuleCache 时间戳不一致时的安全构建
make package ceserver_USE_MODULES=0
```

构建产物输出于 `packages/` 目录下（如 `packages/com.0xiuks.ceserver_1.1.0_iphoneos-arm64.deb`）。

---

## 🚀 安装与运行

### 安装方式
将生成的 `.deb` 文件传输至越狱 iOS 设备并通过终端或包管理器安装：
```bash
dpkg -i packages/com.0xiuks.ceserver_1.1.0_iphoneos-arm64.deb
```

### 运行模式
- **产物路径**：`/usr/bin/ceserver`
- **默认端口**：`TCP 52736`
- **命令行启动**：
  ```bash
  # 默认在 52736 端口启动
  ceserver

  # 指定自定义端口启动
  ceserver -p 52736
  ```
- **LaunchDaemon 后台常驻**：
  - 配置文件：`/Library/LaunchDaemons/com.0xiuks.ceserver.plist`
  - 运行日志输出至 `/var/log/ceserver.log`。
  - 加载/重载命令：
    ```bash
    launchctl load -w /Library/LaunchDaemons/com.0xiuks.ceserver.plist
    launchctl unload /Library/LaunchDaemons/com.0xiuks.ceserver.plist
    ```

---

## 💻 Cheat Engine 客户端连接

1. 在桌面端（Windows / macOS）打开 Cheat Engine。
2. 依次点击菜单：`File` -> `Network Configuration`（或网络连接按钮）。
3. 输入 iOS 设备的局域网 IP 地址及端口（默认 `52736`）。
4. 连接成功后，在进程列表中即可选择 iOS 目标进程进行内存搜索与调试。

---

## 📄 许可证与鸣谢

本项目基于 [Cheat Engine](https://github.com/cheat-engine/cheat-engine) 的 `ceserver` 模块进行 iOS ARM64 深度移植与增强。
遵循原始项目的开源许可协议。
