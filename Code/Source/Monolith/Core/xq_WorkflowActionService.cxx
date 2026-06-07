#include "xq_WorkflowActionService.h"

#include "xq_TaskRunner.h"
#include "xq_WorkflowContextService.h"

namespace xq::core
{

WorkflowActionService::WorkflowActionService(
    WorkflowContextService& workflowContext,
    TaskRunner& taskRunner,
    QObject* parent)
    : QObject(parent)
    , m_WorkflowContext(workflowContext)
    , m_TaskRunner(taskRunner)
{
}

bool WorkflowActionService::RequestActiveWorkflowAction(
    QString* message) const
{
    const WorkflowContextSnapshot snapshot = m_WorkflowContext.Snapshot();
    if (!snapshot.HasCompatibleSelection)
    {
        SetMessage(message,
                   QStringLiteral("Select compatible data before running %1.")
                       .arg(snapshot.WorkflowTitle));
        return false;
    }

    if (!snapshot.RequiresSelectedData)
    {
        SetMessage(message,
                   QStringLiteral("%1 action requested.")
                       .arg(snapshot.WorkflowTitle));
        return true;
    }

    const QString displayName =
        snapshot.SelectedDataDisplayName.trimmed().isEmpty()
            ? snapshot.SelectedCatalogEntryId
            : snapshot.SelectedDataDisplayName;
    SetMessage(message,
               QStringLiteral("%1 action requested for %2.")
                   .arg(snapshot.WorkflowTitle, displayName));
    return true;
}

bool WorkflowActionService::RunActiveWorkflowAction(QString* message)
{
    const WorkflowContextSnapshot snapshot = m_WorkflowContext.Snapshot();
    QString requestMessage;
    if (!RequestActiveWorkflowAction(&requestMessage))
    {
        SetMessage(message, requestMessage);
        return false;
    }

    const QString taskName =
        QStringLiteral("Run %1").arg(snapshot.WorkflowTitle);
    return m_TaskRunner.RunBlocking(
        taskName,
        [requestMessage](QString* taskMessage) {
            SetMessage(taskMessage, requestMessage);
            return true;
        },
        message);
}

void WorkflowActionService::SetMessage(QString* message,
                                       const QString& value)
{
    if (message)
        *message = value;
}

} // namespace xq::core
