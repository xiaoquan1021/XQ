#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/tinyxml2-$TINYXML2_VERSION" "TinyXML2"

# create install directory 
mkdir -p $TINYXML2_INSTALL_DIR

# build and install
pushd tinyxml2-$TINYXML2_VERSION
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=$TINYXML2_INSTALL_DIR ..
make -j 2 && make install
popd

popd
