#!/bin/bash
URI=$1
VER=$2
OPT=$3
PKG=`basename $URI .git`

if [ -d $PKG ]
 then cd $PKG
 else git clone -q $URI -b $VER; cd $PKG; mkdir build
fi

cd build
[ -f Makefile ] || cmake -Wno-dev .. $OPT

cmake --build . --target install -- -j 4

