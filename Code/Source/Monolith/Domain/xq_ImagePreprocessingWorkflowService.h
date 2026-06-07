#ifndef XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H
#define XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H

#include "Core/xq_WorkflowContextService.h"

#include <QString>
#include <QVector>

namespace xq::domain
{

struct ImagePreprocessingWorkflowResult
{
    bool Succeeded = false;
    QString SourceCatalogEntryId;
    QString SelectedDataDisplayName;
    QString OperationId;
    QString OperationTitle;
    QString Message;
};

enum class ImagePreprocessingParameterValueType
{
    NumericScalar,
    IntegerScalar,
    IntegerPointList
};

struct ImagePreprocessingParameterDescriptor
{
    QString Id;
    QString Title;
    ImagePreprocessingParameterValueType Type =
        ImagePreprocessingParameterValueType::NumericScalar;
    bool Required = true;
};

struct ImagePreprocessingOperationDescriptor
{
    QString Id;
    QString Title;
    QVector<ImagePreprocessingParameterDescriptor> Parameters;
};

class ImagePreprocessingWorkflowService
{
public:
    const QVector<ImagePreprocessingOperationDescriptor>& Operations() const;
    const ImagePreprocessingOperationDescriptor* FindOperation(
        const QString& operationId) const;

    ImagePreprocessingWorkflowResult RunOperation(
        const xq::core::WorkflowContextSnapshot& snapshot,
        const QString& operationId) const;

    ImagePreprocessingWorkflowResult Run(
        const xq::core::WorkflowContextSnapshot& snapshot) const;
};

} // namespace xq::domain

#endif // XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H
