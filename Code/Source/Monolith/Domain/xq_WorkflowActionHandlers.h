#ifndef XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H
#define XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H

namespace xq::core
{

class WorkflowActionService;

} // namespace xq::core

namespace xq::domain
{

int RegisterDefaultWorkflowActionHandlers(
    xq::core::WorkflowActionService& actions);

} // namespace xq::domain

#endif // XQ_DOMAIN_WORKFLOWACTIONHANDLERS_H
