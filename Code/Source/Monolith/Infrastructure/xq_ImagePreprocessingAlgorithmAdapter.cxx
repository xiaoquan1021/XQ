#include "xq_ImagePreprocessingAlgorithmAdapter.h"

#include "Domain/xq_ImagePreprocessingWorkflowService.h"

#include <QVariantList>

#include <xq_ImageProcessingUtils.h>

#include <array>
#include <vector>

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

std::vector<std::array<int, 3>> SeedsFromParameters(
    const QVariantMap& parameters)
{
    std::vector<std::array<int, 3>> seeds;
    const QVariantList seedValues =
        parameters.value(QStringLiteral("seeds")).toList();
    seeds.reserve(static_cast<std::size_t>(seedValues.size()));

    for (const auto& seedValue : seedValues)
    {
        const QVariantList point = seedValue.toList();
        seeds.push_back({point.at(0).toInt(),
                         point.at(1).toInt(),
                         point.at(2).toInt()});
    }

    return seeds;
}

} // namespace

ImagePreprocessingAlgorithmResult
ImagePreprocessingAlgorithmAdapter::RunBinaryThreshold(
    vtkImageData* input,
    const QVariantMap& parameters) const
{
    xq::domain::ImagePreprocessingWorkflowService domainService;
    const auto validation =
        domainService.ValidateOperationParameters(
            QStringLiteral("binary-threshold"), parameters);
    if (!validation.Succeeded)
        return FailedResult(validation.Message);

    const auto imageResult =
        xq_ImageProcessingUtils::BinaryThreshold(
            input,
            parameters.value(QStringLiteral("lower")).toDouble(),
            parameters.value(QStringLiteral("upper")).toDouble(),
            parameters.value(QStringLiteral("inside-value")).toDouble(),
            parameters.value(QStringLiteral("outside-value")).toDouble());

    ImagePreprocessingAlgorithmResult result;
    result.Succeeded = imageResult.ok;
    result.Image = imageResult.image;
    result.Message = QString::fromStdString(imageResult.diagnostic);
    return result;
}

ImagePreprocessingAlgorithmResult
ImagePreprocessingAlgorithmAdapter::RunConnectedThreshold(
    vtkImageData* input,
    const QVariantMap& parameters) const
{
    xq::domain::ImagePreprocessingWorkflowService domainService;
    const auto validation =
        domainService.ValidateOperationParameters(
            QStringLiteral("connected-threshold"), parameters);
    if (!validation.Succeeded)
        return FailedResult(validation.Message);

    const auto imageResult =
        xq_ImageProcessingUtils::ConnectedThreshold(
            input,
            parameters.value(QStringLiteral("lower")).toDouble(),
            parameters.value(QStringLiteral("upper")).toDouble(),
            SeedsFromParameters(parameters));

    ImagePreprocessingAlgorithmResult result;
    result.Succeeded = imageResult.ok;
    result.Image = imageResult.image;
    result.Message = QString::fromStdString(imageResult.diagnostic);
    return result;
}

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

ImagePreprocessingAlgorithmResult
ImagePreprocessingAlgorithmAdapter::RunMorphologyOpenClose(
    vtkImageData* input,
    const QVariantMap& parameters) const
{
    xq::domain::ImagePreprocessingWorkflowService domainService;
    const auto validation =
        domainService.ValidateOperationParameters(
            QStringLiteral("morphology-open-close"), parameters);
    if (!validation.Succeeded)
        return FailedResult(validation.Message);

    const auto imageResult =
        xq_ImageProcessingUtils::MorphologicalOpenClose(
            input, parameters.value(QStringLiteral("radius")).toInt());

    ImagePreprocessingAlgorithmResult result;
    result.Succeeded = imageResult.ok;
    result.Image = imageResult.image;
    result.Message = QString::fromStdString(imageResult.diagnostic);
    return result;
}

} // namespace xq::infrastructure
