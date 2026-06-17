#ifndef XQ_PROJECTDATANODESPLUGINACTIVATOR_H
#define XQ_PROJECTDATANODESPLUGINACTIVATOR_H

#include <ctkPluginActivator.h>

#include <QString>

class xq_ProjectDataNodesPluginActivator
    : public QObject
    , public ctkPluginActivator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org_xq_projectdatanodes")
    Q_INTERFACES(ctkPluginActivator)

public:
    xq_ProjectDataNodesPluginActivator();
    ~xq_ProjectDataNodesPluginActivator() override;

    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;

private:
    void LoadModules();
    void LoadQtLibrary(QString name, QString libFileName);
};

#endif // XQ_PROJECTDATANODESPLUGINACTIVATOR_H
