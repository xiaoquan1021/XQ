# Regression tests for seg preprocess-architecture slice 1
# Built as part of the XQ Code build tree (so all dependencies are already found).

set(XQ_TEST_LIBRARY_PATHS
    "${CMAKE_BINARY_DIR}/lib"
    "${CMAKE_BINARY_DIR}/lib/plugins"
    "${CMAKE_BINARY_DIR}/bin"
    "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build/lib"
    "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build/lib/plugins"
    "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/lib"
    "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/src/CTK-build/CTK-build/bin"
)

if(WIN32)
    list(APPEND XQ_TEST_LIBRARY_PATHS
        "${XQ_EXTERNALS_DIR}/python-3.11.0"
        "${XQ_EXTERNALS_DIR}/hdf5-1.14.3/bin"
        "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/bin"
        "${XQ_EXTERNALS_DIR}/vtk-9.3.0/bin"
        "${XQ_EXTERNALS_DIR}/itk-5.4.0/bin"
        "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin"
        "${XQ_EXTERNALS_DIR}/mitk-2024.06/bin"
        "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/bin"
        "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/win64/vc14/bin"
        "${XQ_EXTERNALS_DIR}/freetype-2.13.0/bin"
        "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/bin"
        "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/bin/plugins"
    )
    cmake_path(CONVERT "${XQ_TEST_LIBRARY_PATHS}" TO_NATIVE_PATH_LIST XQ_TEST_LIBRARY_PATH NORMALIZE)
    string(REPLACE ";" "\\;" XQ_TEST_LIBRARY_PATH_ESCAPED "${XQ_TEST_LIBRARY_PATH}")
    string(REPLACE ";" "\\;" XQ_HOST_PATH_ESCAPED "$ENV{PATH}")
    set(XQ_TEST_ENVIRONMENT
        "PATH=${XQ_TEST_LIBRARY_PATH_ESCAPED}\;${XQ_HOST_PATH_ESCAPED}"
        "QT_PLUGIN_PATH=${XQ_EXTERNALS_DIR}/qt-6.7.0/plugins"
        "XQ_PLUGIN_PATH=${CMAKE_BINARY_DIR}/lib/plugins"
    )
else()
    list(APPEND XQ_TEST_LIBRARY_PATHS
        "${XQ_EXTERNALS_DIR}/python-3.11.0/lib"
        "${XQ_EXTERNALS_DIR}/hdf5-1.14.3/lib"
        "${XQ_EXTERNALS_DIR}/hdf5-1.12.2/lib"
        "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib"
        "${XQ_EXTERNALS_DIR}/vtk-9.3.0/lib"
        "${XQ_EXTERNALS_DIR}/itk-5.4.0/lib"
        "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib"
        "${XQ_EXTERNALS_DIR}/mitk-2024.06/lib"
        "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/lib"
        "${XQ_EXTERNALS_DIR}/freetype-2.13.0/lib"
    )
    list(JOIN XQ_TEST_LIBRARY_PATHS ":" XQ_TEST_LIBRARY_PATH)
endif()

function(xq_set_test_runtime_environment target)
    if(WIN32)
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT "${XQ_TEST_ENVIRONMENT}"
        )
    else()
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT "LD_LIBRARY_PATH=${XQ_TEST_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH};QT_PLUGIN_PATH=${XQ_EXTERNALS_DIR}/qt-6.7.0/plugins"
        )
    endif()
endfunction()

function(xq_add_registered_test target)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${target}.cxx")
        message(STATUS "Skipping ${target}: source file not present")
        return()
    endif()

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
    xq_set_test_runtime_environment(${target})
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
        xqModuleROMSimulation
        xqModuleMultiPhysics
        xqModulePythonApi
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
        "${XQ_MODULE_BASE_DIR}/ROMSimulation"
        "${XQ_MODULE_BASE_DIR}/MultiPhysics"
        "${XQ_MODULE_BASE_DIR}/PythonApi"
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
xq_set_test_runtime_environment(test_seg_preprocess)

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
        xqModuleROMSimulation
        xqModuleMultiPhysics
        xqModulePythonApi
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
        "${XQ_MODULE_BASE_DIR}/ROMSimulation"
        "${XQ_MODULE_BASE_DIR}/MultiPhysics"
        "${XQ_MODULE_BASE_DIR}/PythonApi"
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
xq_set_test_runtime_environment(test_project_roundtrip)

xq_add_registered_test(test_image_processing
    xqModuleImageProcessing
    xqModuleProjectManagement
    xqModuleCommon
    tinyxml2::tinyxml2
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

xq_add_registered_test(test_simple_flow_solver_backend
    xqModuleSimulation
    xqModuleMeshCommon
    xqModuleModelCommon
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

if(TARGET xqMonolithCore)
    xq_add_registered_test(test_monolith_workflows
        xqMonolithCore
        Qt6::Core
    )

    xq_add_registered_test(test_monolith_application_context
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_project_service
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_selection_context
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_task_runner
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_preferences_service
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_data_catalog
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_project_data_persistence
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_data_import_service
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_data_import_context
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_project_session_service
        xqMonolithCore
        Qt6::Core
        MitkCore
    )

    xq_add_registered_test(test_monolith_data_hierarchy
        xqMonolithCore
        Qt6::Core
        MitkCore
    )
endif()

xq_add_registered_test(test_seg3d_to_model
    xqModuleSegmentation
    xqModulePath
    xqModuleCommon
)
