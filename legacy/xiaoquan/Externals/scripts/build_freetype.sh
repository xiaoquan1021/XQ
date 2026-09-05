#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/freetype-$FREETYPE_VERSION" "FreeType"

# create install directory 
mkdir -p $FREETYPE_INSTALL_DIR
echo $FREETYPE_INSTALL_DIR
# build and install
pushd freetype-$FREETYPE_VERSION
./configure --prefix=$FREETYPE_INSTALL_DIR
make -j 2 && make install
popd

popd
