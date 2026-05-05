project(org_xq_pipeline_lumencontouring)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_SEGMENTATION
    MODULE_DEPENDS
        MitkQtWidgets
        xqModuleSegmentation
        xqModuleProjectManagement
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/include"
)
