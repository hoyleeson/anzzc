#!/bin/bash

pushd ..

INSTAL_DIR=$PWD/_install

aclocal -I m4
autoconf
autoheader
libtoolize --automake
automake --add-missing

if [ ! -e $INSTAL_DIR ]; then
	mkdir -p $INSTAL_DIR
fi

./configure --prefix=$INSTAL_DIR --enable-debug 
#--enable-verbose

make
make install
make dist

popd
