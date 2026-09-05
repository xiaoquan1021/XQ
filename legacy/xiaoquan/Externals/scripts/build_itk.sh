#!/bin/bash

pushd $SRC_DIR

ITK_MAJOR_VERSION=${ITK_VERSION%.*}
source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/InsightToolkit-$ITK_VERSION" "ITK"

# create install directory 
mkdir -p $ITK_INSTALL_DIR

pushd InsightToolkit-$ITK_VERSION
# rm -r -p build
mkdir -p build
cd build

# this option makes compilation fail
# -DBUILD_SHARED_LIBS=1 \

export PATH=$PYTHON_INSTALL_DIR/bin:$PATH

cmake \
    -DBUILD_SHARED_LIBS=1 \
    -DCMAKE_INSTALL_PREFIX=$ITK_INSTALL_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=0 \
    -DBUILD_TESTING=0 \
    -DITK_USE_SYSTEM_GDCM=1 \
    -DITK_USE_SYSTEM_HDF5=1 \
    -DModule_ITKReview=1 \
    -DModule_ITKVtkGlue=1 \
    -DModule_IsotropicWavelets:BOOL=ON \
    -DModule_GrowCut:BOOL=ON \
    -DModule_Bonemorphometry:BOOL=ON \
    -DModule_Thickness3D:BOOL=ON \
    -DModule_MorphologicalContourInterpolation:BOOL=ON \
    -DModule_RLEImage:BOOL=ON \
    -DModule_ITKReview:BOOL=ON \
    -DGDCM_DIR=$GDCM_CMAKE_DIR \
    -DHDF5_DIR=$HDF5_CMAKE_DIR \
    -DVTK_DIR=$VTK_CMAKE_DIR \
    -DQt6_DIR=$QT_INSTALL_DIR_CMAKE \
    -DITK_USE_SYSTEM_ZLIB:BOOL=ON \
    -DPython3_ROOT_DIR=$PYTHON_INSTALL_DIR \
    -DPython3_INCLUDE_DIR=$PYTHON_INSTALL_DIR/$PYTHON_INCLUDE_DIR \
    -DPython3_LIBRARY=$PYTHON_INSTALL_DIR/$PYTHON_LIBRARY \
    -DPython3_EXECUTABLE=$PYTHON_INSTALL_DIR/$PYTHON_EXECUTABLE \
..
make -j 2
make install
popd

popd
