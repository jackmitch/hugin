#!/usr/bin/env bash
# ------------------
#     libexiv2
# ------------------
# $Id: $
# Copyright (c) 2008, Ippei Ukai


# prepare

# export REPOSITORYDIR="/PATH2HUGIN/mac/ExternalPrograms/repository" \
# ARCHS="ppc i386" \
#  ppcTARGET="powerpc-apple-darwin8" \
#  i386TARGET="i386-apple-darwin8" \
#  ppcMACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  i386MACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  ppcONLYARG="-mcpu=G3 -mtune=G4" \
#  i386ONLYARG="-mfpmath=sse -msse2 -mtune=pentium-m -ftree-vectorize" \
#  OTHERARGs="";

# -------------------------------
# 20091206.0 sg Script NOT tested but uses std boilerplate
# 20100111.0 sg Script tested for building dylib
# 20100121.0 sg Script updated for 0.19
# 20100624.0 hvdw More robust error checking on compilation
# 20120414.0 hvdw update to 0.22
# 20121010.0 hvdw update to 0.23
# -------------------------------

env \
 PATH="$REPOSITORYDIR/bin:$PATH" \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind -stdlib=libc++ -lc++" \
 PKG_CONFIG_PATH="$REPOSITORYDIR/lib/pkgconfig" \
 NEXT_ROOT="$MACSDKDIR" \
 ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
 --enable-shared --with-libintl-prefix="$REPOSITORYDIR" \
 --disable-lensdata --disable-commercial \
 --enable-static=no || fail "configure step for $ARCH";

make clean

make $MAKEARGS || fail "make step of $ARCH";
make install || fail "make install step of $ARCH";
