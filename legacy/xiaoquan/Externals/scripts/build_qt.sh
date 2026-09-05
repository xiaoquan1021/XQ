#!/bin/bash

pushd $SRC_DIR

QT_MAJOR_MINOR=${QT_VERSION%.*} # 获取 6.7
source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/qt-everywhere-src-$QT_VERSION" "Qt"

# 创建安装目录 
mkdir -p $QT_INSTALL_DIR

pushd qt-everywhere-src-$QT_VERSION

# 注意：Qt 6 不再需要旧的补丁
# 也不需要 -skip qtwebengine，相反我们要确保它被构建

# 配置 Qt
# -xcb: 强制启用 XCB 支持 (SimVascular/MITK 在 Linux 上需要)
# -submodules qtwebengine: 明确包含 WebEngine (默认也会包含，但这保证它不会被静默跳过)
# -release: 编译 Release 版本
# Qt6 默认使用 CMake 和 Ninja (如果已安装)

./configure \
    -opensource -confirm-license \
    -prefix $QT_INSTALL_DIR \
    -release \
    -xcb \
    -nomake examples \
    -nomake tests \
    -submodules qtwebengine

# 编译与安装 (使用 cmake 命令)
cmake --build . --parallel 4
cmake --install .

popd

popd
