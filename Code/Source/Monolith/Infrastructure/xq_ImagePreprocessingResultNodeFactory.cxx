#include "xq_ImagePreprocessingResultNodeFactory.h"

#include "xq_ImagePreprocessingMitkImageAdapter.h"

#include <xq_PipelineDataUtils.h>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kImageProcessingToolId = "org.xq.views.imageprocessing";

ImagePreprocessingResultNodeCreationResult FailedResult(
    const QString& message)
{
    ImagePreprocessingResultNodeCreationResult result;
    result.Message = message;
    return result;
}

QString SourceNodeName(const mitk::DataNode* sourceNode)
{
    if (!sourceNode)
        return QStringLiteral("Image");

    const QString name = QString::fromStdString(sourceNode->GetName()).trimmed();
    return name.isEmpty() ? QStringLiteral("Image") : name;
}

QString ResultNodeName(const mitk::DataNode* sourceNode,
                       const QString& suffix)
{
    const QString normalizedSuffix = suffix.trimmed();
    if (normalizedSuffix.isEmpty())
        return SourceNodeName(sourceNode);

    return QStringLiteral("%1_%2").arg(SourceNodeName(sourceNode),
                                       normalizedSuffix);
}

} // namespace

ImagePreprocessingResultNodeCreationResult
ImagePreprocessingResultNodeFactory::CreateImageResultNode(
    mitk::DataNode* sourceNode,
    const ImagePreprocessingExecutionResult& executionResult,
    const QString& suffix) const
{
    if (!executionResult.Image)
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing result image is required."));
    }

    ImagePreprocessingMitkImageAdapter imageAdapter;
    const auto imageResult =
        imageAdapter.CreateMitkImage(executionResult.Image);
    if (!imageResult.Succeeded)
        return FailedResult(imageResult.Message);

    auto node = mitk::DataNode::New();
    node->SetName(ResultNodeName(sourceNode, suffix).toStdString());
    node->SetData(imageResult.Image);

    const std::string operation =
        executionResult.OperationId.trimmed().toStdString();
    xq::pipeline::MarkGeneratedNode(
        node,
        xq::pipeline::Stage::ImageProcessing,
        operation,
        kImageProcessingToolId);

    node->SetStringProperty("xq.type", "image_processing_result");
    node->SetStringProperty("xq.image.processing.operation",
                            operation.c_str());
    node->SetBoolProperty("xq.image.processing.output_is_image", true);

    if (sourceNode)
    {
        const std::string sourceName = SourceNodeName(sourceNode).toStdString();
        node->SetStringProperty("xq.image.processing.source_node",
                                sourceName.c_str());
        node->SetStringProperty(xq::pipeline::kSourceImageProperty,
                                sourceName.c_str());
    }

    ImagePreprocessingResultNodeCreationResult result;
    result.Succeeded = true;
    result.Node = node;
    result.Message = executionResult.Message;
    return result;
}

} // namespace xq::infrastructure
