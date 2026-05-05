set(lib xqModuleMeshCommon)
xq_create_module(TARGET ${lib} EXPORT_DIRECTIVE xqMeshCommon
    LIBRARY_DEPENDS ${MITK_LIBRARIES} ${VTK_LIBRARIES} ${QT_LIBRARIES}
        xqModuleCommon xqModuleModelCommon)
target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
