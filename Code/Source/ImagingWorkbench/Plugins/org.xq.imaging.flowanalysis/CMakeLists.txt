project(org_xq_imaging_flowanalysis)

xq_create_plugin(
    TARGET ${PROJECT_NAME}
    EXPORT_DIRECTIVE XQ_QT_SIMULATION
    MODULE_DEPENDS
        xqModuleCommon
        xqModuleMeshCommon
        xqModuleSimulation
        xqModuleProjectManagement
    PACKAGE_DEPENDS
        Qt6|Widgets+Xml
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../org.xq.core.datamanager/include"
)
