#ifndef XQ_SEGMENTATIONWORKFLOWACTIONHANDLER_H
#define XQ_SEGMENTATIONWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

bool RegisterDynamicSegmentationWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_SEGMENTATIONWORKFLOWACTIONHANDLER_H
