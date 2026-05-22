set(CPP_FILES
  xq_PathPlanningPlugin.cxx
  xq_VesselPlanningView.cxx
  xq_PathCreate.cxx
  xq_CenterlineSmoother.cxx
  xq_PathPreferencePage.cxx
  xq_PathCreateAction.cxx
)

set(MOC_H_FILES
  src/internal/xq_PathPlanningPlugin.h
  src/internal/xq_VesselPlanningView.h
  src/internal/xq_PathCreate.h
  src/internal/xq_CenterlineSmoother.h
  src/internal/xq_PathPreferencePage.h
  src/internal/xq_PathCreateAction.h
)

set(UI_FILES
  src/internal/xq_VesselPlanningView.ui
  src/internal/xq_PathCreate.ui
  src/internal/xq_CenterlineSmoother.ui
)

set(CACHED_RESOURCE_FILES
  plugin.xml
  resources/icon.png
  resources/pathplanning.svg
)

set(QRC_FILES
)
