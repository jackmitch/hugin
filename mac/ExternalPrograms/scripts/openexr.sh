#!/usr/bin/env bash
# ------------------
#     openexr
# ------------------
# $Id: openexr.sh 2004 2007-05-11 00:17:50Z ippei $
# Copyright (c) 2007, Ippei Ukai


# prepare

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100624.0 hvdw More robust error checking on compilation
# 201204013.0 hvdw update openexr to 17, e.g. rename script
# 20121010.0 hvdw remove ppc and ppc64 stuff
# -------------------------------

$CXX -DHAVE_CONFIG_H -I./IlmImf -I./config \
  -I"$REPOSITORYDIR/include/OpenEXR" -D_THREAD_SAFE \
  -I. -I./config  -I"$REPOSITORYDIR/include" \
  -I/usr/include $ARGS \
  -mmacosx-version-min="$DEPLOY_TARGET" -O3  -stdlib=libc++ -lc++ -L"$REPOSITORYDIR/lib" -lHalf \
  -o "./IlmImf/b44ExpLogTable-native" ./IlmImf/b44ExpLogTable.cpp

 if [ -f "./IlmImf/b44ExpLogTable-native" ] ; then
  echo "Created b44ExpLogTable-native"
 else
  echo " Error Failed to create b44ExpLogTable-native"
  exit 1
 fi

 if [ -f "./IlmImf/Makefile.in-original" ]; then
  echo "original already exists!";
 else
  mv "./IlmImf/Makefile.in" "./IlmImf/Makefile.in-original"
 fi
 sed -e 's/\.\/b44ExpLogTable/\.\/b44ExpLogTable-native/' \
    "./IlmImf/Makefile.in-original" > "./IlmImf/Makefile.in"


env \
  CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS -prebind -stdlib=libc++ -lc++" \
  NEXT_ROOT="$MACSDKDIR" \
  PKG_CONFIG_PATH="$REPOSITORYDIR/lib/pkgconfig" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
    --enable-shared --enable-static=no || fail "configure step ";

make clean || fail "make clean step"
make $MAKEARGS all || fail "make step";
make install || fail "make install step";
