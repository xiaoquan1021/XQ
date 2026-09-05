project(org_xq_core_workspace)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_PROJECTMANAGER
    MODULE_DEPENDS xqModuleProjectManagement
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/include"
)
