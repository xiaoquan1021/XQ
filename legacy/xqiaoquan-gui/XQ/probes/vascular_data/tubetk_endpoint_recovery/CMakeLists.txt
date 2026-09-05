cmake_minimum_required(VERSION 3.20)

project(xq_tubetk_endpoint_recovery_development CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

get_filename_component(XQ_PRODUCT_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_subdirectory("${XQ_PRODUCT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/xq_product" EXCLUDE_FROM_ALL)

# This development project restores the exact module closure used by the old
# v2.1 TubeTK adapter. It never consumes the separate research install prefix.
find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS
    ITKImageFunction
    ITKImageIntensity
    ITKOptimizers
    ITKSmoothing
    ITKSpatialObjects
    ITKStatistics
)

set(_xq_tubetk_source_dir
    "${XQ_PRODUCT_SOURCE_DIR}/third_party/itk_tubetk_1_3_5")
file(SHA256 "${_xq_tubetk_source_dir}/LICENSE"
    _xq_tubetk_license_sha256)
if(NOT _xq_tubetk_license_sha256 STREQUAL
        "aac73b3148f6d1d7111dbca32099f68d26c644c6813ae1e4f05f6579aa2663fe")
    message(FATAL_ERROR "Vendored ITK-TubeTK 1.3.5 license identity mismatch")
endif()

function(xq_verify_development_tubetk_source relative_path expected_sha256)
    set(_path "${_xq_tubetk_source_dir}/${relative_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR
            "Vendored ITK-TubeTK 1.3.5 source is missing: ${relative_path}")
    endif()
    file(SHA256 "${_path}" _actual_sha256)
    if(NOT _actual_sha256 STREQUAL "${expected_sha256}")
        message(FATAL_ERROR
            "Vendored ITK-TubeTK 1.3.5 source identity mismatch: ${relative_path}")
    endif()
endfunction()

xq_verify_development_tubetk_source(include/tubeSegmentTubes.h
    3b62fe9d178787de43b274799f78846c245ad3dff05f568434cd23fcdfa0a875)
xq_verify_development_tubetk_source(include/tubeConvertTubesToImage.h
    27306a07e6e3e906bb55c589d8b82c933738bd2686ce871baf6b8f185997c15b)
xq_verify_development_tubetk_source(include/tubeResampleImage.h
    334c484413746ce45126be0a2331657024f257b994a5c841c7d395b3f385b898)
xq_verify_development_tubetk_source(src/Filtering/itktubeReResampleImageFilter.hxx
    ca7f4e728c69b18321d966b007935ed9b6d5bd97395b7242b13750d5dfd01227)
xq_verify_development_tubetk_source(src/Segmentation/itktubeTubeExtractor.hxx
    f806a54c558f80d572e1246e6a791e4f056337b92493c9849d39923aeb12ce5d)
xq_verify_development_tubetk_source(src/Filtering/itktubeTubeSpatialObjectToImageFilter.hxx
    cd01dad94eadf7713baf30140a006ec3bd69bd4ed5df36d10e02c2082e485c53)

add_library(xq_development_tubetk_ridge STATIC
    ${_xq_tubetk_source_dir}/src/Common/tubeIndent.cxx
    ${_xq_tubetk_source_dir}/src/Common/tubeObject.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeBrentOptimizer1D.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeOptimizer1D.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeOptimizerND.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeSpline1D.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeSplineApproximation1D.cxx
    ${_xq_tubetk_source_dir}/src/Numerics/tubeSplineND.cxx
)
target_compile_features(xq_development_tubetk_ridge PUBLIC cxx_std_17)
target_include_directories(xq_development_tubetk_ridge SYSTEM PUBLIC
    ${_xq_tubetk_source_dir}/include
    ${_xq_tubetk_source_dir}/src/Common
    ${_xq_tubetk_source_dir}/src/Filtering
    ${_xq_tubetk_source_dir}/src/IO
    ${_xq_tubetk_source_dir}/src/MetaIO
    ${_xq_tubetk_source_dir}/src/Numerics
    ${_xq_tubetk_source_dir}/src/Segmentation
    ${ITKImageFunction_INCLUDE_DIRS}
    ${ITKImageIntensity_INCLUDE_DIRS}
    ${ITKOptimizers_INCLUDE_DIRS}
    ${ITKSmoothing_INCLUDE_DIRS}
    ${ITKSpatialObjects_INCLUDE_DIRS}
    ${ITKStatistics_INCLUDE_DIRS}
)
target_link_libraries(xq_development_tubetk_ridge PUBLIC
    ${ITKImageFunction_LIBRARIES}
    ${ITKImageIntensity_LIBRARIES}
    ${ITKOptimizers_LIBRARIES}
    ${ITKSmoothing_LIBRARIES}
    ${ITKSpatialObjects_LIBRARIES}
    ${ITKStatistics_LIBRARIES}
)
if(MSVC)
    target_compile_options(xq_development_tubetk_ridge PRIVATE
        /utf-8 /bigobj /wd4244 /wd4267 /wd4305 /wd4996)
endif()

function(xq_add_development_tubetk_analyzer target_name)
    add_executable(${target_name}
        ${XQ_PRODUCT_SOURCE_DIR}/probes/vascular_data/VascularRoiDevelopmentAnalyze.cpp
        ${XQ_PRODUCT_SOURCE_DIR}/third_party/itk_minimal_path/itkIterateNeighborhoodOptimizer.cxx
    )
    target_compile_definitions(${target_name} PRIVATE
        XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY=1)
    target_link_libraries(${target_name} PRIVATE
        xq_adapter_dicom
        xq_adapter_itk
        xq_io
        xq_development_tubetk_ridge
        ${ITKBinaryMathematicalMorphology_LIBRARIES}
        ${ITKConnectedComponents_LIBRARIES}
        ${ITKDistanceMap_LIBRARIES}
        ${ITKFastMarching_LIBRARIES}
        ${ITKOptimizers_LIBRARIES}
        ${ITKPath_LIBRARIES}
    )
    target_include_directories(${target_name} SYSTEM PRIVATE
        ${XQ_PRODUCT_SOURCE_DIR}/third_party/itk_thickness3d
        ${XQ_PRODUCT_SOURCE_DIR}/third_party/itk_minimal_path
        ${ITKBinaryMathematicalMorphology_INCLUDE_DIRS}
        ${ITKConnectedComponents_INCLUDE_DIRS}
        ${ITKDistanceMap_INCLUDE_DIRS}
        ${ITKFastMarching_INCLUDE_DIRS}
        ${ITKOptimizers_INCLUDE_DIRS}
        ${ITKPath_INCLUDE_DIRS}
    )
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8 /bigobj)
    endif()
endfunction()

xq_add_development_tubetk_analyzer(
    xq_vascular_roi_tubetk_endpoint_analyze)
xq_add_development_tubetk_analyzer(
    xq_vascular_roi_tubetk_unclipped_analyze)
target_compile_definitions(xq_vascular_roi_tubetk_unclipped_analyze PRIVATE
    XQ_DEVELOPMENT_TUBETK_UNCLIPPED_INPUT=1)
xq_add_development_tubetk_analyzer(
    xq_vascular_roi_tubetk_all_endpoints_analyze)
target_compile_definitions(
    xq_vascular_roi_tubetk_all_endpoints_analyze PRIVATE
    XQ_DEVELOPMENT_TUBETK_UNCLIPPED_INPUT=1
    XQ_DEVELOPMENT_TUBETK_ALL_ENDPOINTS=1)
