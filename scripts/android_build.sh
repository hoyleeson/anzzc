#!/bin/bash

pushd ..

NDK_DIR=/home/$(exec whoami)/develop_tools/android_ndk/android-ndk-r10e
SYSROOT=$NDK_DIR/platforms/android-21/arch-arm/
TOOLCHAIN=$NDK_DIR/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86_64

INSTAL_DIR=$PWD/_android_install

if [ ! -e $INSTAL_DIR ]; then
	mkdir -p $INSTAL_DIR
fi

export CC="$TOOLCHAIN/bin/arm-linux-androideabi-gcc --sysroot=$SYSROOT"

./configure  --enable-debug --enable-verbose --prefix=$INSTAL_DIR --host=arm-linux-androideabi --with-platform=android

make

make install

popd
