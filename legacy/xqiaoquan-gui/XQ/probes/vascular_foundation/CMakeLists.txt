cmake_minimum_required(VERSION 3.24)

project(xq_vascular_foundation_probes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
    message(FATAL_ERROR "The audited XQ dependency baseline requires MSVC")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The audited XQ dependency baseline requires x64")
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "Configure this probe in Release mode")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(XQ_AUDIT_EXPECTED_VTK_DIR "" CACHE PATH "Exact VTK 9.3 CMake package directory")
set(XQ_AUDIT_THICKNESS3D_SOURCE_DIR "" CACHE PATH "Pinned ITKThickness3D source directory")
set(XQ_AUDIT_VMTK_SOURCE_DIR "" CACHE PATH "Pinned SimVascular vtkvmtk source directory")
set(XQ_AUDIT_REAL_SURFACE "" CACHE FILEPATH "Real closed vascular VTP used by the vtkvmtk probe")

foreach(_required IN ITEMS
        XQ_AUDIT_EXPECTED_VTK_DIR
        XQ_AUDIT_THICKNESS3D_SOURCE_DIR
        XQ_AUDIT_VMTK_SOURCE_DIR
        XQ_AUDIT_REAL_SURFACE)
    if(NOT ${_required})
        message(FATAL_ERROR "${_required} must be provided explicitly")
    endif()
endforeach()

if(NOT EXISTS "${XQ_AUDIT_EXPECTED_VTK_DIR}/vtk-config.cmake")
    message(FATAL_ERROR "XQ_AUDIT_EXPECTED_VTK_DIR is not a VTK package directory")
endif()
if(NOT EXISTS "${XQ_AUDIT_THICKNESS3D_SOURCE_DIR}/include/itkBinaryThinningImageFilter3D.h")
    message(FATAL_ERROR "Pinned ITKThickness3D headers are missing")
endif()
if(NOT EXISTS "${XQ_AUDIT_VMTK_SOURCE_DIR}/vtkvmtkPolyDataCenterlines.cxx")
    message(FATAL_ERROR "Pinned vtkvmtk centerline sources are missing")
endif()
if(NOT EXISTS "${XQ_AUDIT_REAL_SURFACE}")
    message(FATAL_ERROR "The real vascular surface does not exist")
endif()

set(VTK_DIR "${XQ_AUDIT_EXPECTED_VTK_DIR}" CACHE PATH "" FORCE)
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    CommonMath
    FiltersCore
    FiltersGeneral
    FiltersGeometry
    IOXML
)

find_package(ITK 5.4.0 EXACT REQUIRED COMPONENTS
    ITKAnisotropicSmoothing
    ITKImageFeature
    ITKBinaryMathematicalMorphology
    ITKConnectedComponents
    ITKDistanceMap
    ITKVtkGlue
)
include(${ITK_USE_FILE})

cmake_path(NORMAL_PATH VTK_DIR OUTPUT_VARIABLE _actual_vtk_dir)
cmake_path(NORMAL_PATH XQ_AUDIT_EXPECTED_VTK_DIR OUTPUT_VARIABLE _expected_vtk_dir)
if(NOT _actual_vtk_dir STREQUAL _expected_vtk_dir)
    message(FATAL_ERROR
        "ITKVtkGlue selected '${_actual_vtk_dir}', expected '${_expected_vtk_dir}'")
endif()
if(NOT VTK_VERSION VERSION_EQUAL "9.3.0")
    message(FATAL_ERROR "Expected VTK 9.3.0, found ${VTK_VERSION}")
endif()
if(NOT ITK_VERSION VERSION_EQUAL "5.4.0")
    message(FATAL_ERROR "Expected ITK 5.4.0, found ${ITK_VERSION}")
endif()

message(STATUS "Audit compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Audit architecture: ${CMAKE_SIZEOF_VOID_P}-byte pointers")
message(STATUS "Audit runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "Audit ITK: ${ITK_VERSION} at ${ITK_DIR}")
message(STATUS "Audit VTK: ${VTK_VERSION} at ${VTK_DIR}")

add_executable(itk_vascular_filters_probe itk_vascular_filters_probe.cpp)
target_link_libraries(itk_vascular_filters_probe PRIVATE ${ITK_LIBRARIES})

add_executable(itk_vtk_bridge_probe itk_vtk_bridge_probe.cpp)
target_link_libraries(itk_vtk_bridge_probe PRIVATE
    ${ITK_LIBRARIES}
    VTK::CommonCore
    VTK::CommonDataModel
)

add_executable(itk_thickness3d_probe itk_thickness3d_probe.cpp)
target_include_directories(itk_thickness3d_probe PRIVATE
    "${XQ_AUDIT_THICKNESS3D_SOURCE_DIR}/include"
)
target_link_libraries(itk_thickness3d_probe PRIVATE ${ITK_LIBRARIES})

set(_vmtk_minimal_sources
    vtkvmtkInternalTetrahedraExtractor.cxx
    vtkvmtkMinHeap.cxx
    vtkvmtkNonManifoldFastMarching.cxx
    vtkvmtkNonManifoldSteepestDescent.cxx
    vtkvmtkPolyDataCenterlines.cxx
    vtkvmtkSimplifyVoronoiDiagram.cxx
    vtkvmtkSteepestDescentLineTracer.cxx
    vtkvmtkVoronoiDiagram3D.cxx
)
list(TRANSFORM _vmtk_minimal_sources PREPEND "${XQ_AUDIT_VMTK_SOURCE_DIR}/")

add_library(vtkvmtk_centerline_minimal STATIC ${_vmtk_minimal_sources})
target_include_directories(vtkvmtk_centerline_minimal PUBLIC "${XQ_AUDIT_VMTK_SOURCE_DIR}")
target_link_libraries(vtkvmtk_centerline_minimal PUBLIC
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::CommonExecutionModel
    VTK::CommonMath
    VTK::FiltersCore
    VTK::FiltersGeneral
    VTK::FiltersGeometry
)

file(TO_CMAKE_PATH "${XQ_AUDIT_REAL_SURFACE}" _real_surface)
add_executable(vtkvmtk_centerline_probe vtkvmtk_centerline_probe.cpp)
target_compile_definitions(vtkvmtk_centerline_probe PRIVATE
    XQ_AUDIT_REAL_SURFACE="${_real_surface}"
)
target_link_libraries(vtkvmtk_centerline_probe PRIVATE
    vtkvmtk_centerline_minimal
    VTK::FiltersCore
    VTK::FiltersGeneral
    VTK::FiltersGeometry
    VTK::IOXML
)

if(MSVC)
    foreach(_target IN ITEMS
            itk_vascular_filters_probe
            itk_vtk_bridge_probe
            itk_thickness3d_probe
            vtkvmtk_centerline_minimal
            vtkvmtk_centerline_probe)
        target_compile_options(${_target} PRIVATE /utf-8 /bigobj)
    endforeach()
endif()

enable_testing()
add_test(NAME itk_vascular_filters_probe COMMAND itk_vascular_filters_probe)
add_test(NAME itk_vtk_bridge_probe COMMAND itk_vtk_bridge_probe)
add_test(NAME itk_thickness3d_probe COMMAND itk_thickness3d_probe)
add_test(NAME vtkvmtk_centerline_probe COMMAND vtkvmtk_centerline_probe)

get_filename_component(_itk_cmake_parent "${ITK_DIR}" DIRECTORY)
get_filename_component(_itk_lib_dir "${_itk_cmake_parent}" DIRECTORY)
get_filename_component(_itk_prefix "${_itk_lib_dir}" DIRECTORY)
get_filename_component(_vtk_cmake_parent "${VTK_DIR}" DIRECTORY)
get_filename_component(_vtk_lib_dir "${_vtk_cmake_parent}" DIRECTORY)
get_filename_component(_vtk_prefix "${_vtk_lib_dir}" DIRECTORY)
set(_host_path "$ENV{PATH}")
string(REPLACE ";" "\\;" _host_path "${_host_path}")
set_tests_properties(
    itk_vascular_filters_probe
    itk_vtk_bridge_probe
    itk_thickness3d_probe
    vtkvmtk_centerline_probe
    PROPERTIES ENVIRONMENT "PATH=${_itk_prefix}/bin\\;${_vtk_prefix}/bin\\;${_host_path}"
)
