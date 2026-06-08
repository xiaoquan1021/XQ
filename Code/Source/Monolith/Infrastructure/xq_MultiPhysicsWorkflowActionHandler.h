#ifndef XQ_MULTIPHYSICSWORKFLOWACTIONHANDLER_H
#define XQ_MULTIPHYSICSWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicMultiPhysicsWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_MULTIPHYSICSWORKFLOWACTIONHANDLER_H
