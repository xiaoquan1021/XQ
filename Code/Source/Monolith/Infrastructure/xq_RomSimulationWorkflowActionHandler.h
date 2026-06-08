#ifndef XQ_ROMSIMULATIONWORKFLOWACTIONHANDLER_H
#define XQ_ROMSIMULATIONWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicRomSimulationWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_ROMSIMULATIONWORKFLOWACTIONHANDLER_H
