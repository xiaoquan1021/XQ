set(lib xqModuleModelOCCT)
xq_create_module(TARGET ${lib} EXPORT_DIRECTIVE xqModelOCCT
    LIBRARY_DEPENDS ${MITK_LIBRARIES} ${VTK_LIBRARIES} ${QT_LIBRARIES}
        xqModuleCommon xqModuleModelCommon
    PACKAGE_DEPENDS OpenCASCADE)
target_include_directories(${lib} PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_include_directories(${lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
