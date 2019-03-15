#!/usr/bin/env bash
# ------------------
#     libglib2
# ------------------
# $Id: libglib2.sh 1902 2008-01-02 22:27:47Z Harry $
# Copyright (c) 2007, Ippei Ukai
# script skeleton Copyright (c) 2007, Ippei Ukai
# libglib2 specifics 2012, Harry van der Wolf


# prepare

# -------------------------------
# 20120411.0 HvdW Script tested
# 20121010.0 hvdw remove openmp stuff
# -------------------------------

# init


export MSGFMT="$REPOSITORYDIR/bin/msgfmt"

env \
 CC="$CC" CXX="$CXX" \
 CFLAGS="-isysroot $MACSDKDIR $ARGS   -O3" \
 CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
 CPPFLAGS="-I$REPOSITORYDIR/include" \
 LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS" \
 NEXT_ROOT="$MACSDKDIR" \
 ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
 ZLIB_CFLAGS="-I$MACSDKDIR/usr/include" ZLIB_LIBS="-L$MACSDKDIR/usr/lib" \
 GETTEXT_CFLAGS="-I$REPOSITORYDIR/include" GETTEXT_LIBS="-L$REPOSITORYDIR/lib" \
 LIBFFI_CFLAGS="-I$REPOSITORYDIR/include" LIBFFI_LIBS="-L$REPOSITORYDIR/lib -lffi" \
 --disable-selinux --disable-fam --disable-xattr \
 --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf \
 --disable-man --disable-dtrace --disable-systemtap \
 --enable-static=no --enable-shared --with-pcre=internal || fail "configure step of $ARCH";

make clean  || fail "make clean step"
make $MAKEARGS || fail "make step of $ARCH"
make install || fail "make install step of $ARCH"
