#include "xq_ImagePreprocessingMitkImageAdapter.h"

#include <vtkSmartPointer.h>

namespace xq::infrastructure
{

namespace
{

ImagePreprocessingMitkInputImageResult FailedInputResult(
    const QString& message)
{
    ImagePreprocessingMitkInputImageResult result;
    result.Message = message;
    return result;
}

ImagePreprocessingMitkImageCreationResult FailedCreationResult(
    const QString& message)
{
    ImagePreprocessingMitkImageCreationResult result;
    result.Message = message;
    return result;
}

} // namespace

ImagePreprocessingMitkInputImageResult
ImagePreprocessingMitkImageAdapter::ExtractInputImage(
    mitk::DataNode* node) const
{
    if (!node)
    {
        return FailedInputResult(QStringLiteral(
            "Image preprocessing input node is required."));
    }

    auto* image = dynamic_cast<mitk::Image*>(node->GetData());
    if (!image || !image->GetVtkImageData())
    {
        return FailedInputResult(QStringLiteral(
            "Selected node does not contain a usable image."));
    }

    ImagePreprocessingMitkInputImageResult result;
    result.Succeeded = true;
    result.Image = image->GetVtkImageData();
    return result;
}

ImagePreprocessingMitkImageCreationResult
ImagePreprocessingMitkImageAdapter::CreateMitkImage(vtkImageData* image) const
{
    if (!image)
    {
        return FailedCreationResult(QStringLiteral(
            "Image preprocessing output image is required."));
    }

    auto templateImage = vtkSmartPointer<vtkImageData>::New();
    templateImage->DeepCopy(image);

    auto mitkImage = mitk::Image::New();
    mitkImage->Initialize(templateImage);

    auto* output = mitkImage->GetVtkImageData();
    if (!output)
    {
        return FailedCreationResult(QStringLiteral(
            "Failed to create MITK image from VTK output."));
    }

    output->DeepCopy(image);

    ImagePreprocessingMitkImageCreationResult result;
    result.Succeeded = true;
    result.Image = mitkImage;
    return result;
}

} // namespace xq::infrastructure
