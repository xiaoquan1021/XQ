set(lib xqModulePath)

xq_create_module(
    TARGET ${lib}
    EXPORT_DIRECTIVE xqModulePath
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES}
        ${VTK_LIBRARIES}
        ${ITK_LIBRARIES}
        ${QT_LIBRARIES}
        xqModuleCommon
)

target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
