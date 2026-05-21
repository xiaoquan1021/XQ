#ifndef XQ_MITKSEGMENTATIONPLUGIN_H
#define XQ_MITKSEGMENTATIONPLUGIN_H

#include <ctkPluginActivator.h>

class xq_MitkSegmentationPlugin
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_imaging_volumesegmentation")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_MITKSEGMENTATIONPLUGIN_H
