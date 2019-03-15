#!/usr/bin/env bash

env \
  CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-I$REPOSITORYDIR/include -isysroot $MACSDKDIR" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind -stdlib=libc++ -lc++" \
  cmake -DCMAKE_INSTALL_PREFIX="$REPOSITORYDIR" -DDEPENDENCY_SEARCH_PREFIX="$REPOSITORYDIR" \
  -DCMAKE_OSX_SYSROOT="macosx${SDKVERSION}" -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY_TARGET" \
  -DWITH_OPENEXR=1 \
  -DFFTW3F_LIBRARY="$REPOSITORYDIR/lib" -DFFTW3F_INCLUDE_DIR="$REPOSITORYDIR/include" || fail "configure step"

make clean || fail "make clean step"
make $MAKEARGS || fail "make step"
make install || fail "make install step"