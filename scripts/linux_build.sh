#!/bin/bash

pushd ..

INSTAL_DIR=$PWD/_install

if [ -e $INSTAL_DIR ]; then
	mkdir -p $INSTAL_DIR
fi

./configure --enable-debug --enable-verbose --prefix=$INSTAL_DIR 

make
make install

popd

