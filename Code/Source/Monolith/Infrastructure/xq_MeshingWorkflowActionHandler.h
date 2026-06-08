#ifndef XQ_MESHINGWORKFLOWACTIONHANDLER_H
#define XQ_MESHINGWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicMeshingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_MESHINGWORKFLOWACTIONHANDLER_H
