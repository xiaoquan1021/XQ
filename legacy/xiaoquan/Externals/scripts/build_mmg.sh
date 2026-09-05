#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/mmg-$MMG_VERSION" "MMG"

# create install directory 
mkdir -p $MMG_INSTALL_DIR
echo $MMG_INSTALL_DIR
# build and install
pushd mmg-$MMG_VERSION
mkdir -p build && cd build
cmake \
    -DCMAKE_INSTALL_PREFIX=$MMG_INSTALL_DIR \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DLIBMMG2D_SHARED=0 \
    -DLIBMMG3D_SHARED=0 \
    -DLIBMMGS_SHARED=0 \
    -DLIBMMG_SHARED=0 \
    -DCMAKE_BUILD_TYPE=Release \
    ..
make -j 2 && make install
popd

popd
