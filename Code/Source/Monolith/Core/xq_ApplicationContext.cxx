#include "xq_ApplicationContext.h"

#include "xq_DataCatalogService.h"
#include "xq_DataImportService.h"
#include "xq_PreferencesService.h"
#include "xq_ProjectService.h"
#include "xq_ProjectSessionService.h"
#include "xq_TaskRunner.h"

#include <mitkStandaloneDataStorage.h>

namespace xq::core
{

ApplicationContext::ApplicationContext(mitk::DataStorage::Pointer dataStorage,
                                       QObject* parent)
    : QObject(parent)
    , m_DataStorage(dataStorage)
    , m_DataCatalogService(new DataCatalogService(this))
    , m_PreferencesService(new PreferencesService(this))
    , m_ProjectService(new ProjectService(this))
    , m_TaskRunner(new TaskRunner(this))
    , m_DataImportService(new DataImportService(*m_DataCatalogService,
                                                *m_TaskRunner,
                                                this))
    , m_ProjectSessionService(new ProjectSessionService(*m_ProjectService,
                                                        *m_DataCatalogService,
                                                        *m_TaskRunner,
                                                        this))
{
}

ApplicationContext* ApplicationContext::CreateDefault(QObject* parent)
{
    return new ApplicationContext(mitk::StandaloneDataStorage::New(), parent);
}

mitk::DataStorage::Pointer ApplicationContext::DataStorage() const
{
    return m_DataStorage;
}

mitk::DataNode::Pointer ApplicationContext::ActiveNode() const
{
    return m_ActiveNode;
}

DataCatalogService* ApplicationContext::DataCatalog() const
{
    return m_DataCatalogService;
}

DataImportService* ApplicationContext::DataImports() const
{
    return m_DataImportService;
}

QStringList ApplicationContext::Diagnostics() const
{
    return m_Diagnostics;
}

PreferencesService* ApplicationContext::Preferences() const
{
    return m_PreferencesService;
}

ProjectService* ApplicationContext::Projects() const
{
    return m_ProjectService;
}

ProjectSessionService* ApplicationContext::ProjectSession() const
{
    return m_ProjectSessionService;
}

TaskRunner* ApplicationContext::Tasks() const
{
    return m_TaskRunner;
}

void ApplicationContext::SetActiveNode(mitk::DataNode::Pointer node)
{
    if (m_ActiveNode.GetPointer() == node.GetPointer())
        return;

    m_ActiveNode = node;
    emit ActiveNodeChanged();
    emit SelectionChanged(m_ActiveNode);
}

void ApplicationContext::ClearActiveNode()
{
    SetActiveNode(nullptr);
}

void ApplicationContext::PostDiagnostic(const QString& message)
{
    if (message.trimmed().isEmpty())
        return;

    m_Diagnostics.append(message);
    emit DiagnosticPosted(message);
}

} // namespace xq::core
