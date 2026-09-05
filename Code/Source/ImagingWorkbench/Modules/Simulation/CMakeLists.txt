set(lib xqModuleSimulation)

xq_create_module(
    TARGET ${lib}
    EXPORT_DIRECTIVE xqModuleSimulation
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES}
        ${VTK_LIBRARIES}
        ${QT_LIBRARIES}
        ${TINYXML2_LIBRARIES}
        xqModuleCommon
        xqModuleMeshCommon
)

target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
