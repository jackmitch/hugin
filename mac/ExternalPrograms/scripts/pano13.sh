#!/usr/bin/env bash
# ------------------
#     pano13
# ------------------
# $Id: pano13.sh 1904 2007-02-05 00:10:54Z ippei $
# Copyright (c) 2007, Ippei Ukai

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100113.0 sg Script adjusted for libpano13-2.9.15
# 20100117.0 sg Move code for detecting which version of pano13 to top for visibility
# 20100119.0 sg Support the SVN version of panotools - 2.9.16
# 201004xx.0 hvdw support 2.9.17
# 20100624.0 hvdw More robust error checking on compilation
# 20110107.0 hvdw support for 2.9.18
# 20110427.0 hvdw compile x86_64 with gcc 4.6 for openmp compatibility on lion and up
# 20121010.0 hvdw remove openmp stuff to make it easier to build
# -------------------------------



env \
  CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind" \
  NEXT_ROOT="$MACSDKDIR" \
  PKGCONFIG="$REPOSITORYDIR/lib" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
  --without-java \
  --with-zlib=/usr \
  --with-png="$REPOSITORYDIR" \
  --with-jpeg="$REPOSITORYDIR" \
  --with-tiff="$REPOSITORYDIR" \
  --enable-shared --enable-static=no || fail "configure step for $ARCH";


make clean || fail "make clean step"
make $MAKEARGS || fail "make step of $ARCH";
make install || fail "make install step of $ARCH";
