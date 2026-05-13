project(org_xq_core_application)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_APPLICATION
    MODULE_DEPENDS MitkAppUtil MitkSceneSerialization org_mitk_gui_qt_application xqModuleProjectManagement org_xq_core_datamanager
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/src/internal"
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/include"
  "${CMAKE_BINARY_DIR}/Code/Source/xq4gui/Plugins/org.xq.core.datamanager"
)
