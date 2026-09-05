set(H_FILES
)

set(CPP_FILES
  xq_SegmentationPlugin.cxx
  xq_LumenContouringView.cxx
  xq_Seg3DCreateAction.cxx
  xq_LoftParamWidget.cxx
  xq_ProfileGroupCreate.cxx
  xq_SegmentationPreferencePage.cxx
  xq_ContourGroupCreateAction.cxx
)

set(MOC_H_FILES
  src/internal/xq_SegmentationPlugin.h
  src/internal/xq_LumenContouringView.h
  src/internal/xq_Seg3DCreateAction.h
  src/internal/xq_LoftParamWidget.h
  src/internal/xq_ProfileGroupCreate.h
  src/internal/xq_SegmentationPreferencePage.h
  src/internal/xq_ContourGroupCreateAction.h
)

set(UI_FILES
  src/internal/xq_LumenContouringView.ui
  src/internal/xq_LoftParamWidget.ui
  src/internal/xq_ProfileGroupCreate.ui
)

set(CACHED_RESOURCE_FILES
  plugin.xml
  resources/icon.png
  resources/segmentation2d.svg
)

set(QRC_FILES
)
