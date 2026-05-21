#ifndef XQ_DATAMANAGERPLUGINACTIVATOR_H
#define XQ_DATAMANAGERPLUGINACTIVATOR_H

#include <ctkPluginActivator.h>

class xq_DataExplorerPluginActivator : public QObject, public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_core_datamanager")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_DATAMANAGERPLUGINACTIVATOR_H
