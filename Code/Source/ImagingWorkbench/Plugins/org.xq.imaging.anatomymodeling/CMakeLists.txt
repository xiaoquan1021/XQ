project(org_xq_imaging_anatomymodeling)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_MODELING
    MODULE_DEPENDS
        xqModuleCommon
        xqModulePath
        xqModuleSegmentation
        xqModuleModelCommon
        xqModuleModelOCCT
        xqModuleProjectManagement
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/include"
)
