#include "xq_ImagePreprocessingExecutionService.h"

#include "xq_ImagePreprocessingAlgorithmAdapter.h"

#include "Domain/xq_ImagePreprocessingWorkflowService.h"

namespace xq::infrastructure
{

namespace
{

ImagePreprocessingExecutionResult FailedResult(const QString& message)
{
    ImagePreprocessingExecutionResult result;
    result.Message = message;
    return result;
}

void CopyWorkflowMetadata(
    const xq::domain::ImagePreprocessingWorkflowResult& workflowResult,
    ImagePreprocessingExecutionResult& executionResult)
{
    executionResult.SourceCatalogEntryId =
        workflowResult.SourceCatalogEntryId;
    executionResult.SelectedDataDisplayName =
        workflowResult.SelectedDataDisplayName;
    executionResult.OperationId = workflowResult.OperationId;
    executionResult.OperationTitle = workflowResult.OperationTitle;
}

QString CompletionMessage(
    const xq::domain::ImagePreprocessingWorkflowResult& workflowResult)
{
    return QStringLiteral("%1 preprocessing operation completed %2.")
        .arg(workflowResult.OperationTitle,
             workflowResult.SelectedDataDisplayName);
}

} // namespace

ImagePreprocessingExecutionResult ImagePreprocessingExecutionService::Run(
    const ImagePreprocessingExecutionRequest& request) const
{
    xq::domain::ImagePreprocessingWorkflowService workflowService;
    const auto workflowResult =
        workflowService.RunOperation(request.Context,
                                     request.OperationId,
                                     request.Parameters);
    if (!workflowResult.Succeeded)
        return FailedResult(workflowResult.Message);

    ImagePreprocessingAlgorithmAdapter adapter;
    const auto algorithmResult =
        adapter.RunOperation(workflowResult.OperationId,
                             request.InputImage,
                             request.Parameters);

    ImagePreprocessingExecutionResult result;
    CopyWorkflowMetadata(workflowResult, result);
    result.Succeeded = algorithmResult.Succeeded;
    result.Image = algorithmResult.Image;
    result.Message = algorithmResult.Succeeded
        ? CompletionMessage(workflowResult)
        : algorithmResult.Message;
    return result;
}

} // namespace xq::infrastructure
