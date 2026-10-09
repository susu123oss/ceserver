# AGENTS.md - iOS ceserver 工程说明与构建指南

本项目为运行在越狱 iOS (ARM64) 设备上的 Cheat Engine Server (`ceserver`)，基于 Theos 进行构建与打包。

## 1. 编译环境与工具链（项目记忆）

### 关键路径信息
- **专用 Bash 绝对路径**（内置 `$THEOS`、`TheosRuntime`、LLVM-16 Clang、iPhoneOS SDK、ldid、dpkg-deb）：
  `/private/var/containers/Bundle/Application/0462B698-DEA1-4366-BDCD-33BCA632314D/CodexApp.app/Helpers/bash`
- **动态探测方式**（若应用更新导致容器 UUID 变动时使用）：
  `find /private/var/containers/Bundle/Application -name "bash" 2>/dev/null | grep "CodexApp.app/Helpers/bash" | head -n 1`
- **注意**：本设备沙盒环境中不存在 `/bin/sh`，执行脚本需使用 `sh ./build.sh` 形式调用，或在 `exec_command` 工具调用中显式指定 `shell` 参数为上述 Bash 绝对路径。

### 便捷构建封装
项目根目录下配置了 `build.sh` 脚本，自动探测并桥接 CodexApp 的 Theos Bash 环境，并且**默认自动执行 `make clean && make package`**，防止中间目标文件或包版本号残留：
```bash
# 一键全新打包（默认自动执行 make clean && make package，输出至 packages/*.deb）
sh ./build.sh

# 显式打包（同样自动先 clean 后 package）
sh ./build.sh package

# Release 正式包（去除调试符号）
FINALPACKAGE=1 sh ./build.sh

# 仅清理构建缓存
sh ./build.sh clean

# 遇到 Clang ModuleCache 时间戳不一致报错时的应对构建
sh ./build.sh ceserver_USE_MODULES=0
```
若在 Codex `exec_command` 工具调用中已指定上述 `shell` 路径，亦可直接执行 `make clean && make package`。

## 2. 关键编译依赖与配置说明
- **Block 语法**：`api.c` 中使用了 dyld SPI 回调 Block，`Makefile` 中必须保持 `ceserver_CFLAGS += -fblocks`。
- **链接库**：依赖 zlib 进行数据压缩，`Makefile` 中必须保持 `ceserver_LIBRARIES = z`。
- **权限与代码签名**：必须使用 `ceserver_CODESIGN_FLAGS = -Sentitlements.plist` 对产物签名，否则 `task_for_pid` 无法获取目标进程权限。
- **头文件声明**：`ceserver.c` 中显式包含了 `<sys/_select.h>` 并手动声明了 `select` 函数原型，以防 Darwin POSIX 标准宏导致原型丢失。

## 3. 架构与运行模式
- **产物输出路径**：`/usr/bin/ceserver`。
- **默认监听端口**：TCP 52736。
- **LaunchDaemon 常驻**：配置文件位于 `layout/Library/LaunchDaemons/com.0xiuks.ceserver.plist`，日志写入 `/var/log/ceserver.log`。
- **优雅退出机制**：采用 Self-Pipe Trick 监听 SIGTERM/SIGINT，退出时保证释放监听 socket。
