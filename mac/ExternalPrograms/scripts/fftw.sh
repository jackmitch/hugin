#!/usr/bin/env bash

env \
  CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind" \
  NEXT_ROOT="$MACSDKDIR" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking || fail "configure step"

make $MAKEARGS || fail "make step"
make install || fail "make install step"