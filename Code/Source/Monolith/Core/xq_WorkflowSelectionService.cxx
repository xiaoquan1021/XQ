#include "xq_WorkflowSelectionService.h"

#include "xq_WorkflowRegistry.h"

namespace xq::core
{

WorkflowSelectionService::WorkflowSelectionService(QObject* parent)
    : QObject(parent)
{
    const auto& workflows = DefaultWorkflowRegistry();
    if (!workflows.empty())
        m_SelectedWorkflowId = workflows.front().Id;
}

QString WorkflowSelectionService::SelectedWorkflowId() const
{
    return m_SelectedWorkflowId;
}

bool WorkflowSelectionService::SelectWorkflow(const QString& workflowId)
{
    if (!FindWorkflowById(workflowId))
        return false;

    if (m_SelectedWorkflowId == workflowId)
        return true;

    m_SelectedWorkflowId = workflowId;
    emit WorkflowChanged(m_SelectedWorkflowId);
    return true;
}

} // namespace xq::core
