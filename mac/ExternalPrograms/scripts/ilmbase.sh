#!/usr/bin/env bash
# ------------------
#     openexr
# ------------------
# $Id: openexr.sh 2004 2007-05-11 00:17:50Z ippei $
# Copyright (c) 2007, Ippei Ukai


# prepare

# export REPOSITORYDIR="/PATH2HUGIN/mac/ExternalPrograms/repository" \
# ARCHS="ppc i386" \
# ppcTARGET="powerpc-apple-darwin7" \
#  ppcONLYARG="-mcpu=G3 -mtune=G4" \
#  ppcMACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
# i386TARGET="i386-apple-darwin8" \
#  i386MACSDKDIR="/Developer/SDKs/MacOSX10.3.9.sdk" \
#  i386ONLYARG="-mfpmath=sse -msse2 -mtune=pentium-m -ftree-vectorize" \
# OTHERARGs="";

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100624.0 hvdw More robust error checking on compilation
# 20121010.0 hvdw remove ppc and ppc64 part
# -------------------------------


$CXX "./Half/eLut.cpp" -o "./Half/eLut-native" -stdlib=libc++ -lc++
$CXX "./Half/toFloat.cpp" -o "./Half/toFloat-native" -stdlib=libc++ -lc++

[ -f "./Half/Makefile.in-original" ] || mv "./Half/Makefile.in" "./Half/Makefile.in-original"
sed -e 's/\.\/eLut/\.\/eLut-native/' \
    -e 's/\.\/toFloat/\.\/toFloat-native/' \
    "./Half/Makefile.in-original" > "./Half/Makefile.in"

env \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -stdlib=libc++ -lc++" \
 NEXT_ROOT="$MACSDKDIR" \
 PKG_CONFIG_PATH="$REPOSITORYDIR/lib/pkgconfig" \
 ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
 --enable-shared --enable-static=no --cache-file=./cache || fail "configure step";

make clean || fail "make clean step"
make $MAKEARGS all || fail "make step";
make install  || fail "make install step";
