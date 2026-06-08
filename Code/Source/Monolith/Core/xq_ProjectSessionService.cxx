#include "xq_ProjectSessionService.h"

#include "xq_DataCatalogService.h"
#include "xq_DataHierarchyService.h"
#include "xq_DataSelectionService.h"
#include "xq_ProjectService.h"
#include "xq_TaskRunner.h"
#include "xq_WorkflowOperationService.h"

namespace xq::core
{

ProjectSessionService::ProjectSessionService(ProjectService& projectService,
                                             DataCatalogService& dataCatalog,
                                             DataHierarchyService& dataHierarchy,
                                             TaskRunner& taskRunner,
                                             QObject* parent)
    : QObject(parent)
    , m_ProjectService(projectService)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(dataHierarchy)
    , m_TaskRunner(taskRunner)
{
}

ProjectSessionService::ProjectSessionService(ProjectService& projectService,
                                             DataCatalogService& dataCatalog,
                                             DataHierarchyService& dataHierarchy,
                                             DataSelectionService& dataSelection,
                                             TaskRunner& taskRunner,
                                             QObject* parent)
    : QObject(parent)
    , m_ProjectService(projectService)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(dataHierarchy)
    , m_DataSelection(&dataSelection)
    , m_TaskRunner(taskRunner)
{
}

ProjectSessionService::ProjectSessionService(
    ProjectService& projectService,
    DataCatalogService& dataCatalog,
    DataHierarchyService& dataHierarchy,
    DataSelectionService& dataSelection,
    WorkflowOperationService& workflowOperations,
    TaskRunner& taskRunner,
    QObject* parent)
    : QObject(parent)
    , m_ProjectService(projectService)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(dataHierarchy)
    , m_DataSelection(&dataSelection)
    , m_WorkflowOperations(&workflowOperations)
    , m_TaskRunner(taskRunner)
{
}

bool ProjectSessionService::Save(QString* errorMessage)
{
    QString taskMessage;
    const bool succeeded = m_TaskRunner.RunBlocking(
        QStringLiteral("Save Project"),
        [this](QString* message) {
            if (m_WorkflowOperations)
            {
                return m_ProjectService.SaveProject(m_DataCatalog,
                                                    m_DataHierarchy,
                                                    *m_WorkflowOperations,
                                                    message);
            }

            return m_ProjectService.SaveProject(m_DataCatalog,
                                                m_DataHierarchy,
                                                message);
        },
        &taskMessage);

    SetError(errorMessage, taskMessage);
    return succeeded;
}

bool ProjectSessionService::Open(const QString& projectFilePath,
                                 QString* errorMessage)
{
    QString taskMessage;
    const bool succeeded = m_TaskRunner.RunBlocking(
        QStringLiteral("Open Project"),
        [this, &projectFilePath](QString* message) {
            if (m_WorkflowOperations)
            {
                return m_ProjectService.OpenProject(projectFilePath,
                                                    m_DataCatalog,
                                                    m_DataHierarchy,
                                                    *m_WorkflowOperations,
                                                    message);
            }

            return m_ProjectService.OpenProject(projectFilePath,
                                                m_DataCatalog,
                                                m_DataHierarchy,
                                                message);
        },
        &taskMessage);

    SetError(errorMessage, taskMessage);
    if (succeeded && m_DataSelection)
        m_DataSelection->Clear();
    return succeeded;
}

void ProjectSessionService::SetError(QString* errorMessage,
                                     const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
