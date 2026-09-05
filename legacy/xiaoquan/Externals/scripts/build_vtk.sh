#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/VTK-$VTK_VERSION" "VTK"

# create install directory 
mkdir -p $VTK_INSTALL_DIR

pushd VTK-$VTK_VERSION
mkdir -p build
cd build

cmake \
    -DCMAKE_INSTALL_PREFIX=$VTK_INSTALL_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=0 \
    -DBUILD_SHARED_LIBS=1 \
    -DVTK_WRAP_PYTHON=1 \
    -DVTK_QT_VERSION=6 \
    -DPython3_EXECUTABLE:FILEPATH=$PYTHON_INSTALL_DIR/$PYTHON_EXECUTABLE \
    -DPython3_INCLUDE_DIR:FILEPATH=$PYTHON_INSTALL_DIR/$PYTHON_INCLUDE_DIR \
    -DPython3_LIBRARY:FILEPATH=$PYTHON_INSTALL_DIR/$PYTHON_LIBRARY \
    -DQt6Core_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Core \
    -DQt6Gui_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Gui \
    -DQt6OpenGL_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6OpenGL \
    -DQt6Widgets_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Widgets \
    -DQt6_DIR=$QT_INSTALL_DIR_CMAKE \
    -DVTK_MODULE_ENABLE_VTK_GUISupportQt=YES \
    -DVTK_MODULE_ENABLE_VTK_ViewsQt=YES \
    -DVTK_MODULE_ENABLE_VTK_RenderingQt=YES \
    ..

make -j 2
make install
popd

popd
