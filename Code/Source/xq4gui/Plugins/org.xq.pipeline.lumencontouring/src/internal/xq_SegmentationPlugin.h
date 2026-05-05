#ifndef XQ_SEGMENTATIONPLUGIN_H
#define XQ_SEGMENTATIONPLUGIN_H

#include <ctkPluginActivator.h>

class xq_SegmentationPlugin : public QObject, public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_gui_qt_segmentation")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_SEGMENTATIONPLUGIN_H
