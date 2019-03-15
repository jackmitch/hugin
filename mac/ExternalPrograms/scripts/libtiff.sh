#!/usr/bin/env bash
# ------------------
#     libtiff
# ------------------
# $Id: libtiff.sh 1902 2007-02-04 22:27:47Z ippei $
# Copyright (c) 2007, Ippei Ukai


# prepare

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100121.0 sg Script updated for 3.9.2
# 20100624.0 hvdw More robust error checking on compilation
# 20121010.0 hvdw Update to 4.03
# 20130131.0 hvdw Temporarily back to 3.94 as 4.03 gives weird UINT64 errors
# -------------------------------

env \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib $LDFLAGS -stdlib=libc++ -lc++" \
 NEXT_ROOT="$MACSDKDIR" \
 ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
   --enable-static=no --enable-shared --with-apple-opengl-framework --without-x \
   || fail "configure step for $ARCH" ;

make clean || fail "make clean step"
make $MAKEARGS || fail "failed at make step of $ARCH";
make install || fail "make install step of $ARCH";


cd "$REPOSITORYDIR/include"
patch -p1 -N < ../../patches/tiff.h.patch
