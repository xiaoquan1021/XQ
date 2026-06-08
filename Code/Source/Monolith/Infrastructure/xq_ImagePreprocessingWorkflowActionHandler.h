#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H

#include <QString>
#include <QVariantMap>

namespace xq::core
{
class ApplicationContext;
class RenderRefreshService;
}

namespace xq::infrastructure
{

struct ImagePreprocessingWorkflowActionOptions
{
    QString OperationId;
    QVariantMap Parameters;
    QString ResultSuffix;
};

bool RegisterImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    const ImagePreprocessingWorkflowActionOptions& options,
    QString* message);

bool RegisterImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    const ImagePreprocessingWorkflowActionOptions& options,
    xq::core::RenderRefreshService* renderRefresh = nullptr,
    QString* message = nullptr);

bool RegisterDynamicImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    QString* message);

bool RegisterDynamicImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh = nullptr,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H
