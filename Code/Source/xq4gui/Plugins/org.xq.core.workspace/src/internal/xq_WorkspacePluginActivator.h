#ifndef XQ_WORKSPACEPLUGINACTIVATOR_H
#define XQ_WORKSPACEPLUGINACTIVATOR_H

#include <XQ_QT_PROJECTMANAGERExports.h>

#include <ctkPluginActivator.h>

class XQ_QT_PROJECTMANAGER_EXPORT xq_WorkspacePluginActivator
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_gui_qt_projectmanager")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_WorkspacePluginActivator();
  ~xq_WorkspacePluginActivator() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif // XQ_WORKSPACEPLUGINACTIVATOR_H
