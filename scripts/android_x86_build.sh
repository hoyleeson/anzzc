#!/bin/bash

pushd ..

NDK_DIR=/home/$(exec whoami)/develop_tools/android_ndk/android-ndk-r10e
SYSROOT=$NDK_DIR/platforms/android-21/arch-x86/
TOOLCHAIN=$NDK_DIR/toolchains/x86-4.8/prebuilt/linux-x86_64

export CC="$TOOLCHAIN/bin/i686-linux-android-gcc --sysroot=$SYSROOT"

INSTAL_DIR=$PWD/_android_x86_install

if [ -e $INSTAL_DIR ]; then
	mkdir -p $INSTAL_DIR
fi

./configure  --enable-debug --enable-verbose --prefix=$INSTAL_DIR --host=i686-linux-androideabi --with-platform=android

make

make install

popd
