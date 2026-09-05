#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/swig-$SWIG_VERSION" "SWIG"

# create install directory 
mkdir -p $SWIG_INSTALL_DIR
echo $SWIG_INSTALL_DIR
# build and install
pushd swig-$SWIG_VERSION
source autogen.sh
cd ..
./configure --prefix=$SWIG_INSTALL_DIR
make -j 2
make install
popd

popd
