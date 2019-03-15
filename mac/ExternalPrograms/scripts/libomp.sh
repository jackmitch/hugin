#!/usr/bin/env bash


mkdir -p "$REPOSITORYDIR/bin";
mkdir -p "$REPOSITORYDIR/lib";
mkdir -p "$REPOSITORYDIR/include";

env \
CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
CPPFLAGS="-isysroot $MACSDKDIR -I$REPOSITORYDIR/include" \
LDFLAGS="-L$REPOSITORYDIR/lib" \
cmake -DCMAKE_INSTALL_PREFIX="$REPOSITORYDIR" \
-DCMAKE_OSX_SYSROOT="macosx${SDKVERSION}" -DCMAKE_OSX_DEPLOYMENT_TARGET="$OSVERSION" \
-DLIBOMP_ARCH="32e" || fail "configure step for $ARCH";

make $MAKEARGS || fail "failed at make step of $ARCH";
make install || fail "make install step of $ARCH";

install_name_tool -id "$REPOSITORYDIR/lib/libiomp5.dylib" "$REPOSITORYDIR/lib/libiomp5.dylib"
ln -sf "libiomp5.dylib" "$REPOSITORYDIR/lib/libomp.dylib"
