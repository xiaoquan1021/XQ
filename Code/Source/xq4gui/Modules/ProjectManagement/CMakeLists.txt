set(lib xqModuleProjectManagement)
xq_create_module(TARGET ${lib} EXPORT_DIRECTIVE xqProjectManagement
    LIBRARY_DEPENDS
        ${MITK_LIBRARIES} ${VTK_LIBRARIES} ${QT_LIBRARIES}
        xqModuleCommon xqModulePath xqModuleSegmentation
        xqModuleModelCommon xqModuleMeshCommon
        xqModuleSimulation)
