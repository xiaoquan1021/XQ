#include "xq_ProjectSessionService.h"

#include "xq_DataCatalogService.h"
#include "xq_ProjectService.h"
#include "xq_TaskRunner.h"

namespace xq::core
{

ProjectSessionService::ProjectSessionService(ProjectService& projectService,
                                             DataCatalogService& dataCatalog,
                                             TaskRunner& taskRunner,
                                             QObject* parent)
    : QObject(parent)
    , m_ProjectService(projectService)
    , m_DataCatalog(dataCatalog)
    , m_TaskRunner(taskRunner)
{
}

bool ProjectSessionService::Save(QString* errorMessage)
{
    QString taskMessage;
    const bool succeeded = m_TaskRunner.RunBlocking(
        QStringLiteral("Save Project"),
        [this](QString* message) {
            return m_ProjectService.SaveProject(m_DataCatalog, message);
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
            return m_ProjectService.OpenProject(projectFilePath,
                                                m_DataCatalog,
                                                message);
        },
        &taskMessage);

    SetError(errorMessage, taskMessage);
    return succeeded;
}

void ProjectSessionService::SetError(QString* errorMessage,
                                     const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
