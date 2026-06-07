#include "xq_ImagePreprocessingAlgorithmAdapter.h"

#include "Domain/xq_ImagePreprocessingWorkflowService.h"

#include <xq_ImageProcessingUtils.h>

namespace xq::infrastructure
{

namespace
{

ImagePreprocessingAlgorithmResult FailedResult(const QString& message)
{
    ImagePreprocessingAlgorithmResult result;
    result.Message = message;
    return result;
}

} // namespace

ImagePreprocessingAlgorithmResult
ImagePreprocessingAlgorithmAdapter::RunGaussianSmoothing(
    vtkImageData* input,
    const QVariantMap& parameters) const
{
    xq::domain::ImagePreprocessingWorkflowService domainService;
    const auto validation =
        domainService.ValidateOperationParameters(
            QStringLiteral("gaussian-smoothing"), parameters);
    if (!validation.Succeeded)
        return FailedResult(validation.Message);

    const auto imageResult =
        xq_ImageProcessingUtils::SmoothGaussian(
            input, parameters.value(QStringLiteral("sigma")).toDouble());

    ImagePreprocessingAlgorithmResult result;
    result.Succeeded = imageResult.ok;
    result.Image = imageResult.image;
    result.Message = QString::fromStdString(imageResult.diagnostic);
    return result;
}

} // namespace xq::infrastructure
