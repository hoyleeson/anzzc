#!/bin/bash

rm -rf autom4te.cache

aclocal -I m4
autoconf
autoheader
libtoolize --automake
automake --add-missing


