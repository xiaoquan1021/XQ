cmake_minimum_required(VERSION 3.20)

project(xq_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(xq_core STATIC
    src/core/NodeId.cpp
    src/core/Diagnostics.cpp
    src/core/XQImageVolume.cpp
    src/core/XQSegmentation.cpp
    src/core/XQPath.cpp
    src/core/XQContourGroup.cpp
    src/core/XQSurfaceModel.cpp
    src/core/XQMesh.cpp
    src/core/XQSimulationCase.cpp
    src/core/XQProject.cpp
    src/core/XQDataNode.cpp
    src/core/XQScene.cpp
)

target_include_directories(xq_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

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
)

set(_xq_extra_cmake_prefixes)
foreach(_xq_cmake_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${_xq_cmake_prefix}/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake")
        list(APPEND _xq_extra_cmake_prefixes "${_xq_cmake_prefix}/vtk-9.3.0")
    endif()
endforeach()
list(APPEND CMAKE_PREFIX_PATH ${_xq_extra_cmake_prefixes})

find_package(VTK 9 REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    ImagingCore
    InteractionStyle
    IOImage
    IOXML
    RenderingCore
    RenderingImage
    RenderingOpenGL2
)

find_package(tinyxml2 CONFIG REQUIRED)

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

add_library(xq_visualization STATIC
    src/visualization/XQImageViewer.cpp
    src/visualization/XQImageViewer.h
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

add_library(xq_app_shell STATIC
    src/app/XQMainWindow.cpp
    src/app/XQMainWindow.h
)

set_target_properties(xq_app_shell PROPERTIES AUTOMOC ON)

target_include_directories(xq_app_shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_app_shell
    PUBLIC xq_core
    PUBLIC xq_ui
    PUBLIC xq_visualization
    PUBLIC Qt6::Widgets
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
    src/io/project/CTGRContourReader.cpp
    src/io/project/PTHPathReader.cpp
    src/io/project/SvProjectReader.cpp
    src/io/project/XQProjectReader.cpp
    src/io/project/XQProjectWriter.cpp
)

target_include_directories(xq_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_io
    PUBLIC xq_core
    PRIVATE tinyxml2::tinyxml2
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

add_executable(test_path
    tests/core/test_path.cpp
)

target_link_libraries(test_path PRIVATE xq_core)

add_executable(test_contour_group
    tests/core/test_contour_group.cpp
)

target_link_libraries(test_contour_group PRIVATE xq_core)

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

add_executable(test_vtk_image_adapter
    tests/adapters/test_vtk_image_adapter.cpp
)

target_link_libraries(test_vtk_image_adapter PRIVATE xq_adapter_vtk)
target_compile_definitions(test_vtk_image_adapter PRIVATE
    XQ_TEST_VTI_PATH="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Images/OSMSC0090-cm.vti"
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
    XQ_MODELS_DIR="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Models"
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
    XQ_MESHES_DIR="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Meshes"
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
    XQ_PTH_DIR="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Paths"
)

add_executable(test_ctgr_reader
    tests/io/project/test_ctgr_reader.cpp
)

target_link_libraries(test_ctgr_reader PRIVATE xq_io)
target_compile_definitions(test_ctgr_reader PRIVATE
    XQ_CTGR_DIR="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Segmentations"
)

add_executable(test_svproject_reader
    tests/io/test_svproject_reader.cpp
)

target_link_libraries(test_svproject_reader PRIVATE xq_io)
target_compile_definitions(test_svproject_reader PRIVATE
    XQ_SVPROJECT_DIR="C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H"
    XQ_EXPECTED_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/tests/io/fixtures/0007_H_AO_H.expected-scene.json"
)

add_executable(test_project_roundtrip
    tests/io/test_project_roundtrip.cpp
)

target_link_libraries(test_project_roundtrip PRIVATE xq_io)

add_executable(test_project_versioned_save
    tests/io/test_project_versioned_save.cpp
)

target_link_libraries(test_project_versioned_save PRIVATE xq_io)

add_executable(test_scene_model
    tests/ui/test_scene_model.cpp
)

set_target_properties(test_scene_model PROPERTIES AUTOMOC ON)

target_link_libraries(test_scene_model PRIVATE xq_ui Qt6::Core)

add_executable(test_image_viewer
    tests/visualization/test_image_viewer.cpp
)

target_link_libraries(test_image_viewer PRIVATE xq_visualization)
vtk_module_autoinit(
    TARGETS test_image_viewer
    MODULES ${VTK_LIBRARIES}
)

add_executable(test_main_window
    tests/app/test_main_window.cpp
)

set_target_properties(test_main_window PROPERTIES AUTOMOC ON)

target_link_libraries(test_main_window PRIVATE
    xq_app_shell
)
vtk_module_autoinit(
    TARGETS test_main_window
    MODULES ${VTK_LIBRARIES}
)

enable_testing()
add_test(NAME test_node_identity COMMAND test_node_identity)
add_test(NAME test_scene_management COMMAND test_scene_management)
add_test(NAME test_scene_remove COMMAND test_scene_remove)
add_test(NAME test_scene_relations COMMAND test_scene_relations)
add_test(NAME test_project_lifecycle COMMAND test_project_lifecycle)
add_test(NAME test_diagnostics_provenance COMMAND test_diagnostics_provenance)
add_test(NAME test_image_volume COMMAND test_image_volume)
add_test(NAME test_path COMMAND test_path)
add_test(NAME test_contour_group COMMAND test_contour_group)
add_test(NAME test_surface_model COMMAND test_surface_model)
add_test(NAME test_segmentation_surface COMMAND test_segmentation_surface)
add_test(NAME test_mesh COMMAND test_mesh)
add_test(NAME test_simulation_case COMMAND test_simulation_case)
add_test(NAME test_vtk_image_adapter COMMAND test_vtk_image_adapter)
add_test(NAME test_mdl_reader COMMAND test_mdl_reader)
add_test(NAME test_msh_reader COMMAND test_msh_reader)
add_test(NAME test_pth_reader COMMAND test_pth_reader)
add_test(NAME test_ctgr_reader COMMAND test_ctgr_reader)
add_test(NAME test_svproject_reader COMMAND test_svproject_reader)
add_test(NAME test_project_roundtrip COMMAND test_project_roundtrip)
add_test(NAME test_project_versioned_save COMMAND test_project_versioned_save)
add_test(NAME test_scene_model COMMAND test_scene_model)
add_test(NAME test_image_viewer COMMAND test_image_viewer)
add_test(NAME test_main_window COMMAND test_main_window)

set(_xq_vtk_bin)
set(_xq_tinyxml2_bin)
set(_xq_qt_bin)
set(_xq_qt_plugin_path)

if(DEFINED VTK_DIR)
    get_filename_component(_xq_vtk_cmake_dir "${VTK_DIR}" ABSOLUTE)
    get_filename_component(_xq_vtk_cmake_parent "${_xq_vtk_cmake_dir}" DIRECTORY)
    get_filename_component(_xq_vtk_lib_dir "${_xq_vtk_cmake_parent}" DIRECTORY)
    get_filename_component(_xq_vtk_prefix "${_xq_vtk_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_bin "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_test_path "$ENV{PATH}")
        string(REPLACE ";" "\\;" _xq_vtk_test_path "${_xq_vtk_test_path}")
        set_tests_properties(test_vtk_image_adapter test_image_viewer PROPERTIES
            ENVIRONMENT "PATH=${_xq_vtk_prefix}/bin\\;${_xq_vtk_test_path}"
        )
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
        set_tests_properties(test_pth_reader test_ctgr_reader test_svproject_reader PROPERTIES
            ENVIRONMENT "PATH=${_xq_tinyxml2_prefix}/bin\\;${_xq_tinyxml2_test_path}"
        )
    endif()
endif()

if(_xq_vtk_bin AND _xq_tinyxml2_bin)
    set(_xq_mdl_reader_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_mdl_reader_test_path "${_xq_mdl_reader_test_path}")
    set_tests_properties(test_mdl_reader test_msh_reader PROPERTIES
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
    set_tests_properties(test_scene_model PROPERTIES
        ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_qt_test_path}"
    )
endif()

if(_xq_qt_bin AND _xq_vtk_bin)
    set(_xq_main_window_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_main_window_test_path "${_xq_main_window_test_path}")
    if(_xq_qt_plugin_path)
        set_tests_properties(test_main_window PROPERTIES
            ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_xq_qt_plugin_path}"
        )
    else()
        set_tests_properties(test_main_window PROPERTIES
            ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()
