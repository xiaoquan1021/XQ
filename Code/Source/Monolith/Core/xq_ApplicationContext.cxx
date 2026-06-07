#include "xq_ApplicationContext.h"

#include "xq_DataCatalogService.h"
#include "xq_DataHierarchyService.h"
#include "xq_DataImportService.h"
#include "xq_DataManagementService.h"
#include "xq_DataSelectionService.h"
#include "xq_PreferencesService.h"
#include "xq_ProjectService.h"
#include "xq_ProjectSessionService.h"
#include "xq_TaskRunner.h"
#include "xq_WorkflowActionService.h"
#include "xq_WorkflowContextService.h"
#include "xq_WorkflowSelectionService.h"

#include <mitkStandaloneDataStorage.h>

namespace xq::core
{

ApplicationContext::ApplicationContext(mitk::DataStorage::Pointer dataStorage,
                                       QObject* parent)
    : QObject(parent)
    , m_DataStorage(dataStorage)
    , m_DataCatalogService(new DataCatalogService(this))
    , m_DataHierarchyService(new DataHierarchyService(this))
    , m_DataSelectionService(new DataSelectionService(*m_DataCatalogService,
                                                      *m_DataHierarchyService,
                                                      this))
    , m_PreferencesService(new PreferencesService(this))
    , m_ProjectService(new ProjectService(this))
    , m_TaskRunner(new TaskRunner(this))
    , m_WorkflowSelectionService(new WorkflowSelectionService(this))
    , m_DataManagementService(new DataManagementService(*m_DataCatalogService,
                                                        *m_DataHierarchyService,
                                                        *m_DataSelectionService,
                                                        *m_TaskRunner,
                                                        this))
    , m_DataImportService(new DataImportService(*m_DataCatalogService,
                                                *m_DataHierarchyService,
                                                *m_DataSelectionService,
                                                *m_TaskRunner,
                                                this))
    , m_ProjectSessionService(new ProjectSessionService(*m_ProjectService,
                                                        *m_DataCatalogService,
                                                        *m_DataHierarchyService,
                                                        *m_DataSelectionService,
                                                        *m_TaskRunner,
                                                        this))
    , m_WorkflowContextService(new WorkflowContextService(
          *m_WorkflowSelectionService,
          *m_DataSelectionService,
          *m_DataCatalogService,
          this))
    , m_WorkflowActionService(new WorkflowActionService(
          *m_WorkflowContextService,
          *m_TaskRunner,
          this))
{
    connect(m_TaskRunner,
            &TaskRunner::TaskFinished,
            this,
            [this](const QString& taskName,
                   bool succeeded,
                   const QString& message) {
                const QString state =
                    succeeded ? QStringLiteral("succeeded")
                              : QStringLiteral("failed");
                const QString trimmedMessage = message.trimmed();
                if (trimmedMessage.isEmpty())
                    PostDiagnostic(QStringLiteral("%1 %2.")
                                       .arg(taskName, state));
                else
                    PostDiagnostic(QStringLiteral("%1 %2: %3")
                                       .arg(taskName, state, trimmedMessage));
            });
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

DataHierarchyService* ApplicationContext::DataHierarchy() const
{
    return m_DataHierarchyService;
}

DataImportService* ApplicationContext::DataImports() const
{
    return m_DataImportService;
}

DataManagementService* ApplicationContext::DataManagement() const
{
    return m_DataManagementService;
}

DataSelectionService* ApplicationContext::DataSelection() const
{
    return m_DataSelectionService;
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

WorkflowActionService* ApplicationContext::WorkflowActions() const
{
    return m_WorkflowActionService;
}

WorkflowContextService* ApplicationContext::WorkflowContext() const
{
    return m_WorkflowContextService;
}

WorkflowSelectionService* ApplicationContext::WorkflowSelection() const
{
    return m_WorkflowSelectionService;
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
