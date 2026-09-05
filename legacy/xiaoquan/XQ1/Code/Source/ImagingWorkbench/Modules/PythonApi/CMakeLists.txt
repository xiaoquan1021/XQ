set(lib xqModulePythonApi)

xq_create_module(
    TARGET ${lib}
    EXPORT_DIRECTIVE xqModulePythonApi
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES}
        ${VTK_LIBRARIES}
        ${QT_LIBRARIES}
        xqModuleCommon
        xqModulePath
        xqModuleSegmentation
        xqModuleModelCommon
        xqModuleMeshCommon
        xqModuleSimulation
        xqModuleROMSimulation
        xqModuleMultiPhysics
        xqModuleProjectManagement
)

target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
