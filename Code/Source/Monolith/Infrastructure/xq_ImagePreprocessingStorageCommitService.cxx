#include "xq_ImagePreprocessingStorageCommitService.h"

#include "xq_ImagePreprocessingMitkImageAdapter.h"
#include "xq_ImagePreprocessingResultNodeFactory.h"

namespace xq::infrastructure
{

namespace
{

ImagePreprocessingStorageCommitResult FailedResult(const QString& message)
{
    ImagePreprocessingStorageCommitResult result;
    result.Message = message;
    return result;
}

ImagePreprocessingStorageCommitResult FailedExecutionResult(
    const ImagePreprocessingExecutionResult& execution)
{
    ImagePreprocessingStorageCommitResult result;
    result.Execution = execution;
    result.Message = execution.Message;
    return result;
}

} // namespace

ImagePreprocessingStorageCommitResult
ImagePreprocessingStorageCommitService::Run(
    const ImagePreprocessingStorageCommitRequest& request) const
{
    if (request.DataStorage.IsNull())
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing data storage is required."));
    }

    ImagePreprocessingMitkImageAdapter imageAdapter;
    const auto inputImageResult =
        imageAdapter.ExtractInputImage(request.SourceNode);
    if (!inputImageResult.Succeeded)
        return FailedResult(inputImageResult.Message);

    ImagePreprocessingExecutionRequest executionRequest;
    executionRequest.Context = request.Context;
    executionRequest.OperationId = request.OperationId;
    executionRequest.Parameters = request.Parameters;
    executionRequest.InputImage = inputImageResult.Image;

    ImagePreprocessingExecutionService executionService;
    const auto executionResult = executionService.Run(executionRequest);
    if (!executionResult.Succeeded)
        return FailedExecutionResult(executionResult);

    ImagePreprocessingResultNodeFactory nodeFactory;
    const auto nodeResult =
        nodeFactory.CreateImageResultNode(request.SourceNode,
                                          executionResult,
                                          request.ResultSuffix);
    if (!nodeResult.Succeeded)
        return FailedResult(nodeResult.Message);

    request.DataStorage->Add(nodeResult.Node, request.SourceNode);

    ImagePreprocessingStorageCommitResult result;
    result.Succeeded = true;
    result.Message = executionResult.Message;
    result.Execution = executionResult;
    result.ResultNode = nodeResult.Node;
    return result;
}

} // namespace xq::infrastructure
