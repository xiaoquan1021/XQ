#ifndef XQ_MODELINGWORKFLOWACTIONHANDLER_H
#define XQ_MODELINGWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicModelingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_MODELINGWORKFLOWACTIONHANDLER_H
