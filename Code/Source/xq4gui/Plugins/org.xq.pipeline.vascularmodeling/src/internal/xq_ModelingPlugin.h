#ifndef XQ_MODELINGPLUGIN_H
#define XQ_MODELINGPLUGIN_H

#include <ctkPluginActivator.h>

class xq_ModelingPlugin : public QObject, public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_gui_qt_modeling")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_MODELINGPLUGIN_H
