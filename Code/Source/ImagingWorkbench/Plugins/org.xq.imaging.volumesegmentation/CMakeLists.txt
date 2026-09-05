project(org_xq_imaging_volumesegmentation)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_MITKSEGMENTATION
    MODULE_DEPENDS
        MitkQtWidgets
        MitkSegmentationUI
        MitkSegmentation
        xqModuleSegmentation
        xqModuleCommon
    PACKAGE_DEPENDS
        Qt6|Widgets
)
