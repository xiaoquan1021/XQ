#!/bin/bash

set -e

# ==============================================================================
# 1. 环境变量与版本设置
# ==============================================================================
export MITK_VERSION=2024.06

# 确保 ROOT_DIR 已定义，如果没有则默认为当前目录的上级
if [ -z "$ROOT_DIR" ]; then
    export ROOT_DIR=$(pwd)/..
fi

# 定义源码和构建路径
MITK_SRC_DIR=$SRC_DIR/MITK-$MITK_VERSION
MITK_BUILD_DIR=$MITK_SRC_DIR/build

echo "Building MITK $MITK_VERSION with Qt 6..."

pushd $SRC_DIR

# 导出必要的路径 (Qt6, Python, TinyXML2, GDCM, VTK)
export PATH=$QT_INSTALL_DIR_CMAKE:$PATH
export PATH=$PYTHON_INSTALL_DIR/bin:$PATH
export PATH=$TINYXML2_INSTALL_DIR_CMAKE:$PATH
export PATH=$TINYXML2_INSTALL_DIR/lib:$PATH
export PATH=$TINYXML2_INSTALL_DIR/include:$PATH
export PATH=$GDCM_INCLUDE_DIR:$PATH
export PATH=$GDCM_LIB_DIR:$PATH
export PATH=$VTK_INSTALL_DIR/lib:$PATH

# 设置库加载路径 (LD_LIBRARY_PATH) - 包含 ITK 以修复安装时的 ldd 错误
export LD_LIBRARY_PATH=$ITK_INSTALL_DIR/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$VTK_INSTALL_DIR/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$GDCM_LIB_DIR:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$MITK_BUILD_DIR/MITK-build/lib:$LD_LIBRARY_PATH

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$MITK_SRC_DIR" "MITK"

# 创建安装目录
mkdir -p $MITK_INSTALL_DIR

pushd MITK-$MITK_VERSION

mkdir -p build
cd build

# ==============================================================================
# 3. CMake 配置
# ==============================================================================

CMAKE_C_FLAGS=""
CMAKE_CXX_FLAGS=""
if [[ $(uname) == 'Linux' ]]; then
    CMAKE_C_FLAGS="-I$GDCM_INCLUDE_DIR -Wl,-rpath=$GDCM_LIB_DIR"
    CMAKE_CXX_FLAGS=$CMAKE_C_FLAGS
fi

echo "Configuring CMake..."
cmake \
    -DMITK_USE_SUPERBUILD=1 \
    -DMITK_USE_GDCM=1 \
    -DMITK_BUILD_EXAMPLES=0 \
    -DBUILD_TESTING=0 \
    -DMITK_USE_Python3=1 \
    -DMITK_USE_SWIG=1 \
    -DBUILD_SHARED_LIBS=1 \
    -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
    -DEXTERNAL_GDCM_DIR:PATH=$GDCM_INSTALL_DIR \
    -DEXTERNAL_ITK_DIR:PATH=$ITK_INSTALL_DIR \
    -DEXTERNAL_VTK_DIR:PATH=$VTK_INSTALL_DIR \
    -DSWIG_EXECUTABLE:PATH=$SWIG_EXECUTABLE \
    -DSWIG_DIR:PATH=$SWIG_INSTALL_DIR \
    -D_Python3_EXECUTABLE:PATH=$PYTHON_INSTALL_DIR/$PYTHON_EXECUTABLE \
    -D_Python3_INCLUDE_DIR:PATH=$PYTHON_INSTALL_DIR/$PYTHON_INCLUDE_DIR \
    -DMITK_USE_Qt6=ON \
    -DQt6_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6 \
    -DQt6Concurrent_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Concurrent \
    -DQt6Core_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Core \
    -DQt6Gui_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Gui \
    -DQt6Help_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Help \
    -DQt6Network_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Network \
    -DQt6OpenGL_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6OpenGL \
    -DQt6PrintSupport_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6PrintSupport \
    -DQt6Sql_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Sql \
    -DQt6Svg_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Svg \
    -DQt6UiTools_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6UiTools \
    -DQt6WebEngineWidgets_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6WebEngineWidgets \
    -DQt6Widgets_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Widgets \
    -DQt6XmlPatterns_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6XmlPatterns \
    -DQt6Xml_DIR:PATH=$QT_INSTALL_DIR/lib/cmake/Qt6Xml \
    -DCMAKE_INSTALL_PREFIX:PATH=$MITK_INSTALL_DIR \
    -DCMAKE_BUILD_TYPE:STRING=Release \
    -DCMAKE_OBJECT_PATH_MAX:STRING=1000 \
    -DPython3_ROOT_DIR=$PYTHON_INSTALL_DIR \
    -DCMAKE_PREFIX_PATH:PATH="$ITK_INSTALL_DIR_CMAKE;$QT_INSTALL_DIR/lib/cmake" \
    -DCMAKE_C_FLAGS="$CMAKE_C_FLAGS" \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
..

# ==============================================================================
# 4. 编译与安装 (Build & Install)
# ==============================================================================
echo "Compiling..."
make -j 4

echo "Installing..."
make install

popd # 退出 build 目录
popd # 退出 MITK 源码目录

echo 'MITK Core Build and Install Finished.'

# ==============================================================================
# 5. 后处理与整理 (Post-Install / Organization)
#    整合原 post-install-mitk.sh 逻辑，并修复已知错误
# ==============================================================================
echo "Starting Post-Install steps..."

# 工具定义
GCP="rsync -azh --ignore-missing-args"
GMKDIR="mkdir -p"
GRM="rm -rf"

# 路径定义 (复用之前的变量以保证一致性)
MITK_BINDIR=$MITK_INSTALL_DIR
MITK_BLDDIR=$MITK_BUILD_DIR

# 创建必要的目录结构
$GMKDIR $MITK_BINDIR/bin
$GMKDIR $MITK_BINDIR/lib
$GMKDIR $MITK_BINDIR/lib/plugins
$GMKDIR $MITK_BINDIR/include

# 复制基础构建产物
echo "Copying MITK artifacts..."
$GCP $MITK_BLDDIR/MITK-build/bin/ $MITK_BINDIR/bin/
$GCP $MITK_BLDDIR/MITK-build/lib/ $MITK_BINDIR/lib/
$GCP $MITK_BLDDIR/ep/bin/ $MITK_BINDIR/bin/
$GCP $MITK_BLDDIR/ep/lib/ $MITK_BINDIR/lib/
$GCP $MITK_BLDDIR/ep/include/ $MITK_BINDIR/include/
$GCP $MITK_BLDDIR/ep/share/ $MITK_BINDIR/share/

# 复制 CTK 相关 (跳过不存在的 CMakeExternals)
echo "Copying CTK artifacts..."
$GCP $MITK_BLDDIR/ep/src/CTK-build/qRestAPI-build/*.so* $MITK_BINDIR/lib/
$GCP $MITK_BLDDIR/ep/src/CTK-build/qRestAPI-build/*.h $MITK_BINDIR/include/
$GCP $MITK_BLDDIR/ep/src/CTK-build/CTK-build/bin/* $MITK_BINDIR/bin/
# 注意：Release 模式下可能没有 Release 子目录，直接通配符尝试
$GCP $MITK_BLDDIR/ep/src/CTK-build/CTK-build/bin/Release/*CTK*.so* $MITK_BINDIR/lib/ 2>/dev/null || true
$GCP $MITK_BLDDIR/ep/src/CTK-build/CTK-build/bin/*CTK*.so* $MITK_BINDIR/lib/ 2>/dev/null || true

# 复制 qRestAPI 头文件
$GMKDIR $MITK_BINDIR/include/qRestAPI
$GCP $MITK_BLDDIR/ep/src/CTK-build/qRestAPI/*.h $MITK_BINDIR/include/qRestAPI/

# 清理 bin 目录下的库文件 (通常应该在 lib)
$GRM $MITK_BINDIR/bin/*.so*
$GRM $MITK_BINDIR/bin/plugins

# 复制 CTK 头文件
echo "Copying CTK headers..."
$GMKDIR $MITK_BINDIR/include/ctk
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Core/*.h $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Core/*.tpp $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Scripting/Python/Core/*.h $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Scripting/Python/Widgets/*.h $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Visualization/VTK/Core/*.h $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/Widgets/*.h $MITK_BINDIR/include/ctk/
$GCP $MITK_BLDDIR/ep/src/CTK/Libs/PluginFramework $MITK_BINDIR/include/ctk/

# 查找并复制 Export 头文件
find $MITK_BLDDIR/ep/src/CTK-build -name "*Export.h" -exec cp {} $MITK_BINDIR/include/ctk/ \;

# 复制 MITK 头文件和结构
echo "Organizing MITK headers..."
$GMKDIR $MITK_BINDIR/include/mitk
$GMKDIR $MITK_BINDIR/include/mitk/configs
$GMKDIR $MITK_BINDIR/include/mitk/exports
$GMKDIR $MITK_BINDIR/include/mitk/ui_files
$GMKDIR $MITK_BINDIR/include/mitk/Modules

$GCP $MITK_BLDDIR/MITK-build/*.h $MITK_BINDIR/include/mitk/

# 复制 Modules 头文件
# 使用循环处理源码目录中的 Modules
for dir in $MITK_SRC_DIR/Modules/*; do
    if [ -d "$dir/include" ]; then
        module_name=$(basename "$dir")
        $GMKDIR $MITK_BINDIR/include/mitk/$module_name
        $GCP $dir/include/ $MITK_BINDIR/include/mitk/$module_name/
        
        # 尝试复制构建目录中生成的 ui_*.h 文件
        if [ -d "$MITK_BLDDIR/MITK-build/Modules/$module_name" ]; then
             $GCP $MITK_BLDDIR/MITK-build/Modules/$module_name/ui_*.h $MITK_BINDIR/include/mitk/$module_name/ 2>/dev/null || true
        fi
    fi
done

# 复制 Plugins 头文件
echo "Organizing Plugin headers..."
for plugin_src in $MITK_SRC_DIR/Plugins/org.mitk.*/src $MITK_SRC_DIR/Plugins/org.blueberry.*/src; do
    if [ -d "$plugin_src" ]; then
        plugin_name=$(basename $(dirname "$plugin_src"))
        target_dir=$MITK_BINDIR/include/mitk/plugins/$plugin_name
        $GMKDIR $target_dir
        $GCP $plugin_src/*.h $target_dir/ 2>/dev/null || true
        
        # 处理子目录
        for subdir in $plugin_src/*; do
             if [ -d "$subdir" ]; then
                subname=$(basename "$subdir")
                $GMKDIR $target_dir/$subname
                $GCP $subdir/*.h $target_dir/$subname/ 2>/dev/null || true
             fi
        done
    fi
done

# 收集所有的 Export.h 和 ui_*.h 和 Config.h
find $MITK_BLDDIR -name "*Exports.h" -exec cp {} $MITK_BINDIR/include/mitk/exports/ \;
find $MITK_BLDDIR/MITK-build/Modules -name "*Export.h" -exec cp {} $MITK_BINDIR/include/mitk/exports/ \; 2>/dev/null || true
find $MITK_BLDDIR/MITK-build/Plugins -name "*Export.h" -exec cp {} $MITK_BINDIR/include/mitk/exports/ \; 2>/dev/null || true
find $MITK_BLDDIR/MITK-build -name "ui_*.h" -exec cp {} $MITK_BINDIR/include/mitk/ui_files/ \; 2>/dev/null || true
find $MITK_BLDDIR/MITK-build -name "*Config.h" -exec cp {} $MITK_BINDIR/include/mitk/configs/ \; 2>/dev/null || true

# 手动复制特定模块 (SimVascular 特别依赖的)
echo "Copying specific module headers..."
# 创建目录
$GMKDIR $MITK_BINDIR/include/mitk/Modules/ContourModel
$GMKDIR $MITK_BINDIR/include/mitk/Modules/Segmentation
$GMKDIR $MITK_BINDIR/include/mitk/Modules/SegmentationUI
$GMKDIR $MITK_BINDIR/include/mitk/Modules/SurfaceInterpolation

# 复制源码
$GCP $MITK_SRC_DIR/Modules/ContourModel/DataManagement/*.h $MITK_BINDIR/include/mitk/Modules/ContourModel/ 2>/dev/null || true
$GCP $MITK_SRC_DIR/Modules/Segmentation/Algorithms/*.h $MITK_BINDIR/include/mitk/Modules/Segmentation/ 2>/dev/null || true
$GCP $MITK_SRC_DIR/Modules/Segmentation/Controllers/*.h $MITK_BINDIR/include/mitk/Modules/Segmentation/ 2>/dev/null || true
$GCP $MITK_SRC_DIR/Modules/Segmentation/Interactions/*.h $MITK_BINDIR/include/mitk/Modules/Segmentation/ 2>/dev/null || true
$GCP $MITK_SRC_DIR/Modules/SegmentationUI/Qmitk/*.h $MITK_BINDIR/include/mitk/Modules/SegmentationUI/ 2>/dev/null || true
$GCP $MITK_SRC_DIR/Modules/SurfaceInterpolation/*.h $MITK_BINDIR/include/mitk/Modules/SurfaceInterpolation/ 2>/dev/null || true

# 复制可执行文件
echo "Copying executables..."
$GCP $MITK_BLDDIR/MITK-build/bin/MitkWorkbench* $MITK_BINDIR/bin/ 2>/dev/null || true
$GCP $MITK_BLDDIR/MITK-build/bin/usResourceCompiler* $MITK_BINDIR/bin/ 2>/dev/null || true
$GCP $MITK_BLDDIR/MITK-build/bin/MitkPluginGenerator* $MITK_BINDIR/bin/ 2>/dev/null || true

# 创建 Wrapper 脚本
echo "Creating workbench wrapper..."
WRAPPER_SCRIPT=${MITK_INSTALL_DIR}/bin/workbench-wrapper
echo "#!/bin/sh -f" > $WRAPPER_SCRIPT
echo "export LD_LIBRARY_PATH=${MITK_INSTALL_DIR}/lib:\$LD_LIBRARY_PATH" >> $WRAPPER_SCRIPT
echo "export LD_LIBRARY_PATH=${MITK_INSTALL_DIR}/bin:\$LD_LIBRARY_PATH" >> $WRAPPER_SCRIPT
echo "export PYTHONHOME=${PYTHON_INSTALL_DIR}" >> $WRAPPER_SCRIPT
echo "export PYTHONPATH=${PYTHON_INSTALL_DIR}/lib/python${PYTHON_MAJOR_VERSION}.${PYTHON_MINOR_VERSION}/site-packages" >> $WRAPPER_SCRIPT
echo "if [ \"\$#\" -gt 0 ]; then" >> $WRAPPER_SCRIPT
echo "  ${MITK_INSTALL_DIR}/bin/MitkWorkbench \"\$@\"" >> $WRAPPER_SCRIPT
echo "else" >> $WRAPPER_SCRIPT
echo "  ${MITK_INSTALL_DIR}/bin/MitkWorkbench" >> $WRAPPER_SCRIPT
echo "fi" >> $WRAPPER_SCRIPT

chmod +x $WRAPPER_SCRIPT

echo "Build and Post-Install steps completed successfully!"
