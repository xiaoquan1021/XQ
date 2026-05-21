set(lib xqModuleSegmentation)

xq_create_module(
    TARGET ${lib}
    EXPORT_DIRECTIVE xqModuleSegmentation
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES} ${VTK_LIBRARIES} ${ITK_LIBRARIES} ${QT_LIBRARIES}
        MitkContourModel MitkSegmentation MitkMultilabel
        xqModuleCommon xqModulePath
)

target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
