#!/bin/bash

pushd ..

aclocal -I m4
autoconf
autoheader
libtoolize --automake
automake --add-missing

popd

