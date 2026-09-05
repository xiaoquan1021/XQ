#ifndef XQ_ROMSIMULATIONPLUGIN_H
#define XQ_ROMSIMULATIONPLUGIN_H

#include <ctkPluginActivator.h>
#include <XQ_QT_ROMSIMULATIONExports.h>

class XQ_QT_ROMSIMULATION_EXPORT xq_ROMSimulationPlugin
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_imaging_reducedflow")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_ROMSimulationPlugin();
  ~xq_ROMSimulationPlugin() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif
