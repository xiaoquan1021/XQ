#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/hdf5-$HDF5_VERSION" "HDF5"

# create install directory 
mkdir -p $HDF5_INSTALL_DIR

# build and install
pushd hdf5-$HDF5_VERSION
mkdir -p build && cd build
cmake \
    -DHDF5_BUILD_SHARED_LIBS=1 \
    -DHDF5_BUILD_HL_LIB:BOOL=ON \
    -DHDF5_BUILD_CPP_LIB:BOOL=ON \
    -DCMAKE_INSTALL_PREFIX=$HDF5_INSTALL_DIR \
    -DCMAKE_BUILD_TYPE=Release \
..
make -j 2
make install
popd

popd
