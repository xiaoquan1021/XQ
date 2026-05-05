set(CPP_FILES
  xq_Application.cxx
  xq_ApplicationPluginActivator.cxx
  xq_NewWorkspaceAction.cxx
  xq_OpenWorkspaceAction.cxx
  xq_ImportLegacyAction.cxx
  xq_SaveWorkspaceAction.cxx
  xq_WorkbenchWindowAdvisor.cxx
  xq_AppWorkbenchAdvisor.cxx
  xq_AboutDialog.cxx
  xq_DefaultPerspective.cxx
  xq_ViewerPerspective.cxx
  xq_VisualizationPerspective.cxx
  xq_WelcomePart.cxx
  xq_Main.cxx
  xq_MitkApp.cxx
)

set(MOC_H_FILES
  src/internal/xq_Application.h
  src/internal/xq_ApplicationPluginActivator.h
  src/internal/xq_NewWorkspaceAction.h
  src/internal/xq_OpenWorkspaceAction.h
  src/internal/xq_ImportLegacyAction.h
  src/internal/xq_SaveWorkspaceAction.h
  src/internal/xq_WorkbenchWindowAdvisor.h
  src/internal/xq_AboutDialog.h
  src/internal/xq_DefaultPerspective.h
  src/internal/xq_ViewerPerspective.h
  src/internal/xq_VisualizationPerspective.h
  src/internal/xq_WelcomePart.h
  src/internal/xq_Main.h
  src/internal/xq_MitkApp.h
)

set(UI_FILES
  src/internal/xq_AboutDialog.ui
)

set(CACHED_RESOURCE_FILES
  plugin.xml
  resources/icon.png
  resources/xq.qss
)

set(QRC_FILES
  resources/xqApplication.qrc
)
