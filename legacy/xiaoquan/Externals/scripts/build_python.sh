#!/bin/bash

pushd $SRC_DIR

source "$ROOT_DIR/scripts/source_helpers.sh"
require_source_dir "$SRC_DIR/Python-$PYTHON_VERSION" "Python"

# create install directory 
mkdir -p $PYTHON_INSTALL_DIR
echo $PYTHON_INSTALL_DIR
# build and install
pushd Python-$PYTHON_VERSION

./configure --with-threads --enable-shared --prefix=$PYTHON_INSTALL_DIR
make -j 2 && make install
popd

$PYTHON_INSTALL_DIR/bin/pip3 install numpy
# we need a python interpreter in the bin directory
cp $PYTHON_INSTALL_DIR/bin/python3 $PYTHON_INSTALL_DIR/bin/python

popd

echo "#!/bin/bash -f" > $PYTHON_INSTALL_DIR/bin/svpython
echo "if [ -z \${PYTHONPATH+x} ]; then" >> $PYTHON_INSTALL_DIR/bin/svpython 
echo "  OLDPYTHONPATH=\"\"" >> $PYTHON_INSTALL_DIR/bin/svpython 
echo "else" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  OLDPYTHONPATH=\$PYTHONPATH" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  OLDPYTHONPATH=\${PYTHONPATH//;/:/}" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "fi" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "export PYTHONHOME=$PYTHON_INSTALL_DIR" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "export LD_LIBRARY_PATH=\$PYTHONHOME/lib:\$LD_LIBRARY_PATH" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "export PYTHONPATH=$PYTHON_INSTALL_DIR/lib/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION/lib-dynload:$PYTHON_INSTALL_DIR/lib:$PYTHON_INSTALL_DIR/lib/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION:$PYTHON_INSTALL_DIR/lib/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION/site-packages:\$OLDPYTHONPATH" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "if [ \"\$#\" -eq 0 ]; then" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  $PYTHON_INSTALL_DIR/bin/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION " >> $PYTHON_INSTALL_DIR/bin/svpython
echo "elif [ \"\$#\" -eq 1 ]; then" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  $PYTHON_INSTALL_DIR/bin/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION \"\$1\" " >> $PYTHON_INSTALL_DIR/bin/svpython
echo "elif [ \"\$#\" -eq 2 ]; then" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  $PYTHON_INSTALL_DIR/bin/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION \"\$1\" \"\$2\" " >> $PYTHON_INSTALL_DIR/bin/svpython
echo "elif [ \"\$#\" -eq 3 ]; then" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  $PYTHON_INSTALL_DIR/bin/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION \"\$1\" \"\$2\" \"\$3\" " >> $PYTHON_INSTALL_DIR/bin/svpython
echo "else" >> $PYTHON_INSTALL_DIR/bin/svpython
echo "  $PYTHON_INSTALL_DIR/bin/python$PYTHON_MAJOR_VERSION.$PYTHON_MINOR_VERSION \"\$1\" \"\$2\" \"\$3\" \"\${@:4}\" " >> $PYTHON_INSTALL_DIR/bin/svpython
echo "fi" >> $PYTHON_INSTALL_DIR/bin/svpython
chmod u+w,a+rx $PYTHON_INSTALL_DIR/bin/svpython

mkdir $PYTHON_INSTALL_DIR/bin-relocate

for f in $PYTHON_INSTALL_DIR/bin/*; do
    shortf="${f##*/}"
    echo "File -> $f"
    sed -e "s+$PYTHON_INSTALL_DIR+/usr/local/sv/svpython+g;s+#!/usr/local/sv/svpython/bin/python+#!/usr/local/sv/svpython/bin/svpython+g" $f > $PYTHON_INSTALL_DIR/bin-relocate/$shortf
done

# sed destroys the actual python executable, need to overwrite with original
rm -f $PYTHON_INSTALL_DIR/bin-relocate/python
rm -f $PYTHON_INSTALL_DIR/bin-relocate/python3*
cp -f $PYTHON_INSTALL_DIR/bin/python $PYTHON_INSTALL_DIR/bin-relocate/ 
cp -f $PYTHON_INSTALL_DIR/bin/python3* $PYTHON_INSTALL_DIR/bin-relocate/
