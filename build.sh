#!/bin/sh
# 自动定位 CodexApp 内置的 Theos bash 工具链并执行构建
DEFAULT_BASH="/private/var/containers/Bundle/Application/0462B698-DEA1-4366-BDCD-33BCA632314D/CodexApp.app/Helpers/bash"

if [ -x "$DEFAULT_BASH" ]; then
    EXEC_BASH="$DEFAULT_BASH"
else
    EXEC_BASH=$(find /private/var/containers/Bundle/Application -name "bash" 2>/dev/null | grep "CodexApp.app/Helpers/bash" | head -n 1)
fi

if [ -z "$EXEC_BASH" ] || [ ! -x "$EXEC_BASH" ]; then
    echo "[!] 错误: 未找到 CodexApp 内置的 Theos bash 工具链" >&2
    exit 1
fi

# 处理构建命令：默认执行 make clean && make package
if [ "$1" = "clean" ]; then
    BUILD_CMD="make clean"
elif [ "$1" = "package" ]; then
    shift
    BUILD_CMD="make clean && make package $*"
elif [ $# -eq 0 ]; then
    BUILD_CMD="make clean && make package"
else
    BUILD_CMD="make clean && make package $*"
fi

exec "$EXEC_BASH" -c "$BUILD_CMD"
