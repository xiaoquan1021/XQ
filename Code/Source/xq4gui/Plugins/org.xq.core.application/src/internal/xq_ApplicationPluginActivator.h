#ifndef XQ_APPLICATION_PLUGIN_ACTIVATOR_H
#define XQ_APPLICATION_PLUGIN_ACTIVATOR_H

#include <berryAbstractUICTKPlugin.h>

class xq_ApplicationPluginActivator : public berry::AbstractUICTKPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org_xq_gui_qt_application")
    Q_INTERFACES(ctkPluginActivator)

public:
    xq_ApplicationPluginActivator();
    ~xq_ApplicationPluginActivator();

    static xq_ApplicationPluginActivator* GetDefault();
    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;
    static ctkPluginContext* getContext();

private:
    static ctkPluginContext* m_Context;
    static xq_ApplicationPluginActivator* m_Instance;
};

#endif // XQ_APPLICATION_PLUGIN_ACTIVATOR_H
