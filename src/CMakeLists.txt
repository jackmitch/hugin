
# define common sets of libraries, used by different subdirectories
SET(common_libs huginbase ${PANO_LIBRARIES} huginlevmar ${GLEW_LIBRARIES})
IF (MSVC)
  SET(common_libs hugingetopt ${common_libs})
  include_directories( ${CMAKE_SOURCE_DIR}/src/foreign/getopt/include )
ENDIF()
IF(NOT HAVE_STD_FILESYSTEM)
  SET(common_libs ${common_libs} ${Boost_LIBRARIES})
ENDIF ()
IF(LAPACK_FOUND)
  SET(common_libs ${common_libs} ${LAPACK_LIBRARIES})
ENDIF()
IF(FFTW_FOUND)
  SET(common_libs ${common_libs} ${FFTW_LIBRARIES})
ENDIF()

set(image_libs ${VIGRA_LIBRARIES} ${OPENEXR_LIBRARIES} ${JPEG_LIBRARIES} ${TIFF_LIBRARIES}
    ${PNG_LIBRARIES} ${ZLIB_LIBRARIES} ${EXIV2_LIBRARIES})

SET(common_libs ${common_libs} ${image_libs} ${LCMS2_LIBRARIES} Threads::Threads)

#add_subdirectory(celeste)
add_subdirectory(foreign)
add_subdirectory(hugin_base)
add_subdirectory(tools)
#add_subdirectory(deghosting)

#add_subdirectory(hugin_cpfind)
#add_subdirectory(translations)
