
# a list of all files belonging to hugin base library

SET(HUGIN_BASE_SRC
algorithms/nona/NonaFileStitcher.cpp
algorithms/basic/CalculateCPStatistics.cpp
algorithms/basic/CalculateMeanExposure.cpp
algorithms/basic/CalculateOptimalScale.cpp
algorithms/basic/CalculateOptimalROI.cpp
algorithms/basic/RotatePanorama.cpp
algorithms/basic/TranslatePanorama.cpp
algorithms/basic/StraightenPanorama.cpp
algorithms/basic/CalculateOverlap.cpp
algorithms/basic/LayerStacks.cpp
algorithms/nona/CalculateFOV.cpp
algorithms/nona/CenterHorizontally.cpp
algorithms/nona/FitPanorama.cpp
algorithms/nona/ComputeImageROI.cpp
algorithms/optimizer/ImageGraph.cpp
algorithms/optimizer/PhotometricOptimizer.cpp
algorithms/optimizer/PTOptimizer.cpp
algorithms/point_sampler/PointSampler.cpp
algorithms/control_points/CleanCP.cpp
appbase/ProgressDisplay.cpp
huginapp/CachedImageRemapper.cpp
huginapp/ImageCache.cpp
hugin_math/eig_jacobi.cpp
hugin_math/Matrix3.cpp
hugin_math/Vector3.cpp
hugin_utils/alphanum.cpp
hugin_utils/utils.cpp
hugin_utils/platform.cpp
lensdb/LensDB.cpp
lines/FindLines.cpp 
lines/FindN8Lines.cpp
nona/SpaceTransform.cpp
nona/Stitcher1.cpp
nona/Stitcher2.cpp
nona/Stitcher3.cpp
nona/Stitcher4.cpp
nona/Stitcher.cpp
nona/StitcherOptions.cpp
panodata/ControlPoint.cpp
panodata/Lens.cpp
panodata/Mask.cpp
panodata/Panorama.cpp
panodata/PanoramaOptions.cpp
panodata/PanoramaVariable.cpp
panodata/ParseExp.cpp
panodata/PTScriptParsing.cpp
panodata/SrcPanoImage.cpp
panodata/ImageVariableGroup.cpp
panodata/StandardImageVariableGroups.cpp
panodata/Exiv2Helper.cpp
panotools/PanoToolsInterface.cpp
panotools/PanoToolsOptimizerWrapper.cpp
panotools/PanoToolsUtils.cpp
panotools/PanoToolsTransformGPU.cpp
vigra_ext/emor.cpp
vigra_ext/ImageTransformsGPU.cpp
)

SET(HUGIN_BASE_HEADER
hugin_basic.h
algorithms/PanoramaAlgorithm.h
algorithms/StitcherAlgorithm.h
algorithms/basic/CalculateCPStatistics.h
algorithms/basic/CalculateMeanExposure.h
algorithms/basic/CalculateOptimalScale.h
algorithms/basic/CalculateOptimalROI.h
algorithms/basic/RotatePanorama.h
algorithms/basic/TranslatePanorama.h
algorithms/basic/StraightenPanorama.h
algorithms/basic/CalculateOverlap.h
algorithms/basic/LayerStacks.h
algorithms/control_points/CleanCP.h
algorithms/nona/NonaFileStitcher.h
algorithms/nona/CalculateFOV.h
algorithms/nona/CenterHorizontally.h
algorithms/nona/FitPanorama.h
algorithms/nona/ComputeImageROI.h
algorithms/optimizer/ImageGraph.h
algorithms/optimizer/PhotometricOptimizer.h
algorithms/optimizer/PTOptimizer.h
algorithms/point_sampler/PointSampler.h
appbase/DocumentData.h
appbase/ProgressDisplay.h
huginapp/CachedImageRemapper.h
huginapp/ImageCache.h
hugin_math/eig_jacobi.h
hugin_math/hugin_math.h
hugin_math/Matrix3.h
hugin_math/Vector3.h
hugin_utils/alphanum.h
hugin_utils/openmp_lock.h
hugin_utils/platform.h
hugin_utils/stl_utils.h
hugin_utils/utils.h
lensdb/LensDB.h
lines/FindLines.h
lines/FindN8Lines.h
lines/LinesTypes.h
nona/ImageRemapper.h
nona/RemappedPanoImage.h
nona/SpaceTransform.h
nona/Stitcher.h
nona/StitcherOptions.h
panodata/ControlPoint.h
panodata/Exiv2Helper.h
panodata/ImageVariable.h
panodata/ImageVariableGroup.h
panodata/ImageVariableTranslate.h
panodata/image_variables.h
panodata/Lens.h
panodata/Mask.h
panodata/OptimizerSwitches.h
panodata/Panorama.h
panodata/PanoramaData.h
panodata/PanoramaOptions.h
panodata/PanoramaVariable.h
panodata/ParseExp.h
panodata/PTScriptParsing.h
panodata/SrcPanoImage.h
panodata/StandardImageVariableGroups.h
panotools/PanoToolsInterface.h
panotools/PanoToolsOptimizerWrapper.h
panotools/PanoToolsUtils.h
photometric/ResponseTransform.h
vigra_ext/BlendPoisson.h
vigra_ext/Correlation.h
vigra_ext/cms.h
vigra_ext/emor.h
vigra_ext/FileRAII.h
vigra_ext/FitPolynom.h
vigra_ext/FunctorAccessor.h
vigra_ext/HDRUtils.h
vigra_ext/ImageTransforms.h
vigra_ext/ImageTransformsGPU.h
vigra_ext/InterestPoints.h
vigra_ext/Interpolators.h
vigra_ext/lut.h
vigra_ext/openmp_vigra.h
vigra_ext/Pyramid.h
vigra_ext/pyramid2.h
vigra_ext/ransac.h
vigra_ext/ReduceOpenEXR.h
vigra_ext/ROIImage.h
vigra_ext/StitchWatershed.h
vigra_ext/tiffUtils.h
vigra_ext/utils.h
vigra_ext/VignettingCorrection.h
)

IF (${HUGIN_SHARED_LIBS})
  add_library(huginbase SHARED ${HUGIN_BASE_SRC} ${HUGIN_BASE_HEADER})
  target_link_libraries(huginbase ${Boost_LIBRARIES} Threads::Threads ${X11_X11_LIB})
  set_target_properties(huginbase PROPERTIES VERSION ${HUGIN_LIB_VERSION})
  IF(WIN32)
    install(TARGETS huginbase RUNTIME DESTINATION ${BINDIR})
  ELSEIF(${HUGIN_LIBS_PRIVATE_DIR})
    install(TARGETS huginbase LIBRARY DESTINATION ${LIBDIR}/hugin NAMELINK_SKIP)
  ELSE(WIN32)
    install(TARGETS huginbase LIBRARY DESTINATION ${LIBDIR} NAMELINK_SKIP)
  ENDIF(WIN32)
ELSE (${HUGIN_SHARED_LIBS})
  add_library(huginbase STATIC ${HUGIN_BASE_SRC} ${HUGIN_BASE_HEADER})
ENDIF (${HUGIN_SHARED_LIBS})

TARGET_LINK_LIBRARIES(huginbase huginlevmar ${VIGRA_LIBRARIES} 
        ${Boost_LIBRARIES} ${EXIV2_LIBRARIES} ${PANO_LIBRARIES}
        ${TIFF_LIBRARIES} ${LAPACK_LIBRARIES}
        ${OPENGL_GLEW_LIBRARIES} Threads::Threads
        ${SQLITE3_LIBRARIES} ${LCMS2_LIBRARIES})

IF (MINGW)
    # exiv2 needs psapi and ws2_32 when compiling with Mingw
    TARGET_LINK_LIBRARIES(huginbase psapi ws2_32)
ENDIF() 

source_group(Lines REGULAR_EXPRESSION lines/*)
source_group(Vigra_ext REGULAR_EXPRESSION vigra_ext/*)
source_group(Algorithm REGULAR_EXPRESSION algorithm/*)
source_group(Panodata REGULAR_EXPRESSION panodata/*)
source_group(Panotools REGULAR_EXPRESSION panotools/*)
source_group(Nona REGULAR_EXPRESSION nona/*)
source_group(LensDB REGULAR_EXPRESSION lensdb/*)
source_group(Hugin_utils REGULAR_EXPRESSION hugin_utils/*)
source_group(Hugin_math REGULAR_EXPRESSION hugin_math/*)
source_group(AppBase REGULAR_EXPRESSION "(appbase/*|huginapp/*)")
