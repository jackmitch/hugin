#!/usr/bin/env bash
# ------------------
#     libffi
# ------------------
# $Id: libffi.sh 1902 2008-01-02 22:27:47Z Harry $
# Copyright (c) 2007, Ippei Ukai
# script skeleton Copyright (c) 2007, Ippei Ukai
# libffi specifics 2012, Harry van der Wolf


# prepare

# -------------------------------
# 20120411.0 HvdW Script tested
# 20121010.0 hvdw remove ppc stuff
# -------------------------------

# init

VERSION_NAME=$(basename "$PWD")

env \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR   $ARGS -O3" \
 CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS" \
 NEXT_ROOT="$MACSDKDIR" \
 ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
 --enable-static=no --enable-shared  || fail "configure step";

make clean || fail "make clean step"
make $MAKEARGS || fail "make step"
make install || fail "make install"


cp -rv "$REPOSITORYDIR/lib/$VERSION_NAME/" "$REPOSITORYDIR"
