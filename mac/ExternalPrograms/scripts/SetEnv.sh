#!/usr/bin/env bash
# Configuration for 64-bit build, only possible from Leopard and up
# Copyright (c) 2008, Ippei Ukai
# Somewhere end of 2010 removed the 64bit ppc stuf as Apple doesn;t support it anymore
# April 2012: Remove support for PPC and Tiger.
# November 2012: tune for only 64-bit. This is the simple version without openmp

########### Adjust these as neccessary: #############################


# The compiler(s) to use
CC="/usr/local/opt/llvm/bin/clang"
CXX="/usr/local/opt/llvm/bin/clang++"


# The minimum macOS version required to run the compiled files
# USE AT LEAST VERSION 10.9
DEPLOY_TARGET="10.9"


# The version of your Xcode macOS-SDK. If you use a current Xcode version this is
# will be your macOS version
SDKVERSION=$(sw_vers -productVersion | sed "s:.[[:digit:]]*.$::g") # "10.12"

#####################################################################
#####################################################################
#####################################################################



# number of jobs that make can use, probably same as the number of CPUs.
if [ "$(uname -p)" = i386 ] || [ "$(uname -p)" = x86_64 ] ; then
  PROCESSNUM=$(hostinfo | grep "Processors active:" | sed 's/^.*://' | wc -w | sed 's/[^[:digit:]]//g');
else
  PROCESSNUM="1"
fi

DIR="$(cd "$(dirname "$BASH_SOURCE")" && pwd)"
REPOSITORYDIR="$(realpath "$DIR/..")/repository" # "..../mac/ExternalPrograms/repository"
mkdir -p "$REPOSITORYDIR"

fail()
{
	echo
    echo "** Failed at $1 **"
    exit 1
}
export -f fail

export \
 REPOSITORYDIR SDKVERSION DEPLOY_TARGET CC CXX\
 MACSDKDIR="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${SDKVERSION}.sdk" \
 MAKEARGS="--jobs=$PROCESSNUM" \
 OPTIMIZE="-march=core2 -mtune=core2 -ftree-vectorize" \
 ARGS="$OPTIMIZE -mmacosx-version-min=$DEPLOY_TARGET" \
 LDARGS="-mmacosx-version-min=$DEPLOY_TARGET"

# cmake settings
export CMAKE_INCLUDE_PATH="$REPOSITORYDIR/include"
export CMAKE_LIBRARY_PATH="$REPOSITORYDIR/lib"
