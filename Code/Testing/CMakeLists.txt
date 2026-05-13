# Regression tests for seg preprocess-architecture slice 1
# Built as part of the XQ Code build tree (so all dependencies are already found).

function(xq_add_registered_test target)
    add_executable(${target}
        ${target}.cxx
    )
    target_link_libraries(${target}
        PRIVATE
            ${ARGN}
    )
    target_include_directories(${target}
        PRIVATE
            "${XQ_MODULE_BASE_DIR}/Common"
            "${XQ_MODULE_BASE_DIR}/ImageProcessing"
            "${XQ_MODULE_BASE_DIR}/Segmentation"
            "${XQ_MODULE_BASE_DIR}/Path"
            "${XQ_MODULE_BASE_DIR}/Model/Common"
            "${XQ_MODULE_BASE_DIR}/Mesh/Common"
            "${XQ_MODULE_BASE_DIR}/Simulation"
            "${XQ_MODULE_BASE_DIR}/ProjectManagement"
            "${XQ_MODULE_BASE_DIR}/MultiPhysics"
            "${XQ_MODULE_BASE_DIR}/ROMSimulation"
            "${XQ_MODULE_BASE_DIR}/PythonApi"
    )
    target_compile_definitions(${target}
        PRIVATE
            US_MODULE_NAME=${target}
    )
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(test_seg_preprocess
    test_seg_preprocess.cxx
)

target_link_libraries(test_seg_preprocess
    PRIVATE
        xqModuleCommon
        xqModulePath
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
add_test(NAME test_seg_preprocess COMMAND test_seg_preprocess)

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
add_test(NAME test_project_roundtrip COMMAND test_project_roundtrip)

xq_add_registered_test(test_image_processing
    xqModuleImageProcessing
    xqModuleCommon
)

xq_add_registered_test(test_levelset_segmentation
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_simulation_result_postprocess
    xqModuleSimulation
    xqModuleMeshCommon
    xqModuleModelCommon
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_mesh_adaptor
    xqModuleMeshCommon
    xqModuleModelCommon
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_multiphysics_job_xml
    xqModuleMultiPhysics
    xqModuleCommon
)

xq_add_registered_test(test_rom_job_contract
    xqModuleROMSimulation
    xqModuleCommon
)

xq_add_registered_test(test_python_api_contract
    xqModulePythonApi
    xqModuleProjectManagement
    xqModuleSimulation
    xqModuleMeshCommon
    xqModuleModelCommon
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_model_geometry_quality
    xqModuleModelCommon
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_ml_segmentation_placeholder
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)

xq_add_registered_test(test_seg3d_to_model
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)
