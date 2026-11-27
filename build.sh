#!/usr/bin/env bash
# ======================================================================
# STRSVR Linux Port - Build Script (robust)
# ======================================================================
# Usage:
#   bash build.sh            # build
#   bash build.sh --run      # build then run
#   bash build.sh --clean    # remove build dir then build
#
# Prereqs (Debian/Ubuntu):
#   sudo apt-get update
#   sudo apt-get install cmake build-essential pkg-config \
#        qtbase5-dev qtbase5-dev-tools libqt5serialport5-dev
# ======================================================================

set -uo pipefail

# ---- locate this script's own directory (CRLF-safe) -------------------
self="${BASH_SOURCE[0]:-$0}"
SCRIPT_DIR="$(cd -- "$(dirname -- "$self")" >/dev/null 2>&1 && pwd -P)"
SCRIPT_DIR="${SCRIPT_DIR%$'\r'}"

# ---- locate the real source root that holds CMakeLists.txt ------------
SRC_DIR=""
for cand in "$SCRIPT_DIR" "$SCRIPT_DIR/.." "$SCRIPT_DIR/../.."; do
    if [ -f "$cand/CMakeLists.txt" ]; then
        SRC_DIR="$(cd -- "$cand" >/dev/null 2>&1 && pwd -P)"
        SRC_DIR="${SRC_DIR%$'\r'}"
        break
    fi
done

echo "=============================================="
echo " STRSVR - GNSS Stream Server (Linux Port)"
echo "=============================================="
echo " script dir : $SCRIPT_DIR"

if [ -z "$SRC_DIR" ]; then
    echo ""
    echo "ERROR: 找不到 CMakeLists.txt。"
    echo "脚本应放在包含 CMakeLists.txt 的 linux_port 目录下运行。"
    echo "当前目录内容:"
    ls -la "$SCRIPT_DIR" 2>/dev/null
    exit 1
fi
echo " source dir : $SRC_DIR"

BUILD_DIR="$SRC_DIR/build"
echo " build dir  : $BUILD_DIR"
echo ""

# ---- tool checks ------------------------------------------------------
if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: 未安装 cmake。请执行: sudo apt-get install cmake build-essential"
    exit 1
fi

if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    echo "ERROR: 未找到 C++ 编译器。请执行: sudo apt-get install build-essential"
    exit 1
fi

if ! pkg-config --exists Qt5Core 2>/dev/null && ! pkg-config --exists Qt6Core 2>/dev/null; then
    echo "ERROR: 未找到 Qt 开发包 (pkg-config 查不到 Qt5Core/Qt6Core)。"
    echo "  Ubuntu/Debian: sudo apt-get install qtbase5-dev qtbase5-dev-tools libqt5serialport5-dev"
    echo "  Fedora:        sudo dnf install qt5-qtbase-devel qt5-qtserialport-devel"
    echo "  Arch:          sudo pacman -S qt5-base qt5-serialport"
    exit 1
fi

# QtSerialPort is required by this project
if pkg-config --exists Qt5Core 2>/dev/null; then
    if ! pkg-config --exists Qt5SerialPort 2>/dev/null; then
        echo "WARNING: 未检测到 Qt5SerialPort 开发包，CMake 配置可能会失败。"
        echo "  请执行: sudo apt-get install libqt5serialport5-dev"
    fi
elif ! pkg-config --exists Qt6SerialPort 2>/dev/null; then
    echo "WARNING: 未检测到 Qt6SerialPort 开发包。"
    echo "  请执行: sudo apt-get install qt6-serialport-dev"
fi

DO_CLEAN=0
DO_RUN=0
for a in "$@"; do
    case "$a" in
        --clean) DO_CLEAN=1 ;;
        --run)   DO_RUN=1 ;;
    esac
done

# ---- optional clean ---------------------------------------------------
if [ "$DO_CLEAN" = "1" ]; then
    echo "--- 清理构建目录 ---"
    rm -rf "$BUILD_DIR"
fi

# ---- drop stale CMake cache pointing somewhere else -------------------
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cached="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
    cached="${cached%$'\r'}"
    if [ -n "$cached" ] && [ "$cached" != "$SRC_DIR" ]; then
        echo "检测到旧 CMake 缓存指向: $cached"
        echo "清理过期的构建目录..."
        rm -rf "$BUILD_DIR"
    fi
fi

if [ -d "$BUILD_DIR" ] && [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    # 目录存在但没有缓存（可能是之前 mkdir 出来、名字带 \r 的残留），清空重建
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# ---- configure --------------------------------------------------------
echo "--- CMake: $(cmake --version | head -n 1) ---"
echo "--- Configuring build ---"
# 兼容旧版 CMake (<3.13 不支持 -S/-B)，统一使用 cd 到 build 目录的写法
( cd "$BUILD_DIR" && cmake "$SRC_DIR" -DCMAKE_BUILD_TYPE=Release )
if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake 配置失败。"
    echo "源码目录: $SRC_DIR"
    echo "请确认该目录下存在 CMakeLists.txt:"
    ls -la "$SRC_DIR"
    exit 1
fi

# ---- build ------------------------------------------------------------
echo ""
echo "--- Building ---"
cmake --build "$BUILD_DIR" -- -j"$(nproc 2>/dev/null || echo 4)"
if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: 编译失败，请查看上方错误信息。"
    exit 1
fi

echo ""
echo "=============================================="
echo " Build complete!"
echo " Executable: $BUILD_DIR/strsvr"
echo "=============================================="

# ---- optional run -----------------------------------------------------
if [ "$DO_RUN" = "1" ]; then
    echo ""
    echo "--- Running STRSVR ---"
    if [ ! -f "$SRC_DIR/strsvr.ini" ] && [ -f "$SRC_DIR/strsvr.ini.example" ]; then
        cp "$SRC_DIR/strsvr.ini.example" "$SRC_DIR/strsvr.ini"
    fi
    cd "$SRC_DIR" && "$BUILD_DIR/strsvr"
fi
