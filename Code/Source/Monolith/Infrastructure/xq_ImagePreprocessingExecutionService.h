#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGEXECUTIONSERVICE_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGEXECUTIONSERVICE_H

#include "Core/xq_WorkflowContextService.h"

#include <QString>
#include <QVariantMap>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace xq::infrastructure
{

struct ImagePreprocessingExecutionRequest
{
    xq::core::WorkflowContextSnapshot Context;
    QString OperationId;
    QVariantMap Parameters;
    vtkImageData* InputImage = nullptr;
};

struct ImagePreprocessingExecutionResult
{
    bool Succeeded = false;
    QString SourceCatalogEntryId;
    QString SelectedDataDisplayName;
    QString OperationId;
    QString OperationTitle;
    QString Message;
    vtkSmartPointer<vtkImageData> Image;
};

class ImagePreprocessingExecutionService
{
public:
    ImagePreprocessingExecutionResult Run(
        const ImagePreprocessingExecutionRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGEXECUTIONSERVICE_H
