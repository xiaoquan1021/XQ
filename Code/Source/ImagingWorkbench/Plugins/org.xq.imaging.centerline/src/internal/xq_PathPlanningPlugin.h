#ifndef XQ_PATHPLANNINGPLUGINACTIVATOR_H
#define XQ_PATHPLANNINGPLUGINACTIVATOR_H

#include <ctkPluginActivator.h>
#include <XQ_QT_PATHPLANNINGExports.h>

class XQ_QT_PATHPLANNING_EXPORT xq_PathPlanningPlugin
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_imaging_centerline")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_PathPlanningPlugin();
  ~xq_PathPlanningPlugin() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_PATHPLANNINGPLUGINACTIVATOR_H
