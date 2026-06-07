#include "Infrastructure/xq_ImagePreprocessingResultNodeFactory.h"

#include <xq_PipelineDataUtils.h>

#include <QCoreApplication>

#include <mitkDataNode.h>
#include <mitkImage.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

vtkSmartPointer<vtkImageData> MakeVtkImage()
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(3, 2, 4);
    image->AllocateScalars(VTK_FLOAT, 1);
    return image;
}

mitk::Image::Pointer MakeMitkImage(vtkImageData* source)
{
    auto image = mitk::Image::New();
    image->Initialize(source);
    auto* output = image->GetVtkImageData();
    if (output)
        output->DeepCopy(source);
    return image;
}

mitk::DataNode::Pointer MakeSourceNode()
{
    auto sourceImage = MakeVtkImage();
    auto node = mitk::DataNode::New();
    node->SetName("CTA");
    node->SetData(MakeMitkImage(sourceImage));
    return node;
}

xq::infrastructure::ImagePreprocessingExecutionResult MakeExecutionResult()
{
    xq::infrastructure::ImagePreprocessingExecutionResult result;
    result.Succeeded = true;
    result.SourceCatalogEntryId = QStringLiteral("image-001");
    result.SelectedDataDisplayName = QStringLiteral("CTA Image");
    result.OperationId = QStringLiteral("crop");
    result.OperationTitle = QStringLiteral("Crop");
    result.Message =
        QStringLiteral("Crop preprocessing operation completed CTA Image.");
    result.Image = MakeVtkImage();
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingResultNodeFactory factory;

    auto missingImageExecution = MakeExecutionResult();
    missingImageExecution.Image = nullptr;

    const auto missingImageResult =
        factory.CreateImageResultNode(MakeSourceNode().GetPointer(),
                                      missingImageExecution,
                                      QStringLiteral("cropped"));
    if (Expect(!missingImageResult.Succeeded,
               "factory should reject missing output images"))
        return 1;
    if (Expect(missingImageResult.Message ==
                   QStringLiteral("Image preprocessing result image is required."),
               "missing output image should use factory diagnostic"))
        return 1;

    auto sourceNode = MakeSourceNode();
    const auto createResult =
        factory.CreateImageResultNode(sourceNode.GetPointer(),
                                      MakeExecutionResult(),
                                      QStringLiteral("cropped"));
    if (Expect(createResult.Succeeded,
               "factory should create result nodes for valid image results"))
        return 1;
    if (Expect(createResult.Node.IsNotNull(),
               "factory should return a result node"))
        return 1;
    if (Expect(QString::fromStdString(createResult.Node->GetName()) ==
                   QStringLiteral("CTA_cropped"),
               "result node should use source name and suffix"))
        return 1;
    if (Expect(dynamic_cast<mitk::Image*>(createResult.Node->GetData()) !=
                   nullptr,
               "result node should contain a MITK image"))
        return 1;

    std::string value;
    if (Expect(createResult.Node->GetStringProperty("xq.type", value) &&
                   value == "image_processing_result",
               "result node should expose legacy result type"))
        return 1;
    if (Expect(createResult.Node->GetStringProperty(
                   "xq.image.processing.operation", value) &&
                   value == "crop",
               "result node should expose operation metadata"))
        return 1;
    if (Expect(createResult.Node->GetStringProperty(
                   "xq.image.processing.source_node", value) &&
                   value == "CTA",
               "result node should expose source node metadata"))
        return 1;
    if (Expect(createResult.Node->GetStringProperty("xq.source.image", value) &&
                   value == "CTA",
               "result node should expose source image metadata"))
        return 1;

    bool outputIsImage = false;
    if (Expect(createResult.Node->GetBoolProperty(
                   "xq.image.processing.output_is_image", outputIsImage) &&
                   outputIsImage,
               "result node should mark image outputs"))
        return 1;
    if (Expect(xq::pipeline::HasStage(createResult.Node.GetPointer(),
                                      xq::pipeline::Stage::ImageProcessing),
               "result node should be marked as image-processing pipeline output"))
        return 1;
    if (Expect(xq::pipeline::GetStringProperty(
                   createResult.Node.GetPointer(),
                   xq::pipeline::kAlgorithmProperty) == "crop",
               "result node should store pipeline algorithm metadata"))
        return 1;

    return 0;
}
