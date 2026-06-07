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
    QString Message;
};

struct ImagePreprocessingOperationDescriptor
{
    QString Id;
    QString Title;
};

class ImagePreprocessingWorkflowService
{
public:
    const QVector<ImagePreprocessingOperationDescriptor>& Operations() const;
    const ImagePreprocessingOperationDescriptor* FindOperation(
        const QString& operationId) const;

    ImagePreprocessingWorkflowResult Run(
        const xq::core::WorkflowContextSnapshot& snapshot) const;
};

} // namespace xq::domain

#endif // XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H
