project(org_xq_pipeline_multiphysics)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_MULTIPHYSICS
    MODULE_DEPENDS
        MitkQtWidgets
        xqModuleMultiPhysics
        xqModuleCommon
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)
