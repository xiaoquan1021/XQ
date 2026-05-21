#ifndef XQ_MULTIPHYSICSPLUGIN_H
#define XQ_MULTIPHYSICSPLUGIN_H

#include <ctkPluginActivator.h>
#include <XQ_QT_MULTIPHYSICSExports.h>

class XQ_QT_MULTIPHYSICS_EXPORT xq_MultiPhysicsPlugin
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_imaging_multiphysics")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_MultiPhysicsPlugin();
  ~xq_MultiPhysicsPlugin() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif
