#!/usr/bin/env bash
# ------------------
#     libpng
# ------------------
# $Id: libpng.sh 1902 2007-02-04 22:27:47Z ippei $
# Copyright (c) 2007, Ippei Ukai

# prepare

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100121.0 sg Script updated for 1.2.40
# 201005xx.0 hvdw Adapted for 1.2.43
# 20100624.0 hvdw More robust error checking on compilation
# 20100831.0 hvdw upgrade to 1.2.44
# 20120422.0 hvdw upgrade to 1.5.10
# 20120427.0 hvdw use gcc 4.6 for x86_64 for openmp compatibility on lion an up
# 20120430.0 hvdw downgrade from 1.5.10 to 1.4.11 as enblend's vigra  can't work with 1.5.10 even after patching
# -------------------------------

env \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-isysroot $MACSDKDIR -I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib -lz $LDARGS" \
 NEXT_ROOT="$MACSDKDIR" \
 ./configure --prefix="$REPOSITORYDIR" || fail "configure step"

make clean || fail "make clean step"
make $MAKEARGS || fail "make step"
make install || fail "make install";
