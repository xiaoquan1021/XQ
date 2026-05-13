# Regression tests for seg preprocess-architecture slice 1
# Built as part of the XQ Code build tree (so all dependencies are already found).

add_executable(test_seg_preprocess
    test_seg_preprocess.cxx
)

target_link_libraries(test_seg_preprocess
    PRIVATE
        xqModuleCommon
        xqModuleSegmentation
        xqModuleModelCommon
        xqModuleMeshCommon
        xqModuleSimulation
        xqModuleProjectManagement
        tinyxml2::tinyxml2
)

# Include the Segmentation module's own source dir for its private headers
target_include_directories(test_seg_preprocess
    PRIVATE
        "${XQ_MODULE_BASE_DIR}/Common"
        "${XQ_MODULE_BASE_DIR}/Segmentation"
        "${XQ_MODULE_BASE_DIR}/Path"
        "${XQ_MODULE_BASE_DIR}/Model/Common"
        "${XQ_MODULE_BASE_DIR}/Mesh/Common"
        "${XQ_MODULE_BASE_DIR}/Simulation"
        "${XQ_MODULE_BASE_DIR}/ProjectManagement"
)

# CppMicroServices requires every TU that includes MITK headers to define US_MODULE_NAME
target_compile_definitions(test_seg_preprocess
    PRIVATE
        US_MODULE_NAME=test_seg_preprocess
)

# Place binary alongside other XQ binaries
set_target_properties(test_seg_preprocess PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Regression tests for Project save/load round-trip (Task B)
add_executable(test_project_roundtrip
    test_project_roundtrip.cxx
)

target_link_libraries(test_project_roundtrip
    PRIVATE
        xqModuleCommon
        xqModuleSegmentation
        xqModuleModelCommon
        xqModuleMeshCommon
        xqModuleSimulation
        xqModuleProjectManagement
        tinyxml2::tinyxml2
)

target_include_directories(test_project_roundtrip
    PRIVATE
        "${XQ_MODULE_BASE_DIR}/Common"
        "${XQ_MODULE_BASE_DIR}/Segmentation"
        "${XQ_MODULE_BASE_DIR}/Path"
        "${XQ_MODULE_BASE_DIR}/Model/Common"
        "${XQ_MODULE_BASE_DIR}/Mesh/Common"
        "${XQ_MODULE_BASE_DIR}/Simulation"
        "${XQ_MODULE_BASE_DIR}/ProjectManagement"
)

target_compile_definitions(test_project_roundtrip
    PRIVATE
        US_MODULE_NAME=test_project_roundtrip
)

set_target_properties(test_project_roundtrip PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
