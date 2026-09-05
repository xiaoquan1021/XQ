#ifndef XQ_SIMULATIONPLUGIN_H
#define XQ_SIMULATIONPLUGIN_H

#include <ctkPluginActivator.h>

class xq_SimulationPlugin : public QObject, public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_imaging_flowanalysis")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_SIMULATIONPLUGIN_H
