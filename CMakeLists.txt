
# require at least cmake 3.1
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR )

# prevent in-tree building
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules )

include(HuginMacros)
include(CheckIncludeFiles)

##  global setup
project(hugin)

# version
set(V_MAJOR 2018)
set(V_MINOR 0)
set(V_PATCH 0)

#
# Check for mercurial and get current revision
#

SET(CPACK_PACKAGE_VERSION_PATCH ${V_PATCH})
SET(HUGIN_PACKAGE_VERSION ${V_MAJOR}.${V_MINOR}.${V_PATCH})
SET(HUGIN_WC_REVISION 0)
FILE(READ "${PROJECT_SOURCE_DIR}/rev.txt" HUGIN_WC_REVISION)
MESSAGE("-- Current HG revision is ${HUGIN_WC_REVISION}")
MESSAGE("-- Assuming this is a tarball (release) build for ${HUGIN_PACKAGE_VERSION}")
SET(HUGIN_DEVELOPMENT_VERSION 0)

# version to display
IF (HUGIN_DEVELOPMENT_VERSION EQUAL 1)
  set(DISPLAY_VERSION "Pre-Release ${V_MAJOR}.${V_MINOR}.${V_PATCH}.${HUGIN_WC_REVISION}")
ELSE (HUGIN_DEVELOPMENT_VERSION EQUAL 1)
  set(DISPLAY_VERSION "${V_MAJOR}.${V_MINOR}.${V_PATCH}.${HUGIN_WC_REVISION}")
ENDIF (HUGIN_DEVELOPMENT_VERSION EQUAL 1)

# version for internal libraries.
set(HUGIN_LIB_VERSION 0.0)

# set include path for internal libraries before using finding and initializing
# external dependencies. This makes sure that "our" include paths are specified
# before any system includes

include_directories( ${CMAKE_BINARY_DIR}/src )
include_directories( ${CMAKE_SOURCE_DIR}/src/hugin_base )
include_directories( ${CMAKE_SOURCE_DIR}/src/celeste )
include_directories( ${CMAKE_BINARY_DIR}/src/celeste )
include_directories( ${CMAKE_SOURCE_DIR}/src )

# this function is not available on some systems, notably FreeBSD and Windows
include(CheckFunctionExists)
set(CMAKE_REQUIRED_LIBRARIES -lm)
check_function_exists(log1p HAVE_LOG1P)

FIND_PACKAGE(PkgConfig)

set(HUGIN_SHARED "1" CACHE STRING "Build shared internal libraries" )

##
## Graphics libraries
##

FIND_PACKAGE(TIFF REQUIRED)
INCLUDE_DIRECTORIES(${TIFF_INCLUDE_DIR})
FIND_PACKAGE(JPEG REQUIRED)
INCLUDE_DIRECTORIES(${JPEG_INCLUDE_DIR})
FIND_PACKAGE(PNG REQUIRED)
INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${PNG_INCLUDE_DIR})

IF(WIN32 AND NOT HUGIN_SHARED)
IF(NOT ${WX_jpeg} MATCHES ${JPEG_LIBRARIES_RELEASE})
MESSAGE(FATAL_ERROR "Using different libjpeg and wxjpeg. This does not work with static libs. Use wxjpeg for libjpeg")
ENDIF()
IF(NOT ${WX_zlib} MATCHES ${ZLIB_LIBRARIES_RELEASE})
MESSAGE(FATAL_ERROR "Using different libzlib and wxzlib. This does not work with static libs. Use wxjpeg for libzlib")
ENDIF()
IF(NOT ${WX_png} MATCHES ${PNG_LIBRARIES_RELEASE})
MESSAGE(FATAL_ERROR "Using different libpng and wxpng. This does not work with static libs. Use wxjpeg for libpng")
ENDIF()
IF(NOT ${WX_tiff} MATCHES ${TIFF_LIBRARIES_RELEASE})
MESSAGE(FATAL_ERROR "Using different libtiff and wxtiff. This does not work with static libs. Use wxtiff for libtiff")
ENDIF()

ENDIF()

FIND_PACKAGE(OpenEXR REQUIRED)
include_directories(${OPENEXR_INCLUDE_DIR})

FIND_PACKAGE(VIGRA 1.9.0 REQUIRED)
include_directories(${VIGRA_INCLUDE_DIR})

FIND_PACKAGE(EXIV2 REQUIRED)
INCLUDE_DIRECTORIES(${EXIV2_INCLUDE_DIR})

##
## Panotools
##

FIND_PACKAGE(PANO13 2.9.19 REQUIRED)

IF(PANO13_FOUND)
  SET(PANO_LIBRARIES ${PANO13_LIBRARIES})
  SET(PANO_INCLUDE_DIR ${PANO13_INCLUDE_DIR})
ELSE(PANO13_FOUND)
  MESSAGE(SEND_ERROR "libpano13 required")
ENDIF(PANO13_FOUND) 
INCLUDE_DIRECTORIES(${PANO_INCLUDE_DIR})

# Check CXX11 compiler
SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
# necessary threads lib
SET(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)

# check if we have C++17 <filesystem> header
INCLUDE(CheckCXXSourceCompiles)
check_cxx_source_compiles(
  "
    #include <filesystem>
    #include <string>
    
    int main()
    {
      std::tr2::sys::path filepath;
      filepath=std::tr2::sys::absolute(filepath);
      std::string s=filepath.filename().string();
      return 0;    
    };
  "
  HAVE_STD_FILESYSTEM
)

IF(HAVE_STD_FILESYSTEM)
# in this combination we don't need Boost anymore
ELSE()
##
## Boost
##
# I am sick of having to edit the boost rev number in this file.
# Now it is a cache variable you can edit in cmakesetup --TKS
#
IF( NOT Boost_root_suffix )
  SET( Boost_root_suffix _1_54_0 CACHE STRING "suffix of boost root dir." FORCE )
ENDIF( NOT Boost_root_suffix )

FIND_PACKAGE(Boost 1.47 COMPONENTS filesystem system)

IF (NOT Boost_FOUND)
  IF(WIN32)
    MESSAGE(FATAL_ERROR "Boost not found. Bad BOOST_ROOT? On windows, try editing Boost_root_suffix")
  ELSE()
    MESSAGE(FATAL_ERROR "Boost not found. Maybe wrong version. Hugin requires at least version 1.47")
  ENDIF()
ENDIF (NOT Boost_FOUND)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF()

# littleCMS 2 now
FIND_PACKAGE(LCMS2 REQUIRED)
INCLUDE_DIRECTORIES(${LCMS2_INCLUDE_DIR})
IF(WIN32)
  IF(${HUGIN_SHARED})
    ADD_DEFINITIONS(-DCMS_DLL)
  ENDIF()
ENDIF()

FIND_PACKAGE(FLANN)

IF(NOT FLANN_FOUND)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/foreign)
ENDIF(NOT FLANN_FOUND)

FIND_PACKAGE(FFTW)
IF(FFTW_FOUND)
  MESSAGE(STATUS "Found FFTW library. Activated support for FFT fast cross correlation used by fine-tune feature.")
  INCLUDE_DIRECTORIES(${FFTW_INCLUDE_DIR})
  SET(HAVE_FFTW TRUE)
ELSE()
  MESSAGE(STATUS "Not found FFTW library. Deactivating support for FFT fast cross correlation used by fine-tune feature.")
ENDIF()

# Check for OpenMP
FIND_PACKAGE(OpenMP)

IF(OPENMP_FOUND)
  MESSAGE(STATUS "Compiler supports OpenMP. Activating support for it.")
  ADD_COMPILE_OPTIONS(${OpenMP_CXX_FLAGS})
  SET(HAVE_OPENMP TRUE)
  IF(OpenMP_CXX_FLAGS AND NOT MSVC)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
    SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
  ENDIF()
ELSE()
  MESSAGE(WARNING "Compiler does not support OpenMP. OpenMP is required for multi-threaded processes. So only single threaded execution is compiled now.") 
ENDIF()

FIND_PACKAGE(SQLite3 REQUIRED)
INCLUDE_DIRECTORIES(${SQLITE3_INCLUDE_DIR})

##
## LAPACK (optional, enable by -DENABLE_LAPACK=ON)
##
OPTION (ENABLE_LAPACK "Use LAPACK based solver in levmar" OFF)

IF(ENABLE_LAPACK)
  FIND_PACKAGE(LAPACK)
  IF(LAPACK_FOUND)
    MESSAGE(STATUS "LAPACK found (${LAPACK_LIBRARIES})")
  ELSE(LAPACK_FOUND)
    MESSAGE(STATUS "LAPACK not found, using LU-based solver")
  ENDIF(LAPACK_FOUND)
ENDIF(ENABLE_LAPACK)

#
# Check build type and set options accordingly
# 
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# allow user to specify lib suffix
set(LIB_SUFFIX "" CACHE STRING "Define suffix of lib directory name (32/64)" )

IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  MESSAGE("Building in Debug mode, resulting programs will be SLOW")
  ADD_DEFINITIONS(-DDEBUG)
  IF(UNIX)
    ADD_COMPILE_OPTIONS(-Wall)
  ENDIF(UNIX)
ENDIF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

#
# define destination dirs (relative to prefix)
#
SET(HUGINDATADIR share/hugin)
SET(DATADIR share)
SET(BINDIR  bin)

IF (NOT MANDIR AND NOT $ENV{MANDIR} STREQUAL "")
    SET (MANDIR "$ENV{MANDIR}")
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    SET (MANDIR "man")
ELSE(NOT MANDIR AND NOT $ENV{MANDIR} STREQUAL "")
    SET (MANDIR "share/man")
ENDIF(NOT MANDIR AND NOT $ENV{MANDIR} STREQUAL "")

SET(LOCALEDIR share/locale) 
SET(ICONDIR share/icons) 
SET(LINKDIR share/applications)
SET(LIBDIR  lib${LIB_SUFFIX})
SET(INSTALL_XRC_DIR ${CMAKE_INSTALL_PREFIX}/share/hugin/xrc)
SET(INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share/hugin/data)
SET(INSTALL_LOCALE_DIR ${CMAKE_INSTALL_PREFIX}/${LOCALEDIR})

IF (${HUGIN_SHARED} AND UNIX AND NOT APPLE)
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIBDIR}/hugin")
  SET(HUGIN_LIBS_PRIVATE_DIR "1" CACHE INTERNAL "Store internal
    libraries in private subdirectory" )
ELSE (${HUGIN_SHARED} AND UNIX AND NOT APPLE)
  # On OS X files are moved around after make install, do not try to use
  # rpath or CMAKE_INSTALL_NAME_DIR
  SET(HUGIN_LIBS_PRIVATE_DIR "0" CACHE INTERNAL "Store internal
    libraries in private subdirectory" )
ENDIF (${HUGIN_SHARED} AND UNIX AND NOT APPLE)

IF (${HUGIN_SHARED})
  # use shared internal libraries
  SET(HUGIN_SHARED_LIBS 1)
ENDIF (${HUGIN_SHARED})
MESSAGE("-- Using shared internal libraries")

# create config.h file
configure_file(src/hugin_config.h.in.cmake ${CMAKE_BINARY_DIR}/src/hugin_config.h)
configure_file(src/hugin_version.h.in.cmake ${CMAKE_BINARY_DIR}/src/hugin_version.h)

add_subdirectory(src)
add_subdirectory(doc)
# install enfuse droplets and windows installer and everything else in platforms
ADD_SUBDIRECTORY(platforms)

SET(CPACK_PACKAGE_VERSION_MAJOR "${V_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${V_MINOR}")

SET(CPACK_PACKAGE_VERSION_PATCH "${V_PATCH}.${CPACK_PACKAGE_VERSION_PATCH}")

SET(CPACK_SOURCE_PACKAGE_FILE_NAME "hugin-${V_MAJOR}.${V_MINOR}.${V_PATCH}")
SET(CPACK_SOURCE_GENERATOR "TBZ2")
SET(CPACK_PACKAGE_CONTACT "hugin-ptx@googlegroups.com")

SET(CPACK_SET_DESTDIR "ON")

SET(CPACK_SOURCE_IGNORE_FILES
"/_CPack_Packages/"
"/CMakeCache.txt$"
"\\\\.dir/"
"\\\\.tar\\\\.gz$"
"\\\\.tar\\\\.bz2$"
"\\\\.tar\\\\.Z$"
"\\\\.svn/"
"\\\\.cvsignore$"
"\\\\.swp$"
"~$"
"\\\\.#"
"/#"
"/src/api-doc/"
"/celeste/celeste_standalone$"
"/celeste/Config.h$"
"/celeste/training/celeste_train$"
"/tools/autooptimiser$"
"/tools/fulla$"
"/tools/nona$"
"/tools/vig_optimize$"
"/tools/align_image_stack$"
"/tools/tca_correct$"
"/tools/pto2mk$"
"/tools/hugin_hdrmerge$"
"/ptbatcher/PTBatcher$"
"/ptbatcher/PTBatcherGUI$"
"/hugin_base/test/open_file$"
"/matchpoint/matchpoint$"
"/hugin/hugin$"
"/whitepaper/"
"/lens_calibrate/test_lensFunc$"
"/lens_calibrate/calibrate_lens$"
"/nona_gui/nona_gui$"
"/stitch_project/hugin_stitch_project$"
"/deghosting/deghosting_mask$" 
"/outdated/"
"/build/"
"/CVS/"
"/cmake_install.cmake$"
"/CMakeOutput.log$"
"/CPackConfig.cmake$"
"/CPackSourceConfig.cmake$"
"/Makefile$"
"/Makefile2$"
"/CMakeFiles/"
"/\\\\.libs/"
"/\\\\.deps/"
"\\\\.o$"
"\\\\.gmo$"
"\\\\.mo$"
"\\\\.a$"
"\\\\.lo$"
"\\\\.so$"
"\\\\.so\\\\.0$" 
"\\\\.so\\\\.0\\\\.0$" 
"\\\\.la$"
"Makefile\\\\.in$"
"/\\\\.hg/"
"/artwork-src/"
"\\\\.hgignore$"
"\\\\.hgtags$"
 )

message(STATUS "Current source dir = ${CMAKE_CURRENT_SOURCE_DIR}")
SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/Debian/postinst"
				       "${CMAKE_CURRENT_SOURCE_DIR}/Debian/shlibs.local")

INCLUDE(CPack)

##
## Uninstall Taget
##
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

