#ifndef XQ_INFRASTRUCTURE_PATHWORKFLOWACTIONHANDLER_H
#define XQ_INFRASTRUCTURE_PATHWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicPathWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh = nullptr,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_PATHWORKFLOWACTIONHANDLER_H
