set(lib xqModuleCommon)

xq_create_module(
    TARGET ${lib}
    EXPORT_DIRECTIVE xqModuleCommon
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES}
        ${VTK_LIBRARIES}
        ${ITK_LIBRARIES}
        ${QT_LIBRARIES}
        ${TINYXML2_LIBRARIES}
)

target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
