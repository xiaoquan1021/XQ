project(org_xq_pipeline_imageprocessing)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_IMAGEPROCESSING
    MODULE_DEPENDS
        MitkQtWidgets
        xqModuleImageProcessing
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)
