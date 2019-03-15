#!/usr/bin/env bash
# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100110.0 sg Make libGLEW and libexiv2 dynamic
#               Update to enblend-enfuse-4.0 and panotools 2.9.15
# 20100112.0 sg Made libxmi dynamic. Created lib-static directory
# 20100117.0 sg Update for glew 1.5.2
# 20100119.0 HvdW Add libiconv
# 20100118.1 sg Fixed missing "" and named SVN directory for panotools libpano13-2.9.16
# 20100121.0 sg Updated for newer packages: boost,jpeg,png,tiff,exiv2,lcms
# 20100121.1 sg Backed out new version of boost
# 20120413.0 hvdw update a lot of stuff. Add new scripts
# 20121010.0 hvdw update lots of scripts
# -------------------------------

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR" || exit 1
source SetEnv.sh
pre="<<<<<<<<<<<<<<<<<<<<"
pst=">>>>>>>>>>>>>>>>>>>>"

FORCE_BUILD=$1

build(){
    cd "$REPOSITORYDIR"
    
    local name=$1
    local dir=$2
    if [[ -n "$4" ]]; then
        local patch=$3
        local script=$4
    else
        local script=$3
    fi
    if [[ ! -f "_built/$name" ]] || [[ $FORCE_BUILD = "$name" ]]; then
        echo "$pre $name:   building    $pst"
        cd "$dir" || exit 1
        sh -c "$patch"
        { $BASH "$script" || exit 1; } && touch "../_built/$name"
        cd ..
    else
        echo "$pre $name:   already built   $pst"
    fi
}

cd "$REPOSITORYDIR"
mkdir -p _built

build "libomp"    libomp*       ../../scripts/libomp.sh
build "boost"     boost*        ../../scripts/boost.sh
build "gettext"   gettext*      ../../scripts/gettext.sh
build "libffi"    libffi*       ../../scripts/libffi.sh
build "glib2"     glib*         ../../scripts/libglib2.sh
build "fftw"      fftw*         ../../scripts/fftw.sh
build "libglew"   glew*         ../../scripts/libglew.sh
build "gsl"       gsl*          ../../scripts/gsl.sh
build "libjpeg"   jpeg*         ../../scripts/libjpeg.sh
build "libpng"    libpng*       ../../scripts/libpng.sh
build "libtiff"   tiff*         ../../scripts/libtiff.sh
build "ilmbase"   ilmbase*      ../../scripts/ilmbase.sh
build "openexr"   openexr*      ../../scripts/openexr.sh
build "libpano"   libpano13*    ../../scripts/pano13.sh
build "libexiv2"  exiv2*        ../../scripts/libexiv2.sh
build "liblcms2"  lcms2*        ../../scripts/lcms2.sh
build "vigra"     vigra*        "patch -p1 -N < ../../patches/vigra-patch-include-vigra-hdf5impex.hxx.diff;" ../../scripts/vigra.sh
build "wxmac"     wxWidgets*    "patch -p1 -N < ../../patches/wx-patch-quicktime-removal.diff; patch -p1 -N < ../../patches/wx-patch-yosemite.diff" ../../scripts/wxmac.sh
build "enblend"   enblend*      ../../scripts/enblend.sh

echo "$pre Finished! $pst"

cd "$REPOSITORYDIR"
