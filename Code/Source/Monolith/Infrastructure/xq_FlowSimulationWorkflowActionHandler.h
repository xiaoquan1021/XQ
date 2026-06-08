#ifndef XQ_FLOWSIMULATIONWORKFLOWACTIONHANDLER_H
#define XQ_FLOWSIMULATIONWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicFlowSimulationWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_FLOWSIMULATIONWORKFLOWACTIONHANDLER_H
