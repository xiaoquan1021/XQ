#ifndef XQ_GRIDINGPLUGIN_H
#define XQ_GRIDINGPLUGIN_H

#include <ctkPluginActivator.h>

class xq_GridingPlugin : public QObject, public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_gui_qt_meshing")
  Q_INTERFACES(ctkPluginActivator)

public:
  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_GRIDINGPLUGIN_H
