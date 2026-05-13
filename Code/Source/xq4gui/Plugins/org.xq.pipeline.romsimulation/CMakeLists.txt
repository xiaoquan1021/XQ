project(org_xq_pipeline_romsimulation)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_ROMSIMULATION
    MODULE_DEPENDS
        MitkQtWidgets
        xqModuleROMSimulation
        xqModuleCommon
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)
