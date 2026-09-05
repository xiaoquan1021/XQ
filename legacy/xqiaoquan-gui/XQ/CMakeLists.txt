cmake_minimum_required(VERSION 3.20)

project(xq_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# An explicitly supplied package directory is a reproducibility contract. CMake
# may otherwise ignore a stale/invalid <Package>_DIR and continue searching host
# prefixes or the registry, which would hide dependency-baseline defects.
function(xq_validate_explicit_package_dir variable config_file)
    if(DEFINED ${variable})
        set(_xq_package_dir "${${variable}}")
        if(_xq_package_dir STREQUAL "")
            message(FATAL_ERROR
                "${variable} was supplied but is empty; provide the locked "
                "package directory containing ${config_file}")
        elseif(NOT EXISTS "${_xq_package_dir}/${config_file}")
            message(FATAL_ERROR
                "${variable} must name a package directory containing ${config_file}: "
                "${_xq_package_dir}")
        endif()
    endif()
endfunction()

xq_validate_explicit_package_dir(Qt6_DIR Qt6Config.cmake)
xq_validate_explicit_package_dir(VTK_DIR vtk-config.cmake)
xq_validate_explicit_package_dir(ITK_DIR ITKConfig.cmake)
xq_validate_explicit_package_dir(tinyxml2_DIR tinyxml2Config.cmake)

option(XQ_ENABLE_FLOW "Build Flow execution services and controllers" ON)

set(XQ_TEST_DATA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../0007_H_AO_H"
    CACHE PATH "Root directory for XQ real sample test data")
set(XQ_DICOM_TEST_DATA_ROOT ""
    CACHE PATH "Authorized de-identified DICOM series directory for the optional real-data gate")
get_filename_component(_xq_test_data_root "${XQ_TEST_DATA_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${_xq_test_data_root}" _xq_test_data_root)
if(NOT "${XQ_DICOM_TEST_DATA_ROOT}" STREQUAL "")
    get_filename_component(_xq_dicom_test_data_root
        "${XQ_DICOM_TEST_DATA_ROOT}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    file(TO_CMAKE_PATH
        "${_xq_dicom_test_data_root}" _xq_dicom_test_data_root)
    if(NOT IS_DIRECTORY "${_xq_dicom_test_data_root}")
        message(FATAL_ERROR
            "XQ_DICOM_TEST_DATA_ROOT must name a readable DICOM series directory")
    endif()
endif()
set(_xq_ctgr_dir "${_xq_test_data_root}/Segmentations")
set(_xq_flow_dir "${_xq_test_data_root}/flow-files")
set(_xq_vti_path "${_xq_test_data_root}/Images/OSMSC0090-cm.vti")
set(_xq_models_dir "${_xq_test_data_root}/Models")
set(_xq_meshes_dir "${_xq_test_data_root}/Meshes")
set(_xq_pth_dir "${_xq_test_data_root}/Paths")
set(_xq_svproject_dir "${_xq_test_data_root}")

add_library(xq_core STATIC
    src/core/NodeId.cpp
    src/core/Diagnostics.cpp
    src/core/asset/AssetId.cpp
    src/core/asset/AssetRegistry.cpp
    src/core/XQImageVolume.cpp
    src/core/XQMemoryImageBufferHandle.cpp
    src/core/image/DicomSeriesReader.cpp
    src/core/image/IAutomaticVesselSegmenter.cpp
    src/core/image/IVascularRoiPriorReader.cpp
    src/core/image/IVascularPreprocessor.cpp
    src/core/image/IVascularSegmentationEvaluator.cpp
    src/core/image/XQVascularRoiPrior.cpp
    src/core/XQSegmentation.cpp
    src/core/XQSegmentationMask.cpp
    src/core/XQPath.cpp
    src/core/path/ICenterlineSkeletonizer3D.cpp
    src/core/XQContourGroup.cpp
    src/core/XQVesselProfile.cpp
    src/core/XQVesselPath.cpp
    src/core/XQSurfaceModel.cpp
    src/core/XQTriangleSurfaceGeometryHandle.cpp
    src/core/XQTetVolumeMeshHandle.cpp
    src/core/source/ResidentVoxelSource.cpp
    src/core/source/ResidentSurfaceSource.cpp
    src/core/source/ResidentTetSource.cpp
    src/core/XQMesh.cpp
    src/core/XQSimulationCase.cpp
    src/core/XQFlowResult.cpp
    src/core/XQAiAnalysis.cpp
    src/core/XQProject.cpp
    src/core/XQDataNode.cpp
    src/core/XQScene.cpp
    src/core/command/XQCommandStack.cpp
    src/core/command/XQProjectCommands.cpp
    src/core/command/XQSceneCommands.cpp
    src/core/io/PathSafety.cpp
)

target_include_directories(xq_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Qt6 6.7.0 EXACT CONFIG REQUIRED COMPONENTS Core Widgets)

add_library(xq_ui STATIC
    src/ui/XQSceneModel.cpp
    src/ui/XQSceneModel.h
)

set_target_properties(xq_ui PROPERTIES AUTOMOC ON)

target_include_directories(xq_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_ui
    PUBLIC xq_core
    PUBLIC Qt6::Core
    PUBLIC Qt6::Gui
)

set(_xq_extra_cmake_prefixes)
foreach(_xq_cmake_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${_xq_cmake_prefix}/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake")
        list(APPEND _xq_extra_cmake_prefixes "${_xq_cmake_prefix}/vtk-9.3.0")
    endif()
endforeach()
list(APPEND CMAKE_PREFIX_PATH ${_xq_extra_cmake_prefixes})

find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    FiltersCore
    FiltersGeneral
    FiltersGeometry
    FiltersExtraction
    ImagingCore
    InteractionStyle
    IOImage
    IOXML
    RenderingCore
    RenderingImage
    RenderingLOD
    RenderingOpenGL2
    GUISupportQt
    RenderingUI
)
# ITKVtkGlue's installed ITK 5.4 module config performs a second, componentless
# find_package(VTK), which overwrites VTK_LIBRARIES with every installed VTK
# module (including Python wrappers). Freeze the product's explicit C++ set now
# and restore it after ITK capability discovery below.
set(_xq_product_vtk_libraries ${VTK_LIBRARIES})

find_package(tinyxml2 8.0.0 EXACT CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# ITK 5.4.0 (default ON): backs the SimVascular vascular two-phase level set
# section segmenter (third_party/sv_levelset + src/adapters/itk). Installed at
# Externals/install/windows-x64/itk-5.4.0; put that root on CMAKE_PREFIX_PATH.
# The explicit component set covers the current level-set/DICOM adapters and
# the audited vascular capabilities that later children may consume. Header-only
# ITK modules expose module-specific *_LIBRARIES closures instead of same-named
# imported targets; product targets use those bounded closures and never the
# package-wide ITK_LIBRARIES list. Runtime needs ITK bin on PATH (CTest injection
# below; run_xq.bat for the app).
# ---------------------------------------------------------------------------
set(_xq_itk_prefixes)
foreach(_xq_cmake_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${_xq_cmake_prefix}/itk-5.4.0/lib/cmake/ITK-5.4/ITKConfig.cmake")
        list(APPEND _xq_itk_prefixes "${_xq_cmake_prefix}/itk-5.4.0")
    endif()
endforeach()
list(APPEND CMAKE_PREFIX_PATH ${_xq_itk_prefixes})
set(_xq_itk_components
    ITKLevelSets
    ITKImageGradient
    ITKImageLabel
    ITKRegionGrowing
    ITKAnisotropicSmoothing
    ITKImageFeature
    ITKBinaryMathematicalMorphology
    ITKConnectedComponents
    ITKDistanceMap
    ITKFastMarching
    ITKOptimizers
    ITKPath
    ITKImageIntensity
    ITKVtkGlue
    ITKIOGDCM
    ITKIONIFTI
)
find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS ${_xq_itk_components})
set(VTK_LIBRARIES ${_xq_product_vtk_libraries})

# ---------------------------------------------------------------------------
# ITK vascular level-set section segmenter (design: tetgen-adapter template).
# sv_levelset = vendored SimVascular vascular two-phase level-set filter headers
# (header-only ITK templates, no compiled sources -> INTERFACE lib). The adapter
# is the ONLY TU that includes ITK / sv_levelset; the public header carries zero
# ITK types so xq_core / xq_services never link ITK.
# ---------------------------------------------------------------------------
add_library(sv_levelset INTERFACE)
target_include_directories(sv_levelset INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sv_levelset)

add_library(xq_adapter_itk STATIC
    src/adapters/itk/ItkCenterlineSkeletonizer3D.cpp
    src/adapters/itk/ItkAutomaticVesselSegmenter.cpp
    src/adapters/itk/ItkVascularRoiPriorReader.cpp
    src/adapters/itk/ItkVascularPreprocessor.cpp
    src/adapters/itk/ItkVascularSegmentationEvaluator.cpp
    src/adapters/itk/ItkVascularSegmenter.cpp
)
target_include_directories(xq_adapter_itk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(xq_adapter_itk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picosha2
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/itk_thickness3d
)
target_include_directories(xq_adapter_itk SYSTEM PRIVATE
    ${ITKAnisotropicSmoothing_INCLUDE_DIRS}
    ${ITKBinaryMathematicalMorphology_INCLUDE_DIRS}
    ${ITKConnectedComponents_INCLUDE_DIRS}
    ${ITKDistanceMap_INCLUDE_DIRS}
    ${ITKImageFeature_INCLUDE_DIRS}
    ${ITKImageIntensity_INCLUDE_DIRS}
    ${ITKLevelSets_INCLUDE_DIRS}
    ${ITKImageGradient_INCLUDE_DIRS}
    ${ITKImageLabel_INCLUDE_DIRS}
    ${ITKRegionGrowing_INCLUDE_DIRS}
    ${ITKIONIFTI_INCLUDE_DIRS}
)
target_link_libraries(xq_adapter_itk
    PUBLIC xq_core
    PRIVATE
        sv_levelset
        ${ITKAnisotropicSmoothing_LIBRARIES}
        ${ITKBinaryMathematicalMorphology_LIBRARIES}
        ${ITKConnectedComponents_LIBRARIES}
        ${ITKDistanceMap_LIBRARIES}
        ${ITKImageFeature_LIBRARIES}
        ${ITKImageIntensity_LIBRARIES}
        ${ITKLevelSets_LIBRARIES}
        ${ITKImageGradient_LIBRARIES}
        ${ITKImageLabel_LIBRARIES}
        ${ITKRegionGrowing_LIBRARIES}
        ${ITKIONIFTI_LIBRARIES}
)
if(MSVC)
    # Vendored SV/ITK templates: /utf-8 for the CRLF-ASCII headers, /bigobj for
    # the heavy template instantiation, /wd* to quiet the warning flood
    # (batch-0 spike verified this set builds).
    target_compile_options(xq_adapter_itk PRIVATE /utf-8 /bigobj /wd4996 /wd4267 /wd4244 /wd4305)
endif()

add_executable(test_itk_vascular_segmenter tests/adapters/itk/test_itk_vascular_segmenter.cpp)
target_link_libraries(test_itk_vascular_segmenter PRIVATE xq_adapter_itk xq_core)
if(MSVC)
    target_compile_options(test_itk_vascular_segmenter PRIVATE /utf-8)
endif()
add_test(NAME test_itk_vascular_segmenter COMMAND test_itk_vascular_segmenter)

add_executable(test_itk_vascular_preprocessor
    tests/adapters/itk/test_itk_vascular_preprocessor.cpp
)
target_link_libraries(test_itk_vascular_preprocessor PRIVATE
    xq_adapter_itk
    xq_core
)
if(MSVC)
    target_compile_options(test_itk_vascular_preprocessor PRIVATE /utf-8)
endif()
add_test(NAME test_itk_vascular_preprocessor
    COMMAND test_itk_vascular_preprocessor)

add_executable(test_itk_centerline_skeletonizer_3d
    tests/adapters/itk/test_itk_centerline_skeletonizer_3d.cpp
)
target_link_libraries(test_itk_centerline_skeletonizer_3d PRIVATE
    xq_adapter_itk
    xq_core
)
if(MSVC)
    target_compile_options(test_itk_centerline_skeletonizer_3d PRIVATE /utf-8)
endif()
add_test(NAME test_itk_centerline_skeletonizer_3d
    COMMAND test_itk_centerline_skeletonizer_3d)

add_executable(test_itk_automatic_vessel_segmenter
    tests/adapters/itk/test_itk_automatic_vessel_segmenter.cpp
)
target_link_libraries(test_itk_automatic_vessel_segmenter PRIVATE
    xq_adapter_itk
    xq_core
    xq_io
)
target_include_directories(test_itk_automatic_vessel_segmenter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picosha2
)
if(MSVC)
    target_compile_options(test_itk_automatic_vessel_segmenter PRIVATE /utf-8)
endif()
add_test(NAME test_itk_automatic_vessel_segmenter
    COMMAND test_itk_automatic_vessel_segmenter)

# DICOM series adapter: official GDCM performs image-instance metadata access
# and ITKIOGDCM performs pixel decode/series assembly. External types remain in
# the adapter .cpp; callers see only IDicomSeriesReader and XQ-owned values.
add_library(xq_adapter_dicom STATIC
    src/adapters/gdcm/GdcmItkDicomLabelReader.cpp
    src/adapters/gdcm/GdcmItkDicomSeriesReader.cpp
)
target_include_directories(xq_adapter_dicom PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(xq_adapter_dicom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picosha2
)
target_include_directories(xq_adapter_dicom SYSTEM PRIVATE
    ${ITKIOGDCM_INCLUDE_DIRS}
)
target_link_libraries(xq_adapter_dicom
    PUBLIC xq_core
    PRIVATE ITKIOGDCM gdcmMSFF
)
target_compile_definitions(xq_adapter_dicom PRIVATE GDCM_BUILD_SHARED_LIBS)
if(MSVC)
    target_compile_options(xq_adapter_dicom PRIVATE
        /utf-8 /bigobj /wd4996 /wd4267 /wd4244
        /UITK_IMAGEIO_FACTORY_REGISTER_MANAGER
    )
else()
    target_compile_options(xq_adapter_dicom PRIVATE
        -UITK_IMAGEIO_FACTORY_REGISTER_MANAGER
    )
endif()

add_executable(test_dicom_series_adapter
    tests/adapters/gdcm/test_dicom_series_adapter.cpp
)
target_link_libraries(test_dicom_series_adapter PRIVATE
    xq_adapter_dicom xq_core
)
target_compile_definitions(test_dicom_series_adapter PRIVATE
    XQ_DICOM_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
)
if(MSVC)
    target_compile_options(test_dicom_series_adapter PRIVATE /utf-8)
endif()

add_executable(test_dicom_fixture_policy
    tests/adapters/gdcm/test_dicom_fixture_policy.cpp
)
target_link_libraries(test_dicom_fixture_policy PRIVATE gdcmMSFF)
target_compile_definitions(test_dicom_fixture_policy PRIVATE GDCM_BUILD_SHARED_LIBS)
target_compile_definitions(test_dicom_fixture_policy PRIVATE
    XQ_DICOM_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
)
if(MSVC)
    target_compile_options(test_dicom_fixture_policy PRIVATE /utf-8)
endif()

# Reproducibility helper only. Golden fixtures are committed and CTest never
# runs this generator; regeneration is an explicit audited maintenance action.
add_executable(xq_generate_dicom_fixture EXCLUDE_FROM_ALL
    tools/generate_dicom_test_fixture.cpp
)
target_link_libraries(xq_generate_dicom_fixture PRIVATE gdcmMSFF)
target_compile_definitions(xq_generate_dicom_fixture PRIVATE GDCM_BUILD_SHARED_LIBS)
if(MSVC)
    target_compile_options(xq_generate_dicom_fixture PRIVATE /utf-8)
endif()

add_library(xq_adapter_vtk STATIC
    src/adapters/vtk/MDLModelReader.cpp
    src/adapters/vtk/MSHMeshReader.cpp
    src/adapters/vtk/VtkImageAdapter.cpp
)

target_include_directories(xq_adapter_vtk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_adapter_vtk
    PUBLIC xq_core
    PRIVATE ${VTK_LIBRARIES}
    PRIVATE tinyxml2::tinyxml2
)

vtk_module_autoinit(
    TARGETS xq_adapter_vtk
    MODULES ${VTK_LIBRARIES}
)

# XQSceneRenderer is test-only: it is the single-actor offscreen-to-RGBA contract
# harness exercised by test_scene_renderer / test_scene_renderer_progressive /
# test_surface_lod. The product render entry point is XQRenderScene (multi-view
# resident scene). Do not reference XQSceneRenderer from product code.
add_library(xq_visualization STATIC
    src/visualization/XQSceneRenderer.cpp
    src/visualization/XQSceneRenderer.h
    src/visualization/XQRenderScene.cpp
    src/visualization/XQRenderScene.h
    src/visualization/XQRenderTypes.h
    src/visualization/XQCrossSectionResampler.cpp
    src/visualization/XQCrossSectionResampler.h
    src/visualization/SurfaceLodBuilder.cpp
    src/visualization/SurfaceLodBuilder.h
)

target_include_directories(xq_visualization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_visualization
    PUBLIC xq_core
    PRIVATE ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS xq_visualization
    MODULES ${VTK_LIBRARIES}
)

# Interactive 3D render widget. Split out from xq_visualization so the render
# core (XQSceneRenderer) and its offscreen test never get forced to link Qt;
# only this thin widget pulls Qt6::Widgets + VTK's GUISupportQt. AUTOMOC for the
# QWidget subclass.
add_library(xq_render_widget STATIC
    src/visualization/XQSliceViewWidget.cpp
    src/visualization/XQSliceViewWidget.h
    src/visualization/XQVolumeViewWidget.cpp
    src/visualization/XQVolumeViewWidget.h
    src/visualization/XQMprWidget.cpp
    src/visualization/XQMprWidget.h
    src/visualization/XQCrossSectionViewWidget.cpp
    src/visualization/XQCrossSectionViewWidget.h
)

set_target_properties(xq_render_widget PROPERTIES AUTOMOC ON)

target_include_directories(xq_render_widget PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_render_widget
    PUBLIC xq_visualization
    PUBLIC Qt6::Widgets
    PRIVATE ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS xq_render_widget
    MODULES ${VTK_LIBRARIES}
)

add_library(xq_app_shell STATIC
    src/app/XQMainWindow.cpp
    src/app/XQMainWindow.h
    src/app/WorkflowCapabilities.h
    src/app/XQWorkflowSession.cpp
    src/app/XQWorkflowSession.h
    src/app/XQAppStartup.cpp
    src/app/XQAppStartup.h
    src/app/XQAboutDialog.cpp
    src/app/XQAboutDialog.h
    src/app/XQPreferencesDialog.cpp
    src/app/XQPreferencesDialog.h
    src/app/XQTaskRunner.cpp
    src/app/XQTaskRunner.h
    src/ui/panels/XQStageWidgets.cpp
    src/ui/panels/XQStageWidgets.h
    resources/xq_resources.qrc
)

set_target_properties(xq_app_shell PROPERTIES AUTOMOC ON AUTORCC ON)

target_include_directories(xq_app_shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(xq_app_shell PUBLIC
    XQ_ENABLE_FLOW=$<BOOL:${XQ_ENABLE_FLOW}>
)

target_link_libraries(xq_app_shell
    PUBLIC xq_core
    PUBLIC xq_ui
    PUBLIC xq_render_widget
    PUBLIC xq_controllers
    PUBLIC Qt6::Widgets
    PRIVATE xq_adapter_vtk
    PRIVATE xq_adapter_itk
    PRIVATE xq_adapter_dicom
    PRIVATE xq_io
    PRIVATE psapi
)

vtk_module_autoinit(
    TARGETS xq_app_shell
    MODULES ${VTK_LIBRARIES}
)

add_executable(xq_app
    src/app/main.cpp
)

target_link_libraries(xq_app PRIVATE xq_app_shell)

vtk_module_autoinit(
    TARGETS xq_app
    MODULES ${VTK_LIBRARIES}
)

add_library(xq_io STATIC
    src/io/blob/Sha256.cpp
    src/io/blob/BlobStore.cpp
    src/io/blob/MerkleSidecar.cpp
    src/io/source/MmapBlob.cpp
    src/io/source/MappedVoxelSource.cpp
    src/io/source/MappedGeometrySource.cpp
    src/io/vascular/AutomaticVesselSegmentationArtifact.cpp
    src/io/project/CTGRContourReader.cpp
    src/io/project/PTHPathReader.cpp
    src/io/project/SvProjectReader.cpp
    src/io/project/XQProjectReader.cpp
    src/io/project/XQProjectWriter.cpp
)

target_include_directories(xq_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(xq_io PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picosha2
)

target_link_libraries(xq_io
    PUBLIC xq_core
    PRIVATE tinyxml2::tinyxml2
)

target_link_libraries(xq_app PRIVATE xq_io)

# Data-gate utility: inspect any external DICOM directory through the same
# production GDCM/ITK adapter used by the application. Output is restricted to
# safe technical metadata, geometry, fingerprints and diagnostic codes.
add_executable(xq_vascular_dicom_probe
    probes/vascular_data/DicomSeriesProbe.cpp
)
target_link_libraries(xq_vascular_dicom_probe PRIVATE
    xq_adapter_dicom
    xq_io
)
if(MSVC)
    target_compile_options(xq_vascular_dicom_probe PRIVATE /utf-8)
endif()

# Production-path probe: decode a real enhanced CT/CTA through the DICOM
# adapter, then run the same XQ-owned 3D ITK preprocessing API used by services.
add_executable(xq_vascular_preprocess_probe
    probes/vascular_data/VascularPreprocessProbe.cpp
)
target_link_libraries(xq_vascular_preprocess_probe PRIVATE
    xq_adapter_dicom
    xq_adapter_itk
)
if(MSVC)
    target_compile_options(xq_vascular_preprocess_probe PRIVATE /utf-8)
endif()

# Imports offline organ/coarse-vessel NIfTI priors through ITK, aligns them in
# physical LPS space and reports only XQ-owned masks and provenance.
add_executable(xq_vascular_roi_inspect
    probes/vascular_data/VascularRoiInspect.cpp
)
target_link_libraries(xq_vascular_roi_inspect PRIVATE
    xq_adapter_dicom
    xq_adapter_itk
)
if(MSVC)
    target_compile_options(xq_vascular_roi_inspect PRIVATE /utf-8)
endif()

# Development-only scorer used to reproduce and audit the frozen case-1
# topology/MinimalPath profile. Production never reads gold through this target.
add_executable(xq_vascular_roi_development_analyze
    probes/vascular_data/VascularRoiDevelopmentAnalyze.cpp
    third_party/itk_minimal_path/itkIterateNeighborhoodOptimizer.cxx
)
target_link_libraries(xq_vascular_roi_development_analyze PRIVATE
    xq_adapter_dicom
    xq_adapter_itk
    xq_io
    ${ITKBinaryMathematicalMorphology_LIBRARIES}
    ${ITKConnectedComponents_LIBRARIES}
    ${ITKDistanceMap_LIBRARIES}
    ${ITKFastMarching_LIBRARIES}
    ${ITKOptimizers_LIBRARIES}
    ${ITKPath_LIBRARIES}
)
target_include_directories(xq_vascular_roi_development_analyze SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/itk_thickness3d
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/itk_minimal_path
    ${ITKBinaryMathematicalMorphology_INCLUDE_DIRS}
    ${ITKConnectedComponents_INCLUDE_DIRS}
    ${ITKDistanceMap_INCLUDE_DIRS}
    ${ITKFastMarching_INCLUDE_DIRS}
    ${ITKOptimizers_INCLUDE_DIRS}
    ${ITKPath_INCLUDE_DIRS}
)
if(MSVC)
    target_compile_options(xq_vascular_roi_development_analyze PRIVATE
        /utf-8 /bigobj)
endif()

# Gold-free production prediction entry. It accepts CT DICOM, two explicit
# offline ROI files, a versioned built-in profile and an output artifact path.
# Reference masks are opened only by the separate evaluator after this process
# has closed and hashed its output.
add_executable(xq_vascular_segment_predict
    probes/vascular_data/VascularSegmentationPredict.cpp
)
target_link_libraries(xq_vascular_segment_predict PRIVATE
    xq_adapter_dicom
    xq_adapter_itk
    xq_io
)
if(MSVC)
    target_compile_options(xq_vascular_segment_predict PRIVATE /utf-8)
endif()

add_executable(xq_vascular_segment_inspect
    probes/vascular_data/VascularSegmentationInspect.cpp
)
target_link_libraries(xq_vascular_segment_inspect PRIVATE xq_io)
if(MSVC)
    target_compile_options(xq_vascular_segment_inspect PRIVATE /utf-8)
endif()

# Gold-separated evaluator. The prediction artifact is opened and hashed before
# this process invokes the DICOM label reader.
add_executable(xq_vascular_segment_evaluate
    probes/vascular_data/VascularSegmentationEvaluate.cpp
)
target_link_libraries(xq_vascular_segment_evaluate PRIVATE
    xq_adapter_dicom
    xq_adapter_itk
    xq_io
)
if(MSVC)
    target_compile_options(xq_vascular_segment_evaluate PRIVATE /utf-8 /bigobj)
endif()

# Pure domain services. Depends only on xq_core (no Qt / VTK / ITK / plugins).
set(_xq_service_sources
    src/services/path/PathService.cpp
    src/services/path/CenterlineFrameService.cpp
    src/services/path/CenterlineBGraph.cpp
    src/services/path/CenterlineBService.cpp
    src/services/segmentation/SegmentationService.cpp
    src/services/segmentation/ContourExtractionService.cpp
    src/services/modeling/ContourLoftInputBuilder.cpp
    src/services/modeling/ModelingService.cpp
    src/services/meshing/SurfaceMeshService.cpp
    src/services/meshing/VolumeMeshService.cpp
    src/services/ai/FlowMetricsService.cpp
    src/services/ai/AiService.cpp
    src/services/image/DicomServiceSupport.cpp
    src/services/image/DicomImportService.cpp
    src/services/image/ImageResourceResolver.cpp
    src/services/profile/VesselProfileAssembler.cpp
    src/services/profile/VesselProfileImporter.cpp
    src/services/profile/VesselPathSnapshotService.cpp
    src/services/path/VesselPathDump.cpp
    src/services/path/ShellGeometrySmokeService.cpp
    src/services/modules/PathModuleRegistry.cpp
    src/services/resource/GeometryResourceManager.cpp
    src/services/resource/GeometrySourceResolver.cpp
)
if(XQ_ENABLE_FLOW)
    list(APPEND _xq_service_sources
        src/services/flow/BoundaryConditionService.cpp
        src/services/flow/FlowSolver1D.cpp
        src/services/flow/FlowInputAssembler.cpp
        src/services/flow/FlowGeometrySmokeService.cpp
    )
endif()
add_library(xq_services STATIC ${_xq_service_sources})

target_include_directories(xq_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(xq_services PUBLIC
    XQ_ENABLE_FLOW=$<BOOL:${XQ_ENABLE_FLOW}>
)

# GeometryResourceManager.cpp constructs io's MappedVoxelSource; the header only
# exposes core abstractions (IVoxelSource/AssetId), so xq_io is a PRIVATE edge
# (services -> io is acyclic; io never depends on services).
target_link_libraries(xq_services
    PUBLIC xq_core
    PRIVATE xq_io
)

# Data-gate utility: read one enhanced CT/CTA series and one binary DICOM gold
# series through the production adapter, materialize an XQ mask, and emit only
# safe numerical alignment/fingerprint evidence.
add_executable(xq_vascular_label_alignment_probe
    probes/vascular_data/DicomLabelAlignmentProbe.cpp
)
target_link_libraries(xq_vascular_label_alignment_probe PRIVATE
    xq_adapter_dicom
    xq_services
    xq_io
)
if(MSVC)
    target_compile_options(xq_vascular_label_alignment_probe PRIVATE /utf-8)
endif()

# Deterministic, offline repair for legacy public DICOM series that omit
# FrameOfReferenceUID. Original bytes remain untouched; the tool writes a new
# directory and verifies identical decoded pixels and LPS geometry.
add_executable(xq_normalize_dicom_frame
    probes/vascular_data/NormalizeDicomFrame.cpp
)
target_link_libraries(xq_normalize_dicom_frame PRIVATE
    xq_adapter_dicom
    xq_services
    xq_io
    gdcmMSFF
)
target_compile_definitions(xq_normalize_dicom_frame PRIVATE GDCM_BUILD_SHARED_LIBS)
if(MSVC)
    target_compile_options(xq_normalize_dicom_frame PRIVATE /utf-8)
endif()

# Workflow controllers. Thin UI-facing logic (intent -> service -> command ->
# stack -> scene), pure C++: depends only on xq_core + xq_services, NO Qt / VTK,
# so the whole click->service->command->scene chain is headless-testable.
set(_xq_controller_sources
    src/ui/controllers/PathController.cpp
    src/ui/controllers/SegmentationController.cpp
    src/ui/controllers/ModelingController.cpp
    src/ui/controllers/MeshingController.cpp
    src/ui/controllers/AiController.cpp
    src/ui/controllers/VesselProfileController.cpp
    src/ui/controllers/PathModuleController.cpp
    src/ui/controllers/CenterlineBController.cpp
)
if(XQ_ENABLE_FLOW)
    list(APPEND _xq_controller_sources
        src/ui/controllers/FlowController.cpp
        src/ui/controllers/FlowSmokeController.cpp
    )
endif()
add_library(xq_controllers STATIC ${_xq_controller_sources})

target_include_directories(xq_controllers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(xq_controllers PUBLIC
    XQ_ENABLE_FLOW=$<BOOL:${XQ_ENABLE_FLOW}>
)

target_link_libraries(xq_controllers
    PUBLIC xq_core
    PUBLIC xq_services
)

add_executable(test_node_identity
    tests/core/test_node_identity.cpp
)

target_link_libraries(test_node_identity PRIVATE xq_core)

add_executable(test_scene_management
    tests/core/test_scene_management.cpp
)

target_link_libraries(test_scene_management PRIVATE xq_core)

add_executable(test_scene_remove
    tests/core/test_scene_remove.cpp
)

target_link_libraries(test_scene_remove PRIVATE xq_core)

add_executable(test_scene_relations
    tests/core/test_scene_relations.cpp
)

target_link_libraries(test_scene_relations PRIVATE xq_core)

add_executable(test_project_lifecycle
    tests/core/test_project_lifecycle.cpp
)

target_link_libraries(test_project_lifecycle PRIVATE xq_core)

add_executable(test_diagnostics_provenance
    tests/core/test_diagnostics_provenance.cpp
)

target_link_libraries(test_diagnostics_provenance PRIVATE xq_core)

add_executable(test_image_volume
    tests/core/test_image_volume.cpp
)

target_link_libraries(test_image_volume PRIVATE xq_core)

add_executable(test_scale_slot
    tests/core/test_scale_slot.cpp
)

target_link_libraries(test_scale_slot PRIVATE xq_core)

add_executable(test_image_volume_payload
    tests/core/test_image_volume_payload.cpp
)

target_link_libraries(test_image_volume_payload PRIVATE xq_core)

add_executable(test_dicom_series_reader_contract
    tests/core/test_dicom_series_reader_contract.cpp
)

target_link_libraries(test_dicom_series_reader_contract PRIVATE xq_core)

add_executable(test_vessel_profile
    tests/core/test_vessel_profile.cpp
)

target_link_libraries(test_vessel_profile PRIVATE xq_core)

add_executable(test_vessel_path
    tests/core/test_vessel_path.cpp
)

target_link_libraries(test_vessel_path PRIVATE xq_core)

add_executable(test_path
    tests/core/test_path.cpp
)

target_link_libraries(test_path PRIVATE xq_core)

add_executable(test_contour_group
    tests/core/test_contour_group.cpp
)

target_link_libraries(test_contour_group PRIVATE xq_core)

# Contour group path binding + hand-drawn-contour-into-loft seam (P3-2). Links
# xq_services for ContourExtractionService + ContourLoftInputBuilder.
add_executable(test_contour_group_path_binding
    tests/core/test_contour_group_path_binding.cpp
)

target_link_libraries(test_contour_group_path_binding PRIVATE xq_services)

add_executable(test_surface_model
    tests/core/test_surface_model.cpp
)

target_link_libraries(test_surface_model PRIVATE xq_core)

add_executable(test_segmentation_surface
    tests/core/test_segmentation_surface.cpp
)

target_link_libraries(test_segmentation_surface PRIVATE xq_core)

add_executable(test_mesh
    tests/core/test_mesh.cpp
)

target_link_libraries(test_mesh PRIVATE xq_core)

add_executable(test_simulation_case
    tests/core/test_simulation_case.cpp
)

target_link_libraries(test_simulation_case PRIVATE xq_core)

add_executable(test_payload_and_groups
    tests/core/test_payload_and_groups.cpp
)

target_link_libraries(test_payload_and_groups PRIVATE xq_core)

add_executable(test_payload_geometry_assetid
    tests/core/test_payload_geometry_assetid.cpp
)

target_link_libraries(test_payload_geometry_assetid PRIVATE xq_core)

add_executable(test_command_stack
    tests/core/test_command_stack.cpp
)

target_link_libraries(test_command_stack PRIVATE xq_core)

add_executable(test_asset_registry
    tests/core/test_asset_registry.cpp
)

target_link_libraries(test_asset_registry PRIVATE xq_core)

add_executable(test_project_node_batch_command
    tests/core/test_project_node_batch_command.cpp
)

target_link_libraries(test_project_node_batch_command PRIVATE xq_core)

add_executable(test_source_interface
    tests/core/test_source_interface.cpp
)

target_link_libraries(test_source_interface PRIVATE xq_core)

add_executable(test_path_service
    tests/services/path/PathServiceTest.cpp
)

target_link_libraries(test_path_service PRIVATE xq_services)

add_executable(test_centerline_frame_service
    tests/services/path/CenterlineFrameServiceTest.cpp
)

target_link_libraries(test_centerline_frame_service PRIVATE xq_services)

add_executable(test_centerline_b_graph
    tests/services/path/test_centerline_b_graph.cpp
)
target_link_libraries(test_centerline_b_graph PRIVATE xq_services)

add_executable(test_centerline_b_service
    tests/services/path/test_centerline_b_service.cpp
)
target_link_libraries(test_centerline_b_service PRIVATE xq_services)

add_executable(test_vessel_profile_assembler
    tests/services/profile/test_vessel_profile_assembler.cpp
)

target_link_libraries(test_vessel_profile_assembler PRIVATE xq_services)

add_executable(test_vessel_profile_import
    tests/services/profile/test_vessel_profile_import.cpp
)

target_link_libraries(test_vessel_profile_import PRIVATE xq_services)

add_executable(test_vessel_path_snapshot
    tests/services/profile/test_vessel_path_snapshot.cpp
)
target_link_libraries(test_vessel_path_snapshot PRIVATE xq_services)

add_executable(test_shell_geometry_smoke
    tests/services/path/test_shell_geometry_smoke.cpp
)
target_link_libraries(test_shell_geometry_smoke PRIVATE xq_services)

add_executable(test_path_module_registry
    tests/services/modules/test_path_module_registry.cpp
)
target_link_libraries(test_path_module_registry PRIVATE xq_services)

add_executable(test_segmentation_service
    tests/services/segmentation/SegmentationServiceTest.cpp
)

target_link_libraries(test_segmentation_service PRIVATE xq_services)

# Circle / polygon control-point -> 2D contour geometry (P3-2, pure domain).
add_executable(test_contour_extraction
    tests/services/segmentation/test_contour_extraction.cpp
)

target_link_libraries(test_contour_extraction PRIVATE xq_services)

add_executable(test_modeling_service
    tests/services/modeling/ModelingServiceTest.cpp
)

target_link_libraries(test_modeling_service PRIVATE xq_services)

add_executable(test_modeling_integration
    tests/services/modeling/ModelingIntegrationTest.cpp
)

target_link_libraries(test_modeling_integration PRIVATE xq_services xq_io)
target_compile_definitions(test_modeling_integration PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
)

add_executable(test_surface_mesh_service
    tests/services/meshing/SurfaceMeshServiceTest.cpp
)

target_link_libraries(test_surface_mesh_service PRIVATE xq_services)

add_executable(test_volume_mesh_service
    tests/services/meshing/VolumeMeshServiceTest.cpp
)

target_link_libraries(test_volume_mesh_service PRIVATE xq_services)

add_executable(test_meshing_integration
    tests/services/meshing/MeshingIntegrationTest.cpp
)

target_link_libraries(test_meshing_integration PRIVATE xq_services xq_io)
target_compile_definitions(test_meshing_integration PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
)

if(XQ_ENABLE_FLOW)
    add_executable(test_boundary_condition_service
        tests/services/flow/BoundaryConditionServiceTest.cpp
    )
    target_link_libraries(test_boundary_condition_service PRIVATE xq_services)

    add_executable(test_flow_solver_1d
        tests/services/flow/FlowSolver1DTest.cpp
    )
    target_link_libraries(test_flow_solver_1d PRIVATE xq_services)

    add_executable(test_flow_input_assembler
        tests/services/flow/test_flow_input_assembler.cpp
    )
    target_link_libraries(test_flow_input_assembler PRIVATE xq_services)

    add_executable(test_flow_geometry_smoke
        tests/services/flow/test_flow_geometry_smoke.cpp
    )
    target_link_libraries(test_flow_geometry_smoke PRIVATE xq_services)

    add_executable(test_flow_integration
        tests/services/flow/FlowIntegrationTest.cpp
    )
    target_link_libraries(test_flow_integration PRIVATE xq_services xq_io)
    target_compile_definitions(test_flow_integration PRIVATE
        XQ_CTGR_DIR="${_xq_ctgr_dir}"
        XQ_FLOW_DIR="${_xq_flow_dir}"
    )
endif()

add_executable(test_flow_metrics_service
    tests/services/ai/FlowMetricsServiceTest.cpp
)

target_link_libraries(test_flow_metrics_service PRIVATE xq_services)

add_executable(test_ai_service
    tests/services/ai/AiServiceTest.cpp
)

target_link_libraries(test_ai_service PRIVATE xq_services)

add_executable(test_geometry_resource_manager
    tests/services/resource/GeometryResourceManagerTest.cpp
)

target_link_libraries(test_geometry_resource_manager PRIVATE xq_services xq_io)

add_executable(test_geometry_source_resolver
    tests/services/resource/test_geometry_source_resolver.cpp
)

target_link_libraries(test_geometry_source_resolver PRIVATE xq_services xq_io)

add_executable(test_dicom_import_integration
    tests/services/image/test_dicom_import_integration.cpp
)
target_link_libraries(test_dicom_import_integration PRIVATE
    xq_services xq_adapter_dicom
)
target_compile_definitions(test_dicom_import_integration PRIVATE
    XQ_DICOM_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
)
if(MSVC)
    target_compile_options(test_dicom_import_integration PRIVATE /utf-8)
endif()

add_executable(test_image_resource_resolver
    tests/services/image/test_image_resource_resolver.cpp
)
target_link_libraries(test_image_resource_resolver PRIVATE
    xq_services xq_adapter_dicom xq_io
)
target_compile_definitions(test_image_resource_resolver PRIVATE
    XQ_DICOM_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
)
if(MSVC)
    target_compile_options(test_image_resource_resolver PRIVATE /utf-8)
endif()

if(NOT "${XQ_DICOM_TEST_DATA_ROOT}" STREQUAL "")
    add_executable(test_dicom_real_series
        tests/services/image/test_dicom_real_series.cpp
    )
    target_link_libraries(test_dicom_real_series PRIVATE
        xq_services xq_adapter_dicom xq_io
    )
    target_compile_definitions(test_dicom_real_series PRIVATE
        XQ_DICOM_TEST_DATA_ROOT="${_xq_dicom_test_data_root}"
    )
    if(MSVC)
        target_compile_options(test_dicom_real_series PRIVATE /utf-8)
    endif()
endif()

if(XQ_ENABLE_FLOW)
    add_executable(test_ai_integration
        tests/services/ai/AiIntegrationTest.cpp
    )
    target_link_libraries(test_ai_integration PRIVATE xq_services xq_io)
    target_compile_definitions(test_ai_integration PRIVATE
        XQ_CTGR_DIR="${_xq_ctgr_dir}"
        XQ_FLOW_DIR="${_xq_flow_dir}"
    )
endif()

# M7 workflow controllers: headless click->service->command->scene chain. Pure
# C++ (links only xq_controllers), no Qt / VTK -- synthetic inputs throughout.
add_executable(test_workflow_controllers
    tests/ui/controllers/WorkflowControllerTest.cpp
)

target_link_libraries(test_workflow_controllers PRIVATE xq_controllers)

add_executable(test_vessel_profile_controller
    tests/ui/controllers/VesselProfileControllerTest.cpp
)

target_link_libraries(test_vessel_profile_controller PRIVATE xq_controllers)

add_executable(test_path_module_controller
    tests/ui/controllers/PathModuleControllerTest.cpp
)

target_link_libraries(test_path_module_controller PRIVATE xq_controllers)

add_executable(test_centerline_b_controller
    tests/ui/controllers/CenterlineBControllerTest.cpp
)
target_link_libraries(test_centerline_b_controller PRIVATE
    xq_controllers
    xq_io
)

if(XQ_ENABLE_FLOW)
    add_executable(test_flow_smoke_controller
        tests/ui/controllers/FlowSmokeControllerTest.cpp
    )
    target_link_libraries(test_flow_smoke_controller PRIVATE
        xq_controllers
        xq_io
    )

    add_executable(test_shell_a_e2e
        tests/app/test_shell_a_e2e.cpp
    )
    target_link_libraries(test_shell_a_e2e PRIVATE
        xq_controllers
        xq_adapter_dicom
        xq_io
    )
    target_compile_definitions(test_shell_a_e2e PRIVATE
        XQ_SHELL_A_DICOM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
    )
    if(MSVC)
        target_compile_options(test_shell_a_e2e PRIVATE /utf-8)
    endif()

    if(NOT "${XQ_DICOM_TEST_DATA_ROOT}" STREQUAL "")
        add_executable(test_shell_a_real_data
            tests/app/test_shell_a_e2e.cpp
        )
        target_link_libraries(test_shell_a_real_data PRIVATE
            xq_controllers
            xq_adapter_dicom
            xq_io
        )
        target_compile_definitions(test_shell_a_real_data PRIVATE
            XQ_SHELL_A_DICOM_ROOT="${_xq_dicom_test_data_root}"
            XQ_SHELL_A_REAL_DATA=1
        )
        if(MSVC)
            target_compile_options(test_shell_a_real_data PRIVATE /utf-8)
        endif()
    endif()

    # M7 end-to-end workflow over the real 0007 dataset: image -> path -> seg ->
    # model -> mesh -> flow -> ai, with full undo/redo. Reads the real .vti
    # (xq_adapter_vtk) and .ctgr / .flow (xq_io); AI segmentation uses a mock backend.
    add_executable(test_workflow_integration
        tests/app/WorkflowIntegrationTest.cpp
    )
    target_link_libraries(test_workflow_integration PRIVATE
        xq_controllers
        xq_io
        xq_adapter_vtk
    )
    target_compile_definitions(test_workflow_integration PRIVATE
        XQ_CTGR_DIR="${_xq_ctgr_dir}"
        XQ_FLOW_DIR="${_xq_flow_dir}"
        XQ_TEST_VTI_PATH="${_xq_vti_path}"
    )
    vtk_module_autoinit(
        TARGETS test_workflow_integration
        MODULES ${VTK_LIBRARIES}
    )
endif()

# ---------------------------------------------------------------------------
# Optional real ONNX backend. OFF by default: the default build never finds
# onnxruntime nor compiles the onnx adapter / its test. Turn ON only when
# onnxruntime is available in Externals (M6 technical debt).
# ---------------------------------------------------------------------------
option(XQ_ENABLE_ONNX "Build the real ONNX Runtime backend (requires onnxruntime)" OFF)
if(XQ_ENABLE_ONNX)
    find_package(onnxruntime CONFIG REQUIRED)

    add_library(xq_adapter_onnx STATIC
        src/adapters/onnx/OnnxBackend.cpp
    )
    target_include_directories(xq_adapter_onnx PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(xq_adapter_onnx PRIVATE XQ_ENABLE_ONNX)
    target_link_libraries(xq_adapter_onnx
        PUBLIC xq_core
        PRIVATE onnxruntime::onnxruntime
    )

    add_executable(test_onnx_backend
        tests/adapters/onnx/OnnxBackendTest.cpp
    )
    target_link_libraries(test_onnx_backend PRIVATE xq_adapter_onnx)
endif()


# ---------------------------------------------------------------------------
# Optional TetGen volume-mesh adapter. OFF by default: the default build never
# compiles the vendored TetGen kernel, adapter, or test. Turn ON only for the
# explicitly acknowledged research build; production distribution still needs
# an AGPL-compatible model or a commercial TetGen license.
# ---------------------------------------------------------------------------
option(XQ_ENABLE_TETGEN "Build the research-only TetGen volume mesh adapter" OFF)
option(XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY
    "Acknowledge TetGen 1.5 AGPL-3.0-or-later/commercial terms for a research-only build"
    OFF)
if(XQ_ENABLE_TETGEN AND NOT XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY)
    message(FATAL_ERROR
        "XQ_ENABLE_TETGEN=ON is research-only and requires "
        "-DXQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY=ON. This acknowledgement does "
        "not approve proprietary distribution; use an AGPL-compatible model "
        "or obtain a commercial TetGen license.")
endif()
if(XQ_ENABLE_TETGEN)
    # Vendored TetGen self-identifies as 1.5 and carries the SimVascular
    # outsubfaces fix. Library interface needs
    # TETLIBRARY. TetGen is C-style; silence MSVC's warning flood.
    add_library(tetgen STATIC
        third_party/tetgen/tetgen.cxx
        third_party/tetgen/predicates.cxx
    )
    target_include_directories(tetgen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tetgen)
    target_compile_definitions(tetgen PUBLIC TETLIBRARY)
    if(MSVC)
        target_compile_options(tetgen PRIVATE /w)
    endif()

    add_library(xq_adapter_tetgen STATIC src/adapters/tetgen/TetGenTetMesher.cpp)
    target_include_directories(xq_adapter_tetgen
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tetgen
    )
    target_compile_definitions(xq_adapter_tetgen PRIVATE TETLIBRARY XQ_ENABLE_TETGEN)
    target_link_libraries(xq_adapter_tetgen
        PUBLIC xq_core
        PRIVATE tetgen
    )

    add_executable(test_tetgen_volume_mesh tests/adapters/test_tetgen_volume_mesh.cpp)
    target_link_libraries(test_tetgen_volume_mesh PRIVATE xq_adapter_tetgen xq_services xq_core)
    add_test(NAME test_tetgen_volume_mesh COMMAND test_tetgen_volume_mesh)
endif()


# ---------------------------------------------------------------------------
# Optional MMG3D volume remesh adapter (design §2b / §2c / §7). OFF by default.
# Builds the MMG remesher + the TetGen->MMG two-stage ITetMesher + its test.
# Requires XQ_ENABLE_TETGEN=ON: the two-stage mesher links the TetGen adapter as
# its fill stage. MMG 5.3.9 is installed in
# Externals/install/windows-x64/mmg-5.3.9 (no CMake config); put that root on
# CMAKE_PREFIX_PATH. Running test_mmg_volume_mesh needs mmg3d.dll on PATH.
# ---------------------------------------------------------------------------
option(XQ_ENABLE_MMG "Build MMG3D volume remesh adapter" OFF)
if(XQ_ENABLE_MMG AND NOT XQ_ENABLE_TETGEN)
    message(FATAL_ERROR "XQ_ENABLE_MMG requires XQ_ENABLE_TETGEN=ON")
endif()
if(XQ_ENABLE_MMG)
    find_path(MMG_INCLUDE_DIR mmg/mmg3d/libmmg3d.h
        PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES include
        NO_DEFAULT_PATH)
    find_library(MMG3D_LIBRARY mmg3d
        PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    if(NOT MMG_INCLUDE_DIR OR NOT MMG3D_LIBRARY
            OR NOT EXISTS "${MMG_INCLUDE_DIR}/mmg/mmg3d/libmmg3d.h"
            OR NOT EXISTS "${MMG3D_LIBRARY}")
        message(FATAL_ERROR "XQ_ENABLE_MMG=ON but MMG not found (need mmg-5.3.9 root on CMAKE_PREFIX_PATH)")
    endif()

    get_filename_component(_xq_mmg_include_prefix "${MMG_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(_xq_mmg_library_dir "${MMG3D_LIBRARY}" DIRECTORY)
    get_filename_component(_xq_mmg_library_prefix "${_xq_mmg_library_dir}" DIRECTORY)
    file(REAL_PATH "${_xq_mmg_include_prefix}" _xq_mmg_include_prefix)
    file(REAL_PATH "${_xq_mmg_library_prefix}" _xq_mmg_library_prefix)
    if(NOT _xq_mmg_include_prefix STREQUAL _xq_mmg_library_prefix)
        message(FATAL_ERROR
            "MMG headers and library must come from one install prefix: "
            "${_xq_mmg_include_prefix} != ${_xq_mmg_library_prefix}")
    endif()
    if(WIN32 AND NOT EXISTS "${_xq_mmg_library_prefix}/bin/mmg3d.dll")
        message(FATAL_ERROR
            "MMG import library has no matching runtime DLL: "
            "${_xq_mmg_library_prefix}/bin/mmg3d.dll")
    endif()

    add_library(xq_adapter_mmg STATIC
        src/adapters/mmg/MmgVolumeRemesher.cpp
        src/adapters/mmg/TetGenThenMmg.cpp
    )
    target_include_directories(xq_adapter_mmg
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE ${MMG_INCLUDE_DIR}
    )
    target_compile_definitions(xq_adapter_mmg PRIVATE XQ_ENABLE_MMG)
    target_link_libraries(xq_adapter_mmg
        PUBLIC xq_core xq_adapter_tetgen
        PRIVATE ${MMG3D_LIBRARY}
    )

    add_executable(test_mmg_volume_mesh tests/adapters/test_mmg_volume_mesh.cpp)
    target_link_libraries(test_mmg_volume_mesh PRIVATE xq_adapter_mmg xq_adapter_tetgen xq_services xq_core)
    add_test(NAME test_mmg_volume_mesh COMMAND test_mmg_volume_mesh)
    # Running test_mmg_volume_mesh needs mmg3d.dll on PATH.
endif()


# ---------------------------------------------------------------------------
# Wire the selected volume-mesh kernel into the app shell (design §5 plan A).
# Done here, after the adapter targets exist, so the link references resolve.
# The XQMainWindow.cpp guard (#if XQ_ENABLE_MMG / XQ_ENABLE_TETGEN) needs these
# definitions on xq_app_shell to compile the kernel branch and link its symbols.
# OFF/OFF leaves the shell untouched: the controller falls back to star meshing.
# ---------------------------------------------------------------------------
if(XQ_ENABLE_MMG)
    target_link_libraries(xq_app_shell PRIVATE xq_adapter_mmg)
    target_compile_definitions(xq_app_shell PRIVATE XQ_ENABLE_MMG XQ_ENABLE_TETGEN)
elseif(XQ_ENABLE_TETGEN)
    target_link_libraries(xq_app_shell PRIVATE xq_adapter_tetgen)
    target_compile_definitions(xq_app_shell PRIVATE XQ_ENABLE_TETGEN)
endif()


# ---------------------------------------------------------------------------
# Optional throwaway scale probe (task 06-30-scale-probe). OFF by default: the
# default build never compiles probe/, so the mainline stays byte-for-byte
# identical and the full ctest suite is unaffected. Turn ON only to drive the
# real-scale mmap-backed Source interface spike (design §3). Not added to CI
# (no add_test): this is a benchmark/exploration harness, not a green-gate test.
# ---------------------------------------------------------------------------
option(XQ_ENABLE_SCALE_PROBE "Build throwaway scale probe" OFF)
if(XQ_ENABLE_SCALE_PROBE)
    add_executable(scale_probe
        probe/main.cpp
        probe/MmapBlob.cpp
        probe/SyntheticLoad.cpp
        probe/MappedVoxelSource.cpp
        probe/MappedGeometrySource.cpp
        probe/EvictHarness.cpp
    )
    target_link_libraries(scale_probe PRIVATE xq_core xq_io xq_visualization psapi)
    vtk_module_autoinit(TARGETS scale_probe MODULES ${VTK_LIBRARIES})
endif()


add_executable(test_segmentation_integration
    tests/services/segmentation/SegmentationIntegrationTest.cpp
)

target_link_libraries(test_segmentation_integration PRIVATE xq_services xq_adapter_vtk)
target_compile_definitions(test_segmentation_integration PRIVATE
    XQ_TEST_VTI_PATH="${_xq_vti_path}"
)
vtk_module_autoinit(
    TARGETS test_segmentation_integration
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_vtk_image_adapter
    tests/adapters/test_vtk_image_adapter.cpp
)

target_link_libraries(test_vtk_image_adapter PRIVATE xq_adapter_vtk)
target_compile_definitions(test_vtk_image_adapter PRIVATE
    XQ_TEST_VTI_PATH="${_xq_vti_path}"
)
vtk_module_autoinit(
    TARGETS test_vtk_image_adapter
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_mdl_reader
    tests/adapters/test_mdl_reader.cpp
)

target_link_libraries(test_mdl_reader PRIVATE xq_adapter_vtk)
target_compile_definitions(test_mdl_reader PRIVATE
    XQ_MODELS_DIR="${_xq_models_dir}"
)
vtk_module_autoinit(
    TARGETS test_mdl_reader
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_msh_reader
    tests/adapters/test_msh_reader.cpp
)

target_link_libraries(test_msh_reader PRIVATE xq_adapter_vtk)
target_compile_definitions(test_msh_reader PRIVATE
    XQ_MESHES_DIR="${_xq_meshes_dir}"
)
vtk_module_autoinit(
    TARGETS test_msh_reader
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_pth_reader
    tests/io/project/test_pth_reader.cpp
)

target_link_libraries(test_pth_reader PRIVATE xq_io)
target_compile_definitions(test_pth_reader PRIVATE
    XQ_PTH_DIR="${_xq_pth_dir}"
)

add_executable(test_ctgr_reader
    tests/io/project/test_ctgr_reader.cpp
)

target_link_libraries(test_ctgr_reader PRIVATE xq_io)
target_compile_definitions(test_ctgr_reader PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
)

add_executable(test_svproject_reader
    tests/io/test_svproject_reader.cpp
)

target_link_libraries(test_svproject_reader PRIVATE xq_io)
target_compile_definitions(test_svproject_reader PRIVATE
    XQ_SVPROJECT_DIR="${_xq_svproject_dir}"
    XQ_EXPECTED_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/tests/io/fixtures/0007_H_AO_H.expected-scene.json"
)

add_executable(test_project_roundtrip
    tests/io/test_project_roundtrip.cpp
)

target_link_libraries(test_project_roundtrip PRIVATE xq_io)

add_executable(test_new_domain_roundtrip
    tests/io/test_new_domain_roundtrip.cpp
)

target_link_libraries(test_new_domain_roundtrip PRIVATE xq_io)

add_executable(test_project_versioned_save
    tests/io/test_project_versioned_save.cpp
)

target_link_libraries(test_project_versioned_save PRIVATE xq_io)

add_executable(test_shell_domain_persistence
    tests/io/test_shell_domain_persistence.cpp
)

target_link_libraries(test_shell_domain_persistence PRIVATE xq_io)

add_executable(test_asset_metadata_roundtrip
    tests/io/test_asset_metadata_roundtrip.cpp
)

target_link_libraries(test_asset_metadata_roundtrip PRIVATE xq_io)

add_executable(test_blob_store
    tests/io/test_blob_store.cpp
)

target_link_libraries(test_blob_store PRIVATE xq_io)

add_executable(test_merkle_sidecar
    tests/io/test_merkle_sidecar.cpp
)

target_link_libraries(test_merkle_sidecar PRIVATE xq_io)

add_executable(test_mapped_voxel_source
    tests/io/test_mapped_voxel_source.cpp
)

target_link_libraries(test_mapped_voxel_source PRIVATE xq_io)

add_executable(test_mapped_geometry_source
    tests/io/test_mapped_geometry_source.cpp
)

target_link_libraries(test_mapped_geometry_source PRIVATE xq_io)

add_executable(test_payload_roundtrip
    tests/io/test_payload_roundtrip.cpp
)

target_link_libraries(test_payload_roundtrip PRIVATE xq_io)

add_executable(test_project_lazy_geometry
    tests/io/test_project_lazy_geometry.cpp
)

target_link_libraries(test_project_lazy_geometry PRIVATE xq_io)

add_executable(test_scene_model
    tests/ui/test_scene_model.cpp
)

set_target_properties(test_scene_model PROPERTIES AUTOMOC ON)

target_link_libraries(test_scene_model PRIVATE xq_ui Qt6::Core)

add_executable(test_task_runner
    src/app/XQTaskRunner.cpp
    src/app/XQTaskRunner.h
    tests/app/test_task_runner.cpp
)
set_target_properties(test_task_runner PROPERTIES AUTOMOC ON)
target_include_directories(test_task_runner PRIVATE src)
target_link_libraries(test_task_runner PRIVATE Qt6::Core)

add_executable(test_scene_renderer
    tests/visualization/test_scene_renderer.cpp
)

target_link_libraries(test_scene_renderer PRIVATE xq_visualization)
vtk_module_autoinit(
    TARGETS test_scene_renderer
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_surface_lod
    tests/visualization/test_surface_lod.cpp
)

target_link_libraries(test_surface_lod PRIVATE xq_visualization ${VTK_LIBRARIES})
vtk_module_autoinit(
    TARGETS test_surface_lod
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_scene_renderer_progressive
    tests/visualization/test_scene_renderer_progressive.cpp
)

target_link_libraries(test_scene_renderer_progressive PRIVATE xq_visualization)
vtk_module_autoinit(
    TARGETS test_scene_renderer_progressive
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_render_scene
    tests/visualization/test_render_scene.cpp
)

# ${VTK_LIBRARIES} added (as test_surface_lod above) so the B4c slice-camera
# fit test can include <vtkRenderer.h>/<vtkCamera.h> and read GetParallelScale()
# off the exposed vtkRendererHandle (xq_visualization links VTK PRIVATE, so the
# headers do not transit through it).
target_link_libraries(test_render_scene PRIVATE xq_visualization ${VTK_LIBRARIES})
vtk_module_autoinit(
    TARGETS test_render_scene
    MODULES ${VTK_LIBRARIES}
)

# Cross-section resampler discrete invariants (P3-1): reslice frame == expected
# ContourFrame, center-pixel back-projection == pose origin, null image -> null
# output. Pure VTK, no GL / offscreen context (only vtkImageData / vtkImageReslice
# are exercised). ${VTK_LIBRARIES} added like test_render_scene so the test can
# include VTK headers off the opaque handles.
add_executable(test_cross_section_resampler
    tests/visualization/test_cross_section_resampler.cpp
)
target_link_libraries(test_cross_section_resampler PRIVATE xq_visualization ${VTK_LIBRARIES})
vtk_module_autoinit(
    TARGETS test_cross_section_resampler
    MODULES ${VTK_LIBRARIES}
)

# Pick-interaction takeover invariant (defect A): setSeedPickingEnabled flips
# pickStyleActive(). Needs Qt (XQSliceViewWidget is a QWidget) so it links the
# render-widget lib + a QApplication; runs under offscreen QPA where there is no
# interactor, so the invariant reads the intent flag.
add_executable(test_slice_view_pick_style
    tests/visualization/test_slice_view_pick_style.cpp
)
target_link_libraries(test_slice_view_pick_style PRIVATE xq_render_widget ${VTK_LIBRARIES})
vtk_module_autoinit(
    TARGETS test_slice_view_pick_style
    MODULES ${VTK_LIBRARIES}
)

# Header-only chunk-plan partition invariant (no VTK link); ChunkPlan.h lives
# under src/visualization, so just reuse that include root.
add_executable(test_chunk_plan
    tests/visualization/test_chunk_plan.cpp
)
target_include_directories(test_chunk_plan PRIVATE src)

# Headless gateway test for XQWorkflowSession: command push/undo/redo fire the
# scene-changed callback exactly on success. Links the app shell (where the
# session lives); no VTK / test-data needed since the gateway is pure C++.
add_executable(test_workflow_session
    tests/app/test_workflow_session.cpp
)
set_target_properties(test_workflow_session PROPERTIES AUTOMOC ON)
target_link_libraries(test_workflow_session PRIVATE xq_app_shell)

add_executable(test_workflow_capabilities
    tests/app/test_workflow_capabilities.cpp
)
target_link_libraries(test_workflow_capabilities PRIVATE
    xq_app_shell
    xq_io
)

if(NOT XQ_ENABLE_FLOW)
    add_executable(test_shell_a_noflow
        tests/app/test_shell_a_noflow.cpp
    )
    target_link_libraries(test_shell_a_noflow PRIVATE
        xq_app_shell
        xq_adapter_dicom
        xq_io
    )
    target_compile_definitions(test_shell_a_noflow PRIVATE
        XQ_SHELL_A_DICOM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
        XQ_SHELL_A_HISTORY_PROJECT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/shell-a/history-flow-v1.xqproj"
        XQ_BUILD_NINJA_PATH="${CMAKE_BINARY_DIR}/build.ninja"
    )
    if(MSVC)
        target_compile_options(test_shell_a_noflow PRIVATE /utf-8)
    endif()
endif()

add_executable(test_main_window
    tests/app/test_main_window.cpp
)

set_target_properties(test_main_window PROPERTIES AUTOMOC ON)

target_link_libraries(test_main_window PRIVATE
    xq_app_shell
)
target_compile_definitions(test_main_window PRIVATE
    XQ_TEST_VTI_PATH="${_xq_vti_path}"
    XQ_SVPROJECT_DIR="${_xq_svproject_dir}"
)
vtk_module_autoinit(
    TARGETS test_main_window
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_shell_a_gui
    tests/app/test_shell_a_gui.cpp
)
set_target_properties(test_shell_a_gui PROPERTIES AUTOMOC ON)
target_link_libraries(test_shell_a_gui PRIVATE
    xq_app_shell
    xq_adapter_dicom
)
target_compile_definitions(test_shell_a_gui PRIVATE
    XQ_SHELL_A_DICOM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dicom"
)
if(MSVC)
    target_compile_options(test_shell_a_gui PRIVATE /utf-8)
endif()
vtk_module_autoinit(
    TARGETS test_shell_a_gui
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_centerline_b_gui
    tests/app/test_centerline_b_gui.cpp
)
set_target_properties(test_centerline_b_gui PROPERTIES AUTOMOC ON)
target_link_libraries(test_centerline_b_gui PRIVATE xq_app_shell)
if(MSVC)
    target_compile_options(test_centerline_b_gui PRIVATE /utf-8)
endif()
vtk_module_autoinit(
    TARGETS test_centerline_b_gui
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_app_startup
    tests/app/test_app_startup.cpp
)

set_target_properties(test_app_startup PROPERTIES AUTOMOC ON)

target_link_libraries(test_app_startup PRIVATE
    xq_app_shell
    xq_io
)
vtk_module_autoinit(
    TARGETS test_app_startup
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_ui_font_glyphs
    tests/app/test_ui_font_glyphs.cpp
)

target_link_libraries(test_ui_font_glyphs PRIVATE Qt6::Gui Qt6::Widgets)

add_executable(test_i18n_resources
    tests/app/test_i18n_resources.cpp
)

target_link_libraries(test_i18n_resources PRIVATE
    xq_app_shell
)
vtk_module_autoinit(
    TARGETS test_i18n_resources
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_path_stage
    tests/app/test_path_stage.cpp
)

set_target_properties(test_path_stage PROPERTIES AUTOMOC ON)

target_link_libraries(test_path_stage PRIVATE
    xq_app_shell
)
target_compile_definitions(test_path_stage PRIVATE
    XQ_TEST_VTI_PATH="${_xq_vti_path}"
)
vtk_module_autoinit(
    TARGETS test_path_stage
    MODULES ${VTK_LIBRARIES}
)

enable_testing()
add_test(NAME test_portable_test_data_root
    COMMAND ${CMAKE_COMMAND}
        "-DXQ_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_portable_test_data_root.cmake"
)
add_test(NAME test_arch_boundaries
    COMMAND ${CMAKE_COMMAND}
        "-DXQ_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_arch_boundaries.cmake"
)
add_test(NAME test_dependency_baseline
    COMMAND ${CMAKE_COMMAND}
        "-DXQ_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_dependency_baseline.cmake"
)
add_test(NAME test_node_identity COMMAND test_node_identity)
add_test(NAME test_scene_management COMMAND test_scene_management)
add_test(NAME test_scene_remove COMMAND test_scene_remove)
add_test(NAME test_scene_relations COMMAND test_scene_relations)
add_test(NAME test_project_lifecycle COMMAND test_project_lifecycle)
add_test(NAME test_diagnostics_provenance COMMAND test_diagnostics_provenance)
add_test(NAME test_image_volume COMMAND test_image_volume)
add_test(NAME test_scale_slot COMMAND test_scale_slot)
add_test(NAME test_image_volume_payload COMMAND test_image_volume_payload)
add_test(NAME test_dicom_series_reader_contract COMMAND test_dicom_series_reader_contract)
add_test(NAME test_dicom_series_adapter COMMAND test_dicom_series_adapter)
add_test(NAME test_dicom_fixture_policy COMMAND test_dicom_fixture_policy)
add_test(NAME test_vessel_profile COMMAND test_vessel_profile)
add_test(NAME test_vessel_path COMMAND test_vessel_path)
add_test(NAME test_path COMMAND test_path)
add_test(NAME test_contour_group COMMAND test_contour_group)
add_test(NAME test_contour_group_path_binding COMMAND test_contour_group_path_binding)
add_test(NAME test_surface_model COMMAND test_surface_model)
add_test(NAME test_segmentation_surface COMMAND test_segmentation_surface)
add_test(NAME test_mesh COMMAND test_mesh)
add_test(NAME test_simulation_case COMMAND test_simulation_case)
add_test(NAME test_payload_and_groups COMMAND test_payload_and_groups)
add_test(NAME test_payload_geometry_assetid COMMAND test_payload_geometry_assetid)
add_test(NAME test_command_stack COMMAND test_command_stack)
add_test(NAME test_asset_registry COMMAND test_asset_registry)
add_test(NAME test_project_node_batch_command COMMAND test_project_node_batch_command)
add_test(NAME test_source_interface COMMAND test_source_interface)
add_test(NAME test_path_service COMMAND test_path_service)
add_test(NAME test_centerline_frame_service COMMAND test_centerline_frame_service)
add_test(NAME test_centerline_b_graph COMMAND test_centerline_b_graph)
add_test(NAME test_centerline_b_service COMMAND test_centerline_b_service)
add_test(NAME test_centerline_b_controller COMMAND test_centerline_b_controller)
add_test(NAME test_vessel_profile_assembler COMMAND test_vessel_profile_assembler)
add_test(NAME test_vessel_profile_import COMMAND test_vessel_profile_import)
add_test(NAME test_vessel_path_snapshot COMMAND test_vessel_path_snapshot)
add_test(NAME test_shell_geometry_smoke COMMAND test_shell_geometry_smoke)
add_test(NAME test_path_module_registry COMMAND test_path_module_registry)
add_test(NAME test_segmentation_service COMMAND test_segmentation_service)
add_test(NAME test_contour_extraction COMMAND test_contour_extraction)
add_test(NAME test_modeling_service COMMAND test_modeling_service)
add_test(NAME test_modeling_integration COMMAND test_modeling_integration)
add_test(NAME test_surface_mesh_service COMMAND test_surface_mesh_service)
add_test(NAME test_volume_mesh_service COMMAND test_volume_mesh_service)
add_test(NAME test_meshing_integration COMMAND test_meshing_integration)
if(XQ_ENABLE_FLOW)
    add_test(NAME test_boundary_condition_service COMMAND test_boundary_condition_service)
    add_test(NAME test_flow_solver_1d COMMAND test_flow_solver_1d)
    add_test(NAME test_flow_input_assembler COMMAND test_flow_input_assembler)
    add_test(NAME test_flow_geometry_smoke COMMAND test_flow_geometry_smoke)
    add_test(NAME test_flow_integration COMMAND test_flow_integration)
endif()
add_test(NAME test_flow_metrics_service COMMAND test_flow_metrics_service)
add_test(NAME test_ai_service COMMAND test_ai_service)
add_test(NAME test_geometry_resource_manager COMMAND test_geometry_resource_manager)
add_test(NAME test_geometry_source_resolver COMMAND test_geometry_source_resolver)
add_test(NAME test_dicom_import_integration COMMAND test_dicom_import_integration)
add_test(NAME test_image_resource_resolver COMMAND test_image_resource_resolver)
if(TARGET test_dicom_real_series)
    add_test(NAME test_dicom_real_series COMMAND test_dicom_real_series)
endif()
if(XQ_ENABLE_FLOW)
    add_test(NAME test_ai_integration COMMAND test_ai_integration)
endif()
add_test(NAME test_segmentation_integration COMMAND test_segmentation_integration)
add_test(NAME test_vtk_image_adapter COMMAND test_vtk_image_adapter)
add_test(NAME test_mdl_reader COMMAND test_mdl_reader)
add_test(NAME test_msh_reader COMMAND test_msh_reader)
add_test(NAME test_pth_reader COMMAND test_pth_reader)
add_test(NAME test_ctgr_reader COMMAND test_ctgr_reader)
add_test(NAME test_svproject_reader COMMAND test_svproject_reader)
add_test(NAME test_project_roundtrip COMMAND test_project_roundtrip)
add_test(NAME test_new_domain_roundtrip COMMAND test_new_domain_roundtrip)
add_test(NAME test_project_versioned_save COMMAND test_project_versioned_save)
add_test(NAME test_shell_domain_persistence COMMAND test_shell_domain_persistence)
add_test(NAME test_asset_metadata_roundtrip COMMAND test_asset_metadata_roundtrip)
add_test(NAME test_blob_store COMMAND test_blob_store)
add_test(NAME test_merkle_sidecar COMMAND test_merkle_sidecar)
add_test(NAME test_mapped_voxel_source COMMAND test_mapped_voxel_source)
add_test(NAME test_mapped_geometry_source COMMAND test_mapped_geometry_source)
add_test(NAME test_payload_roundtrip COMMAND test_payload_roundtrip)
add_test(NAME test_project_lazy_geometry COMMAND test_project_lazy_geometry)
add_test(NAME test_scene_model COMMAND test_scene_model)
add_test(NAME test_task_runner COMMAND test_task_runner)
add_test(NAME test_scene_renderer COMMAND test_scene_renderer)
add_test(NAME test_surface_lod COMMAND test_surface_lod)
add_test(NAME test_scene_renderer_progressive COMMAND test_scene_renderer_progressive)
add_test(NAME test_render_scene COMMAND test_render_scene)
add_test(NAME test_cross_section_resampler COMMAND test_cross_section_resampler)
add_test(NAME test_slice_view_pick_style COMMAND test_slice_view_pick_style)
add_test(NAME test_chunk_plan COMMAND test_chunk_plan)
add_test(NAME test_workflow_session COMMAND test_workflow_session)
add_test(NAME test_workflow_capabilities COMMAND test_workflow_capabilities)
if(TARGET test_shell_a_noflow)
    add_test(NAME test_shell_a_noflow COMMAND test_shell_a_noflow)
endif()
add_test(NAME test_main_window COMMAND test_main_window)
if(TARGET test_shell_a_gui)
    add_test(NAME test_shell_a_gui COMMAND test_shell_a_gui)
endif()
add_test(NAME test_centerline_b_gui COMMAND test_centerline_b_gui)
add_test(NAME test_app_startup COMMAND test_app_startup)
add_test(NAME test_ui_font_glyphs COMMAND test_ui_font_glyphs)
add_test(NAME test_i18n_resources COMMAND test_i18n_resources)
add_test(NAME test_path_stage COMMAND test_path_stage)
add_test(NAME test_workflow_controllers COMMAND test_workflow_controllers)
add_test(NAME test_vessel_profile_controller COMMAND test_vessel_profile_controller)
add_test(NAME test_path_module_controller COMMAND test_path_module_controller)
if(XQ_ENABLE_FLOW)
    add_test(NAME test_flow_smoke_controller COMMAND test_flow_smoke_controller)
    add_test(NAME test_shell_a_e2e COMMAND test_shell_a_e2e)
    if(TARGET test_shell_a_real_data)
        add_test(NAME test_shell_a_real_data COMMAND test_shell_a_real_data)
    endif()
    add_test(NAME test_workflow_integration COMMAND test_workflow_integration)
endif()

set(_xq_vtk_bin)
set(_xq_tinyxml2_bin)
set(_xq_qt_bin)
set(_xq_qt_plugin_path)
set(_xq_mmg_bin)

# ON-only: when the app shell links the MMG adapter, GUI tests that launch the
# shell (test_main_window) load mmg3d.dll at startup. ctest sets a fixed PATH
# property per test, so the kernel bin must be added there too -- an external
# `set PATH` does not reach the test process.
if(XQ_ENABLE_MMG AND MMG3D_LIBRARY)
    get_filename_component(_xq_mmg_lib_dir "${MMG3D_LIBRARY}" DIRECTORY)
    get_filename_component(_xq_mmg_prefix "${_xq_mmg_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_mmg_prefix}/bin")
        set(_xq_mmg_bin "${_xq_mmg_prefix}/bin")
    endif()
endif()

if(_xq_mmg_bin AND TARGET test_mmg_volume_mesh)
    set(_xq_mmg_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_mmg_test_path "${_xq_mmg_test_path}")
    set_tests_properties(test_mmg_volume_mesh PROPERTIES
        ENVIRONMENT "PATH=${_xq_mmg_bin}\\;${_xq_mmg_test_path}"
    )
endif()

if(DEFINED VTK_DIR)
    get_filename_component(_xq_vtk_cmake_dir "${VTK_DIR}" ABSOLUTE)
    get_filename_component(_xq_vtk_cmake_parent "${_xq_vtk_cmake_dir}" DIRECTORY)
    get_filename_component(_xq_vtk_lib_dir "${_xq_vtk_cmake_parent}" DIRECTORY)
    get_filename_component(_xq_vtk_prefix "${_xq_vtk_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_bin "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_test_path "$ENV{PATH}")
        string(REPLACE ";" "\\;" _xq_vtk_test_path "${_xq_vtk_test_path}")
        set_tests_properties(test_vtk_image_adapter test_scene_renderer test_render_scene test_cross_section_resampler test_surface_lod test_scene_renderer_progressive test_segmentation_integration PROPERTIES
            ENVIRONMENT "PATH=${_xq_vtk_prefix}/bin\\;${_xq_vtk_test_path}"
        )
    endif()
endif()

# ITK bin -> PATH for test_itk_vascular_segmenter (ITK is a dynamic lib: 72 DLLs).
# ITK_DIR points at <prefix>/lib/cmake/ITK-5.4; walk up 3 levels to <prefix>, its
# bin holds the runtime DLLs. Without this the test fails 0xc0000135 (memory
# ctest-environment-overrides-path: ENVIRONMENT PATH is an overriding snapshot).
set(_xq_dicom_runtime_tests
    test_dicom_series_adapter
    test_dicom_fixture_policy
    test_dicom_import_integration
    test_image_resource_resolver
)
if(TARGET test_dicom_real_series)
    list(APPEND _xq_dicom_runtime_tests test_dicom_real_series)
endif()
if(TARGET test_shell_a_e2e)
    list(APPEND _xq_dicom_runtime_tests test_shell_a_e2e)
endif()
if(TARGET test_shell_a_real_data)
    list(APPEND _xq_dicom_runtime_tests test_shell_a_real_data)
endif()

if(DEFINED ITK_DIR)
    get_filename_component(_xq_itk_cmake_dir "${ITK_DIR}" ABSOLUTE)
    get_filename_component(_xq_itk_cmake_parent "${_xq_itk_cmake_dir}" DIRECTORY)
    get_filename_component(_xq_itk_lib_dir "${_xq_itk_cmake_parent}" DIRECTORY)
    get_filename_component(_xq_itk_prefix "${_xq_itk_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_itk_prefix}/bin")
        set(_xq_itk_bin "${_xq_itk_prefix}/bin")
        set(_xq_itk_test_path "$ENV{PATH}")
        string(REPLACE ";" "\\;" _xq_itk_test_path "${_xq_itk_test_path}")
        set_tests_properties(
            test_itk_vascular_segmenter
            test_itk_vascular_preprocessor
            test_itk_automatic_vessel_segmenter
            test_itk_centerline_skeletonizer_3d
            PROPERTIES
            ENVIRONMENT "PATH=${_xq_itk_bin}\\;${_xq_itk_test_path}"
        )

        set(_xq_gdcm_bin "")
        foreach(_xq_cmake_prefix IN LISTS CMAKE_PREFIX_PATH)
            if(EXISTS "${_xq_cmake_prefix}/gdcm-3.0.10/bin/gdcmMSFF.dll")
                set(_xq_gdcm_bin "${_xq_cmake_prefix}/gdcm-3.0.10/bin")
            elseif(EXISTS "${_xq_cmake_prefix}/bin/gdcmMSFF.dll")
                set(_xq_gdcm_bin "${_xq_cmake_prefix}/bin")
            endif()
        endforeach()
        if(_xq_gdcm_bin)
            set_tests_properties(
                ${_xq_dicom_runtime_tests}
                PROPERTIES
                ENVIRONMENT "PATH=${_xq_itk_bin}\\;${_xq_gdcm_bin}\\;${_xq_itk_test_path}"
            )
        else()
            set_tests_properties(
                ${_xq_dicom_runtime_tests}
                PROPERTIES
                ENVIRONMENT "PATH=${_xq_itk_bin}\\;${_xq_itk_test_path}"
            )
        endif()
    endif()
endif()

if(DEFINED tinyxml2_DIR)
    get_filename_component(_xq_tinyxml2_cmake_dir "${tinyxml2_DIR}" ABSOLUTE)
    get_filename_component(_xq_tinyxml2_cmake_parent "${_xq_tinyxml2_cmake_dir}" DIRECTORY)
    get_filename_component(_xq_tinyxml2_lib_dir "${_xq_tinyxml2_cmake_parent}" DIRECTORY)
    get_filename_component(_xq_tinyxml2_prefix "${_xq_tinyxml2_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_tinyxml2_prefix}/bin")
        set(_xq_tinyxml2_bin "${_xq_tinyxml2_prefix}/bin")
        set(_xq_tinyxml2_test_path "$ENV{PATH}")
        string(REPLACE ";" "\\;" _xq_tinyxml2_test_path "${_xq_tinyxml2_test_path}")
        set(_xq_tinyxml2_runtime_tests
            test_pth_reader
            test_ctgr_reader
            test_svproject_reader
            test_modeling_integration
            test_meshing_integration
        )
        if(TARGET test_flow_integration)
            list(APPEND _xq_tinyxml2_runtime_tests test_flow_integration)
        endif()
        if(TARGET test_ai_integration)
            list(APPEND _xq_tinyxml2_runtime_tests test_ai_integration)
        endif()
        set_tests_properties(${_xq_tinyxml2_runtime_tests} PROPERTIES
            ENVIRONMENT "PATH=${_xq_tinyxml2_prefix}/bin\\;${_xq_tinyxml2_test_path}"
        )
    endif()
endif()

if(_xq_vtk_bin AND _xq_tinyxml2_bin)
    set(_xq_mdl_reader_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_mdl_reader_test_path "${_xq_mdl_reader_test_path}")
    set(_xq_vtk_tinyxml2_runtime_tests test_mdl_reader test_msh_reader)
    if(TARGET test_workflow_integration)
        list(APPEND _xq_vtk_tinyxml2_runtime_tests test_workflow_integration)
    endif()
    set_tests_properties(${_xq_vtk_tinyxml2_runtime_tests} PROPERTIES
        ENVIRONMENT "PATH=${_xq_vtk_bin}\\;${_xq_tinyxml2_bin}\\;${_xq_mdl_reader_test_path}"
    )
endif()

foreach(_xq_cmake_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${_xq_cmake_prefix}/bin/Qt6Core.dll")
        set(_xq_qt_bin "${_xq_cmake_prefix}/bin")
        if(EXISTS "${_xq_cmake_prefix}/plugins")
            set(_xq_qt_plugin_path "${_xq_cmake_prefix}/plugins")
        endif()
    endif()
endforeach()

if(_xq_qt_bin)
    set(_xq_qt_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_qt_test_path "${_xq_qt_test_path}")
    set_tests_properties(test_scene_model test_task_runner PROPERTIES
        ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_qt_test_path}"
    )
    if(_xq_qt_plugin_path)
        set_tests_properties(test_ui_font_glyphs PROPERTIES
            ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_qt_test_path};QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_xq_qt_plugin_path}"
        )
    else()
        set_tests_properties(test_ui_font_glyphs PROPERTIES
            ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_qt_test_path};QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

if(_xq_qt_bin AND _xq_vtk_bin)
    set(_xq_main_window_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_main_window_test_path "${_xq_main_window_test_path}")
    set(_xq_mmg_bin_prefix "")
    if(_xq_mmg_bin)
        set(_xq_mmg_bin_prefix "${_xq_mmg_bin}\\;")
    endif()
    # The app shell links xq_io (SimVascular project reader -> tinyxml2), so the
    # GUI shell test needs tinyxml2.dll on its PATH too.
    set(_xq_tinyxml2_bin_prefix "")
    if(_xq_tinyxml2_bin)
        set(_xq_tinyxml2_bin_prefix "${_xq_tinyxml2_bin}\\;")
    endif()
    # The app shell links xq_adapter_itk (default-ON vascular level-set segmenter),
    # so the GUI shell tests need the ITK DLLs on PATH too (else 0xc0000135).
    set(_xq_itk_bin_prefix "")
    if(_xq_itk_bin)
        set(_xq_itk_bin_prefix "${_xq_itk_bin}\\;")
    endif()
    set(_xq_gdcm_bin_prefix "")
    if(_xq_gdcm_bin)
        set(_xq_gdcm_bin_prefix "${_xq_gdcm_bin}\\;")
    endif()
    if(_xq_qt_plugin_path)
        set(_xq_gui_shell_tests test_main_window test_app_startup test_i18n_resources test_path_stage test_workflow_session test_workflow_capabilities test_slice_view_pick_style)
        if(TARGET test_shell_a_gui)
            list(APPEND _xq_gui_shell_tests test_shell_a_gui)
        endif()
        list(APPEND _xq_gui_shell_tests test_centerline_b_gui)
        if(TARGET test_shell_a_noflow)
            list(APPEND _xq_gui_shell_tests test_shell_a_noflow)
        endif()
        set_tests_properties(${_xq_gui_shell_tests} PROPERTIES
            ENVIRONMENT "PATH=${_xq_itk_bin_prefix}${_xq_gdcm_bin_prefix}${_xq_mmg_bin_prefix}${_xq_tinyxml2_bin_prefix}${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_xq_qt_plugin_path}"
        )
    else()
        set(_xq_gui_shell_tests test_main_window test_app_startup test_i18n_resources test_path_stage test_workflow_session test_workflow_capabilities test_slice_view_pick_style)
        if(TARGET test_shell_a_gui)
            list(APPEND _xq_gui_shell_tests test_shell_a_gui)
        endif()
        list(APPEND _xq_gui_shell_tests test_centerline_b_gui)
        if(TARGET test_shell_a_noflow)
            list(APPEND _xq_gui_shell_tests test_shell_a_noflow)
        endif()
        set_tests_properties(${_xq_gui_shell_tests} PROPERTIES
            ENVIRONMENT "PATH=${_xq_itk_bin_prefix}${_xq_gdcm_bin_prefix}${_xq_mmg_bin_prefix}${_xq_tinyxml2_bin_prefix}${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# Real ONNX backend test, only when the optional adapter was built.
if(XQ_ENABLE_ONNX)
    add_test(NAME test_onnx_backend COMMAND test_onnx_backend)
endif()
