#!/usr/bin/env bash
# ------------------
#     libglew
# ------------------
# $Id: libglew.sh 1908 2007-02-05 14:59:45Z ippei $
# Copyright (c) 2007, Ippei Ukai

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100110.0 sg Script enhanced to copy dynamic lib also
# 20100624.0 hvdw More robust error checking on compilation
# 20120414.0 hvdw updated to 1.7
# 20120428.0 hvdw use gcc 4.6 for x86_64 for openmp compatibility on lion an up
# 20121010.0 hvdw update to 1.9; remove openmp stuff and place in openmp script
# -------------------------------


cd build

CC="$CC" CXX="$CXX" \
LDFLAGS="$LDARGS" \
cmake ./cmake -DCMAKE_INSTALL_PREFIX="$REPOSITORYDIR" -DBUILD_UTILS=OFF \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY_TARGET" -DCMAKE_OSX_SYSROOT="$MACSDKDIR" || fail "cmake step";

make || fail "make step";
make install || fail "make install step";

install_name_tool -id "$REPOSITORYDIR/lib/libGLEW.dylib" "$REPOSITORYDIR/lib/libGLEW.dylib"
