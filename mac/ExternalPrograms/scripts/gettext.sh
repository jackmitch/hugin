#!/usr/bin/env bash
# ------------------
#    gettext
# ------------------
# Based on the works of (c) 2007, Ippei Ukai
# Created for Hugin by Harry van der Wolf 2009

# download location ftp.gnu.org/gnu/gettext/

# prepare

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100116.0 HvdW Correct script for libintl install_name in libgettext*.dylib
# 20100624.0 hvdw More robust error checking on compilation
# 20121110.0 hvdw update to 0.18.1
# 20121114.0 hvdw fix some errors in symlinking
# -------------------------------


env \
  CC="$CC" CXX="$CXX" \
  CFLAGS="-isysroot $MACSDKDIR   $ARGS -O3" \
  CXXFLAGS="-isysroot $MACSDKDIR $ARGS -O3" \
  CPPFLAGS="-I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib $LDARGS " \
  NEXT_ROOT="$MACSDKDIR" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
    --enable-shared --enable-static=no --disable-csharp --disable-java \
    --with-included-gettext --with-included-glib \
    --with-included-libxml --without-examples --with-libexpat-prefix="$REPOSITORYDIR" \
    --with-included-libcroco  --without-emacs --with-libiconv-prefix="$REPOSITORYDIR" || fail "configure step for $ARCH" ;


make clean || fail "make clean step"
make $MAKEARGS || fail "failed at make step of $ARCH";
make install || fail "make install step of $ARCH";
