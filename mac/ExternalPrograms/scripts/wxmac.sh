#!/usr/bin/env bash
# ------------------
#     wxMac 3.0
# ------------------
# $Id: wxmac29.sh 1902 2007-02-04 22:27:47Z ippei $
# Copyright (c) 2007-2008, Ippei Ukai

# 2009-12-04.0 Remove unneeded arguments to make and make install; made make single threaded


# prepare

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
#               Works Intel: 10.5, 10.6 & Powerpc 10.4, 10.5
# 20100624.0 hvdw More robust error checking on compilation
# 20120415.0 hvdw upgrade to 2.8.12
# 20120526.0 hvdw upgrade to 2.9.3 and rename to wxmac29.sh
# 20121010.0 hvdw upgrade to 2.9.4
# 20130205.0 hvdw temporarily downgrade to 2.9.3 again
# -------------------------------

env \
  CC=clang CXX=clang++ \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-isysroot $MACSDKDIR -I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind -stdlib=libc++" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
    --with-macosx-sdk="$MACSDKDIR" --with-macosx-version-min="$DEPLOY_TARGET" \
    --enable-monolithic --enable-unicode --with-opengl --disable-graphics_ctx \
    --with-libjpeg --with-libtiff --with-libpng --with-zlib \
    --with-cocoa --without-sdl --disable-sdltest --disable-mediactrl \
    --enable-shared --disable-debug --enable-aui || fail "configure step";

mkdir build && cd build

make clean || fail "make clean step"

make $MAKEARGS || fail "make step";
make install || fail "make install step";