cmake_minimum_required(VERSION 3.20)

project(xq_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(XQ_TEST_DATA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../0007_H_AO_H"
    CACHE PATH "Root directory for XQ real sample test data")
get_filename_component(_xq_test_data_root "${XQ_TEST_DATA_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${_xq_test_data_root}" _xq_test_data_root)
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
    src/core/XQSegmentation.cpp
    src/core/XQSegmentationMask.cpp
    src/core/XQPath.cpp
    src/core/XQContourGroup.cpp
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
    src/core/command/XQSceneCommands.cpp
    src/core/io/PathSafety.cpp
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
    src/visualization/XQSceneRenderer.cpp
    src/visualization/XQSceneRenderer.h
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
    src/visualization/XQRenderWidget.cpp
    src/visualization/XQRenderWidget.h
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
)

set_target_properties(xq_app_shell PROPERTIES AUTOMOC ON)

target_include_directories(xq_app_shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xq_app_shell
    PUBLIC xq_core
    PUBLIC xq_ui
    PUBLIC xq_render_widget
    PUBLIC xq_controllers
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
    src/io/blob/Sha256.cpp
    src/io/blob/BlobStore.cpp
    src/io/blob/MerkleSidecar.cpp
    src/io/source/MmapBlob.cpp
    src/io/source/MappedVoxelSource.cpp
    src/io/source/MappedGeometrySource.cpp
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

target_sources(xq_app PRIVATE
    src/app/XQAppStartup.cpp
    src/app/XQAppStartup.h
)

target_link_libraries(xq_app PRIVATE xq_io)

# Pure domain services. Depends only on xq_core (no Qt / VTK / ITK / plugins).
add_library(xq_services STATIC
    src/services/path/PathService.cpp
    src/services/path/CenterlineFrameService.cpp
    src/services/segmentation/SegmentationService.cpp
    src/services/modeling/ContourLoftInputBuilder.cpp
    src/services/modeling/ModelingService.cpp
    src/services/meshing/SurfaceMeshService.cpp
    src/services/meshing/VolumeMeshService.cpp
    src/services/flow/BoundaryConditionService.cpp
    src/services/flow/FlowSolver1D.cpp
    src/services/ai/FlowMetricsService.cpp
    src/services/ai/AiService.cpp
    src/services/resource/GeometryResourceManager.cpp
    src/services/resource/GeometrySourceResolver.cpp
)

target_include_directories(xq_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# GeometryResourceManager.cpp constructs io's MappedVoxelSource; the header only
# exposes core abstractions (IVoxelSource/AssetId), so xq_io is a PRIVATE edge
# (services -> io is acyclic; io never depends on services).
target_link_libraries(xq_services
    PUBLIC xq_core
    PRIVATE xq_io
)

# Workflow controllers. Thin UI-facing logic (intent -> service -> command ->
# stack -> scene), pure C++: depends only on xq_core + xq_services, NO Qt / VTK,
# so the whole click->service->command->scene chain is headless-testable.
add_library(xq_controllers STATIC
    src/ui/controllers/PathController.cpp
    src/ui/controllers/SegmentationController.cpp
    src/ui/controllers/ModelingController.cpp
    src/ui/controllers/MeshingController.cpp
    src/ui/controllers/FlowController.cpp
    src/ui/controllers/AiController.cpp
)

target_include_directories(xq_controllers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

add_executable(test_segmentation_service
    tests/services/segmentation/SegmentationServiceTest.cpp
)

target_link_libraries(test_segmentation_service PRIVATE xq_services)

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

add_executable(test_boundary_condition_service
    tests/services/flow/BoundaryConditionServiceTest.cpp
)

target_link_libraries(test_boundary_condition_service PRIVATE xq_services)

add_executable(test_flow_solver_1d
    tests/services/flow/FlowSolver1DTest.cpp
)

target_link_libraries(test_flow_solver_1d PRIVATE xq_services)

add_executable(test_flow_integration
    tests/services/flow/FlowIntegrationTest.cpp
)

target_link_libraries(test_flow_integration PRIVATE xq_services xq_io)
target_compile_definitions(test_flow_integration PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
    XQ_FLOW_DIR="${_xq_flow_dir}"
)

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

add_executable(test_ai_integration
    tests/services/ai/AiIntegrationTest.cpp
)

target_link_libraries(test_ai_integration PRIVATE xq_services xq_io)
target_compile_definitions(test_ai_integration PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
    XQ_FLOW_DIR="${_xq_flow_dir}"
)

# M7 workflow controllers: headless click->service->command->scene chain. Pure
# C++ (links only xq_controllers), no Qt / VTK -- synthetic inputs throughout.
add_executable(test_workflow_controllers
    tests/ui/controllers/WorkflowControllerTest.cpp
)

target_link_libraries(test_workflow_controllers PRIVATE xq_controllers)

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
# compiles the vendored TetGen kernel, the adapter, nor its test, so it stays
# byte-for-byte identical to the current build. Turn ON to build the production
# PLC tetrahedralization kernel (design §2a / §7).
# ---------------------------------------------------------------------------
option(XQ_ENABLE_TETGEN "Build TetGen volume mesh adapter" OFF)
if(XQ_ENABLE_TETGEN)
    # Vendored TetGen 1.5.1 (+SV outsubfaces fix). Library interface needs
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
    find_path(MMG_INCLUDE_DIR mmg/libmmg.h
        PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES include)
    find_library(MMG3D_LIBRARY mmg3d
        PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib)
    if(NOT MMG_INCLUDE_DIR OR NOT MMG3D_LIBRARY)
        message(FATAL_ERROR "XQ_ENABLE_MMG=ON but MMG not found (need mmg-5.3.9 root on CMAKE_PREFIX_PATH)")
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

add_executable(test_image_viewer
    tests/visualization/test_image_viewer.cpp
)

target_link_libraries(test_image_viewer PRIVATE xq_visualization)
vtk_module_autoinit(
    TARGETS test_image_viewer
    MODULES ${VTK_LIBRARIES}
)

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

# Header-only chunk-plan partition invariant (no VTK link); ChunkPlan.h lives
# under src/visualization, so just reuse that include root.
add_executable(test_chunk_plan
    tests/visualization/test_chunk_plan.cpp
)
target_include_directories(test_chunk_plan PRIVATE src)

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

add_executable(test_app_startup
    src/app/XQAppStartup.cpp
    src/app/XQAppStartup.h
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
add_test(NAME test_payload_and_groups COMMAND test_payload_and_groups)
add_test(NAME test_payload_geometry_assetid COMMAND test_payload_geometry_assetid)
add_test(NAME test_command_stack COMMAND test_command_stack)
add_test(NAME test_asset_registry COMMAND test_asset_registry)
add_test(NAME test_source_interface COMMAND test_source_interface)
add_test(NAME test_path_service COMMAND test_path_service)
add_test(NAME test_centerline_frame_service COMMAND test_centerline_frame_service)
add_test(NAME test_segmentation_service COMMAND test_segmentation_service)
add_test(NAME test_modeling_service COMMAND test_modeling_service)
add_test(NAME test_modeling_integration COMMAND test_modeling_integration)
add_test(NAME test_surface_mesh_service COMMAND test_surface_mesh_service)
add_test(NAME test_volume_mesh_service COMMAND test_volume_mesh_service)
add_test(NAME test_meshing_integration COMMAND test_meshing_integration)
add_test(NAME test_boundary_condition_service COMMAND test_boundary_condition_service)
add_test(NAME test_flow_solver_1d COMMAND test_flow_solver_1d)
add_test(NAME test_flow_integration COMMAND test_flow_integration)
add_test(NAME test_flow_metrics_service COMMAND test_flow_metrics_service)
add_test(NAME test_ai_service COMMAND test_ai_service)
add_test(NAME test_geometry_resource_manager COMMAND test_geometry_resource_manager)
add_test(NAME test_geometry_source_resolver COMMAND test_geometry_source_resolver)
add_test(NAME test_ai_integration COMMAND test_ai_integration)
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
add_test(NAME test_asset_metadata_roundtrip COMMAND test_asset_metadata_roundtrip)
add_test(NAME test_blob_store COMMAND test_blob_store)
add_test(NAME test_merkle_sidecar COMMAND test_merkle_sidecar)
add_test(NAME test_mapped_voxel_source COMMAND test_mapped_voxel_source)
add_test(NAME test_mapped_geometry_source COMMAND test_mapped_geometry_source)
add_test(NAME test_payload_roundtrip COMMAND test_payload_roundtrip)
add_test(NAME test_project_lazy_geometry COMMAND test_project_lazy_geometry)
add_test(NAME test_scene_model COMMAND test_scene_model)
add_test(NAME test_image_viewer COMMAND test_image_viewer)
add_test(NAME test_scene_renderer COMMAND test_scene_renderer)
add_test(NAME test_surface_lod COMMAND test_surface_lod)
add_test(NAME test_scene_renderer_progressive COMMAND test_scene_renderer_progressive)
add_test(NAME test_chunk_plan COMMAND test_chunk_plan)
add_test(NAME test_main_window COMMAND test_main_window)
add_test(NAME test_app_startup COMMAND test_app_startup)
add_test(NAME test_workflow_controllers COMMAND test_workflow_controllers)
add_test(NAME test_workflow_integration COMMAND test_workflow_integration)

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

if(DEFINED VTK_DIR)
    get_filename_component(_xq_vtk_cmake_dir "${VTK_DIR}" ABSOLUTE)
    get_filename_component(_xq_vtk_cmake_parent "${_xq_vtk_cmake_dir}" DIRECTORY)
    get_filename_component(_xq_vtk_lib_dir "${_xq_vtk_cmake_parent}" DIRECTORY)
    get_filename_component(_xq_vtk_prefix "${_xq_vtk_lib_dir}" DIRECTORY)
    if(EXISTS "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_bin "${_xq_vtk_prefix}/bin")
        set(_xq_vtk_test_path "$ENV{PATH}")
        string(REPLACE ";" "\\;" _xq_vtk_test_path "${_xq_vtk_test_path}")
        set_tests_properties(test_vtk_image_adapter test_image_viewer test_scene_renderer test_surface_lod test_scene_renderer_progressive test_segmentation_integration PROPERTIES
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
        set_tests_properties(test_pth_reader test_ctgr_reader test_svproject_reader test_modeling_integration test_meshing_integration test_flow_integration test_ai_integration PROPERTIES
            ENVIRONMENT "PATH=${_xq_tinyxml2_prefix}/bin\\;${_xq_tinyxml2_test_path}"
        )
    endif()
endif()

if(_xq_vtk_bin AND _xq_tinyxml2_bin)
    set(_xq_mdl_reader_test_path "$ENV{PATH}")
    string(REPLACE ";" "\\;" _xq_mdl_reader_test_path "${_xq_mdl_reader_test_path}")
    set_tests_properties(test_mdl_reader test_msh_reader test_workflow_integration PROPERTIES
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
    set(_xq_mmg_bin_prefix "")
    if(_xq_mmg_bin)
        set(_xq_mmg_bin_prefix "${_xq_mmg_bin}\\;")
    endif()
    if(_xq_qt_plugin_path)
        set_tests_properties(test_main_window test_app_startup PROPERTIES
            ENVIRONMENT "PATH=${_xq_mmg_bin_prefix}${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_xq_qt_plugin_path}"
        )
    else()
        set_tests_properties(test_main_window test_app_startup PROPERTIES
            ENVIRONMENT "PATH=${_xq_mmg_bin_prefix}${_xq_qt_bin}\\;${_xq_vtk_bin}\\;${_xq_main_window_test_path};QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# Real ONNX backend test, only when the optional adapter was built.
if(XQ_ENABLE_ONNX)
    add_test(NAME test_onnx_backend COMMAND test_onnx_backend)
endif()
