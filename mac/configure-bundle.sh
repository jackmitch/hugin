#!/usr/bin/env bash

# INSTRUCTIONS FOR BUILDING A SELF CONTAINED BUNDLE (FOR DISTRIBUTION)
# MORE DETAILS AT THE PANOTOOLS WIKI: http://wiki.panotools.org/Hugin_Compiling_OSX
#
# Install cmake and llvm from homebrew (OpenMP support is missing from Apple's clang)
#
#					brew install llvm cmake
#
# Build the external programs (see in folder ExternalProgramms) by running
# download-all.sh and build-all.sh
#
# create and cd into a folder next to the "hugin" source folder (i.e. "build")
#
# someFolder
#  |- hugin (the source folder)
#   - build (cd into this folder)
#
# then run:
#  						../hugin/mac/cmake-bundle.sh
#
# 						make
#
#						make install   (installing into PREFIX)
# 		(or)			make package   (to create a dmg image)
#
# 					(styling the dmg can sometimes be a bit fiddly,
# 							retry if it doesn't work correctly)


# Use "Release" and "/" for creating a dmg
# add   -DHUGIN_BUILDER="YOUR NAME" to the cmake call to set the builder

TYPE="Release"
PREFIX="~/Desktop/Hugin"


SDKVERSION=$(sw_vers -productVersion | sed "s:.[[:digit:]]*.$::g") # "10.12"
REPOSITORYDIR=$(cd .. && pwd)"/hugin/mac/ExternalPrograms/repository"

PKG_CONFIG_PATH=$REPOSITORYDIR/lib/pkgconfig \
CC=/usr/local/opt/llvm/bin/clang CXX=/usr/local/opt/llvm/bin/clang++ \
cmake ../hugin -B. \
-DCMAKE_OSX_SYSROOT="macosx${SDKVERSION}" \
-DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$REPOSITORYDIR" \
-DBUILD_HSI=OFF -DENABLE_LAPACK=ON -DMAC_SELF_CONTAINED_BUNDLE=ON \
-DCMAKE_BUILD_TYPE="$TYPE" -G "Unix Makefiles"
