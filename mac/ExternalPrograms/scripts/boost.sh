#!/usr/bin/env bash
# ------------------
#      boost
# ------------------
# $Id: boost.sh 1902 2007-02-04 22:27:47Z ippei $
# Copyright (c) 2007-2008, Ippei Ukai

# prepare

# export REPOSITORYDIR="/PATH2HUGIN/mac/ExternalPrograms/repository" \
#  ARCHS="ppc i386" \
#  ppcTARGET="powerpc-apple-darwin8" \
#  ppcOSVERSION="10.4" \
#  ppcMACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  ppcOPTIMIZE="-mcpu=G3 -mtune=G4" \
#  i386TARGET="i386-apple-darwin8" \
#  i386OSVERSION="10.4" \
#  i386MACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  i386OPTIMIZE ="-march=prescott -mtune=pentium-m -ftree-vectorize" \
#  OTHERARGs="";

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100121.0 sg Script updated for 1_41
# 20100121.1 sg Script reverted to 1_40
# 20100624.0 hvdw More robust error checking on compilation
# 20100831.0 hvdw Upgraded to 1_44
# 20100920.0 hvdw Add removed libboost_system again and add iostreams and regex
# 20100920.1 hvdw Add date_time as well
# 20111230.0 hvdw Adapt for versions >= 1.46. slightly different source tree structure
# 20111230.1 hvdw Correct stupid typo
# -------------------------------


echo "## First compiling b2 ##"
cd "./tools/build/"
sh "bootstrap.sh"
mkdir -p bin
./b2 install --prefix=./bin toolset=darwin
cd "../../"
B2=$(ls ./tools/build/bin/bin/b2)

echo "b2 command is: $B2"
echo "## Done compiling b2 ##"


echo "using darwin : : $CXX :  ;" > ./user-conf.jam
$B2 -a --prefix="$REPOSITORYDIR" --user-config=./user-conf.jam install \
  --with-filesystem --with-system \
  variant=release \
  cxxflags="$OPTIMIZE -std=c++11 -stdlib=libc++ -mmacosx-version-min=$DEPLOY_TARGET" \
  linkflags="-stdlib=libc++ -lc++ -mmacosx-version-min=$DEPLOY_TARGET" || fail "building"


if [ -f "$REPOSITORYDIR/lib/libboost_filesystem.dylib" ]; then
 install_name_tool -id "$REPOSITORYDIR/lib/libboost_filesystem.dylib" "$REPOSITORYDIR/lib/libboost_filesystem.dylib";
 install_name_tool -change "libboost_system.dylib" "$REPOSITORYDIR/lib/libboost_system.dylib" "$REPOSITORYDIR/lib/libboost_filesystem.dylib";
fi

if [ -f "$REPOSITORYDIR/lib/libboost_system.dylib" ]; then
 install_name_tool -id "$REPOSITORYDIR/lib/libboost_system.dylib" "$REPOSITORYDIR/lib/libboost_system.dylib";
fi
