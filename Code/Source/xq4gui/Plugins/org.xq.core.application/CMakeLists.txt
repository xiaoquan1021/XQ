project(org_xq_core_application)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_APPLICATION
    MODULE_DEPENDS MitkAppUtil MitkSceneSerialization org_mitk_gui_qt_application xqModuleProjectManagement
)
