#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H

#include <QString>
#include <QVariantMap>

namespace xq::core
{
class ApplicationContext;
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
    QString* message = nullptr);

bool RegisterDynamicImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGWORKFLOWACTIONHANDLER_H
