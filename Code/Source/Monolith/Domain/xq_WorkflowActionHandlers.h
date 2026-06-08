#ifndef XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H
#define XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H

namespace xq::core
{

class WorkflowActionService;
class WorkflowOperationService;

} // namespace xq::core

namespace xq::domain
{

int RegisterDefaultWorkflowActionHandlers(
    xq::core::WorkflowActionService& actions,
    xq::core::WorkflowOperationService* operations = nullptr);

} // namespace xq::domain

#endif // XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H
