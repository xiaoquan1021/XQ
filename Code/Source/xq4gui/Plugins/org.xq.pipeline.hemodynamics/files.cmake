set(H_FILES
  src/internal/xq_HemoContextMenuActionBase.h
)

set(CPP_FILES
  xq_SimulationPlugin.cxx
  xq_HemodynamicsView.cxx
  xq_SimJobCreate.cxx
  xq_CapBCWidget.cxx
  xq_SolverProcessHandler.cxx
  xq_SimulationPreferencePage.cxx
  xq_SimJobCreateAction.cxx
  xq_SolverExportAction.cxx
  xq_ResultImportAction.cxx
)

set(MOC_H_FILES
  src/internal/xq_SimulationPlugin.h
  src/internal/xq_HemodynamicsView.h
  src/internal/xq_SimJobCreate.h
  src/internal/xq_CapBCWidget.h
  src/internal/xq_SolverProcessHandler.h
  src/internal/xq_SimulationPreferencePage.h
  src/internal/xq_SimJobCreateAction.h
  src/internal/xq_SolverExportAction.h
  src/internal/xq_ResultImportAction.h
)

set(UI_FILES
  src/internal/xq_HemodynamicsView.ui
  src/internal/xq_SimJobCreate.ui
  src/internal/xq_SimulationPreferencePage.ui
)

set(CACHED_RESOURCE_FILES
  plugin.xml
  resources/icon.png
)

set(QRC_FILES
)
