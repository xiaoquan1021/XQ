#ifndef XQ_PYTHONDATANODESPLUGINACTIVATOR_H
#define XQ_PYTHONDATANODESPLUGINACTIVATOR_H

#include <berryAbstractUICTKPlugin.h>
#include <ctkPluginActivator.h>
#include <QString>

class xq_PythonDataNodesPluginActivator : public berry::AbstractUICTKPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_pythondatanodes")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_PythonDataNodesPluginActivator();
  ~xq_PythonDataNodesPluginActivator() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;

  static xq_PythonDataNodesPluginActivator* GetDefault();
  static ctkPluginContext* GetContext();

private:
  static ctkPluginContext* m_Context;
  static xq_PythonDataNodesPluginActivator* m_Inst;
};

#endif // XQ_PYTHONDATANODESPLUGINACTIVATOR_H
