# iOS ceserver (v1.1.0) 编译与开发踩坑记录

本文档记录了基于 Theos 在 iOS (ARM64) 越狱环境下移植与编译 Cheat Engine Server (`ceserver`) 的核心踩坑点、底层机制与解决方案。

---

## 1. 编译环境与工具链配置

### 坑点 1：沙盒/移动端环境找不到 `make` 与 `dpkg-deb`
- **现象**：在默认辅助终端（`/Helpers/sh`）下直接运行 `make` 或 `dpkg-deb` 提示 `command not found`。
- **原因**：移动端 CodexApp 将完整的 Theos 工具链、Clang (LLVM-16) 与打包工具内置于 `TheosRuntime` 目录下，默认精简版 shell 仅内置了部分基础命令。
- **正确方式**：使用环境专属 `bash`，确保环境变量指向 `TheosRuntime`：
  ```bash
  # 必须使用 CodexApp 附带的 bash 执行编译
  /private/var/containers/Bundle/Application/0462B698-DEA1-4366-BDCD-33BCA632314D/CodexApp.app/Helpers/bash -c "make package"
  # 或在任意终端下直接运行包装脚本：
  sh ./build.sh package
  ```
  该环境中内置了 `$THEOS` 变量（指向 `TheosRuntime/theos`）及 iPhoneOS SDK。

### 补充：Clang ModuleCache 与工具链头文件时间戳不一致
- **现象**：构建时报错：
  ```text
  fatal error: file '.../__stddef_max_align_t.h' has been modified since the module file '.../ModuleCache/...pcm' was built: mtime changed
  note: please rebuild precompiled header '...pcm'
  ```
- **原因**：Clang 复用了旧的预编译模块（PCM），但 CodexApp 内置工具链中的头文件时间戳已变化，Clang 因缓存失效而拒绝继续编译。`make clean` 清理项目的 `.theos` 构建目录，不一定会清除这个共享 ModuleCache。
- **解决方案**：优先清理报错路径指向的失效 ModuleCache 子目录，再正常运行 `make package`。如果当前环境无法清理共享缓存，可对本次构建关闭该工具目标的 Clang modules：
  ```bash
  make package ceserver_USE_MODULES=0
  ```
  该参数不修改项目文件；之后的默认构建仍可能复用旧缓存并再次报错。
- **附带警告**：若看到 `setlocale: LC_ALL: cannot change locale (C.UTF-8)`，这是运行环境缺少该 locale 的警告；本次构建中它不影响编译、签名或打包结果。

---

## 2. 源码编译与语法坑

### 坑点 2：`<sys/select.h>` 在 Darwin POSIX 下的声明丢失
- **现象**：`ceserver.c` 编译时报 `implicit declaration of function 'select'` 或找不到 `fd_set` / `select` 原型。
- **原因**：在定义了 `_POSIX_C_SOURCE 200809L` 和 `__DARWIN_C_LEVEL` 的交叉编译头文件环境下，Darwin 的 `<sys/select.h>` 条件宏可能导致 `select` 函数声明被意外跳过。
- **解决方案**：
  - 在 `ceserver.c` 中显式包含 `<sys/_select.h>`；
  - 显式声明 `int select(int, fd_set * __restrict, fd_set * __restrict, fd_set * __restrict, struct timeval * __restrict);`。

### 坑点 3：C 代码中使用 Apple Block 语法报语法错误
- **现象**：`api.c` 中调用 `_dyld_process_info_for_each_image(..., ^(uint64_t addr, ...))` 遍历动态库时报错 `blocks support not enabled`。
- **原因**：Clang 默认对 C 代码禁用 Blocks 扩展。
- **解决方案**：在 `Makefile` 中添加：
  ```makefile
  ceserver_CFLAGS += -fblocks
  ```

### 坑点 4：缺少 zlib 链接导致未定义符号
- **现象**：链接时报错找不到 `_deflateInit_`, `_deflate`, `_deflateEnd` 等符号。
- **原因**：符号表导出与内存传输均采用了分块压缩机制，依赖系统 libz。
- **解决方案**：在 `Makefile` 中添加：
  ```makefile
  ceserver_LIBRARIES = z
  ```

---

## 3. iOS 系统与底层逆向机制

### 坑点 5：`task_for_pid` 权限缺失导致附加失败
- **现象**：`OpenProcess(pid)` 总是返回 0，日志记录 `task_for_pid failed: 5` (`KERN_FAILURE`)。
- **原因**：iOS 内核对跨进程获取 task port 有极严格的限制。
- **解决方案**：
  - 必须编写 `entitlements.plist`，声明：
    - `task_for_pid-allow = true`
    - `platform-application = true`
    - `com.apple.system-task-ports = true`
    - `com.apple.private.security.container-required = false`
  - 在 `Makefile` 强制签名：
    ```makefile
    ceserver_CODESIGN_FLAGS = -Sentitlements.plist
    ```

### 坑点 6：代码段写入与 W^X 保护及指令缓存失效
- **现象**：对只读可执行代码段下软件断点或写内存时失败，或写入后目标进程未按预期改变甚至 Crash。
- **原因**：
  1. iOS 的 W^X 保护禁止同时存在写和执行权限，强行申请 RWX 会被内核剥离可执行位；
  2. ARM64 拥有独立的指令缓存（I-Cache）和数据缓存（D-Cache），通过 `mach_vm_write` 修改指令后，I-Cache 仍缓存着旧指令。
- **解决方案**：
  - 写入时临时修改为 `VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY`；
  - 写入完成后恢复原始保护，并立刻执行：
    ```c
    int state = MATTR_VAL_ICACHE_FLUSH;
    vm_machine_attribute(task, cache_base, cache_len, MATTR_CACHE, &state);
    ```

### 坑点 7：Darwin 虚拟内存 Submap 遮蔽真实权限
- **现象**：调用 `mach_vm_region` 查询内存属性时，返回全为只读或大跨度未分配。
- **原因**：Darwin 存在多级 Submap 机制，顶层 region 只是子映射容器（`is_submap == 1`）。
- **解决方案**：实现递归查询函数 `query_leaf_region`，最大递归展开 64 层，直至取得真正的物理叶子页属性。

### 坑点 8：动态库枚举与高版本 dyld 兼容
- **现象**：读取 `dyld_all_image_infos` 结构体解析模块列表时，高版本 iOS 下经常由于结构体偏移变更导致数据错乱或越界读取。
- **解决方案**：改用系统私有动态 SPI（`_dyld_process_info_create` 和 `_dyld_process_info_for_each_image`），由系统 dyld 自身负责遍历，稳定且安全。

---

## 4. 守护进程与网络生命周期

### 坑点 9：被杀后端口未释放导致重启失败
- **现象**：使用 LaunchDaemon 常驻后台时，服务退出后重启报 `bind: Address already in use`。
- **原因**：旧版主线程阻塞在 `accept()`，当被系统信号（SIGTERM）终止时无法主动关闭监听套接字。
- **解决方案**：
  - 采用 **Self-Pipe Trick**：创建非阻塞管道 `shutdown_pipe`；
  - 注册 `SIGTERM`/`SIGINT` 信号捕获，信号到来时向管道写入字节唤醒 `select`；
  - 主循环检测到关闭通知后，主动 `close(listen_socket)` 并退出；
  - 显式设置 `sigaction(SIGPIPE, SIG_IGN)`，避免客户端异常断网导致服务端被系统中断杀死。

---

## 5. 打包与发布命令速查

```bash
# 便捷构建（自动先 clean 后 package）
sh ./build.sh

# 原生 Debug 打包
make clean && make package

# Release 正式包（去除调试符号）
FINALPACKAGE=1 make package

# 清理构建缓存
make clean
```
