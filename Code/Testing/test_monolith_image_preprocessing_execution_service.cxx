#include "Infrastructure/xq_ImagePreprocessingExecutionService.h"

#include <QCoreApplication>
#include <QVariantMap>

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

xq::core::WorkflowContextSnapshot MakeSnapshot(
    const QString& workflowId,
    xq::core::DataWorkflowRole role = xq::core::DataWorkflowRole::Image)
{
    xq::core::WorkflowContextSnapshot snapshot;
    snapshot.WorkflowId = workflowId;
    snapshot.WorkflowTitle = QStringLiteral("Image Preprocessing");
    snapshot.RequiresSelectedData = true;
    snapshot.HasSelectedData = true;
    snapshot.HasCompatibleSelection = true;
    snapshot.SelectedCatalogEntryId = QStringLiteral("image-001");
    snapshot.SelectedDataDisplayName = QStringLiteral("CTA Image");
    snapshot.SelectedDataRole = role;
    return snapshot;
}

vtkSmartPointer<vtkImageData> MakeImage()
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(5, 5, 5);
    image->AllocateScalars(VTK_FLOAT, 1);

    for (int z = 0; z < 5; ++z)
    {
        for (int y = 0; y < 5; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                auto* voxel =
                    static_cast<float*>(image->GetScalarPointer(x, y, z));
                *voxel = static_cast<float>(x + y + z);
            }
        }
    }

    return image;
}

QVariantMap CropParameters()
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("origin-x"), 1);
    parameters.insert(QStringLiteral("origin-y"), 1);
    parameters.insert(QStringLiteral("origin-z"), 0);
    parameters.insert(QStringLiteral("size-x"), 3);
    parameters.insert(QStringLiteral("size-y"), 2);
    parameters.insert(QStringLiteral("size-z"), 4);
    return parameters;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingExecutionService service;

    xq::infrastructure::ImagePreprocessingExecutionRequest request;
    request.Context = MakeSnapshot(QStringLiteral("path"));
    request.OperationId = QStringLiteral("crop");
    request.Parameters = CropParameters();
    auto inputImage = MakeImage();
    request.InputImage = inputImage;

    const auto wrongWorkflowResult = service.Run(request);
    if (Expect(!wrongWorkflowResult.Succeeded,
               "execution service should reject incompatible workflow"))
        return 1;
    if (Expect(wrongWorkflowResult.Message ==
                   QStringLiteral("Image preprocessing service requires the image-preprocessing workflow."),
               "wrong workflow should use domain diagnostic"))
        return 1;

    request.Context = MakeSnapshot(QStringLiteral("image-preprocessing"));
    request.OperationId = QStringLiteral("missing-operation");
    const auto unknownOperationResult = service.Run(request);
    if (Expect(!unknownOperationResult.Succeeded,
               "execution service should reject unknown operation ids"))
        return 1;
    if (Expect(unknownOperationResult.Message ==
                   QStringLiteral("Image preprocessing operation was not found."),
               "unknown operation should use domain diagnostic"))
        return 1;

    request.OperationId = QStringLiteral("crop");
    request.InputImage = nullptr;
    const auto nullInputResult = service.Run(request);
    if (Expect(!nullInputResult.Succeeded,
               "execution service should forward algorithm failures"))
        return 1;
    if (Expect(nullInputResult.Message ==
                   QStringLiteral("Crop: input image is null."),
               "null input should use legacy algorithm diagnostic"))
        return 1;

    inputImage = MakeImage();
    request.InputImage = inputImage;
    const auto cropResult = service.Run(request);
    if (Expect(cropResult.Succeeded,
               "execution service should run valid crop requests"))
        return 1;
    if (Expect(cropResult.SourceCatalogEntryId ==
                   QStringLiteral("image-001"),
               "execution result should preserve source catalog id"))
        return 1;
    if (Expect(cropResult.SelectedDataDisplayName ==
                   QStringLiteral("CTA Image"),
               "execution result should preserve selected data display name"))
        return 1;
    if (Expect(cropResult.OperationId == QStringLiteral("crop"),
               "execution result should preserve operation id"))
        return 1;
    if (Expect(cropResult.OperationTitle == QStringLiteral("Crop"),
               "execution result should preserve operation title"))
        return 1;
    if (Expect(cropResult.Image != nullptr,
               "execution result should carry output image"))
        return 1;

    const int* dimensions = cropResult.Image->GetDimensions();
    if (Expect(dimensions[0] == 3 &&
                   dimensions[1] == 2 &&
                   dimensions[2] == 4,
               "execution service should preserve algorithm output dimensions"))
        return 1;
    if (Expect(cropResult.Message ==
                   QStringLiteral("Crop preprocessing operation completed CTA Image."),
               "successful execution should report operation and data names"))
        return 1;

    return 0;
}
