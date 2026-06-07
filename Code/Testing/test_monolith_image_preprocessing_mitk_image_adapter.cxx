#include "Infrastructure/xq_ImagePreprocessingMitkImageAdapter.h"

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
    image->SetDimensions(4, 3, 2);
    image->AllocateScalars(VTK_FLOAT, 1);

    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                auto* voxel =
                    static_cast<float*>(image->GetScalarPointer(x, y, z));
                *voxel = static_cast<float>(x + y + z);
            }
        }
    }

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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingMitkImageAdapter adapter;

    const auto nullNodeResult = adapter.ExtractInputImage(nullptr);
    if (Expect(!nullNodeResult.Succeeded,
               "null node extraction should fail"))
        return 1;
    if (Expect(nullNodeResult.Message ==
                   QStringLiteral("Image preprocessing input node is required."),
               "null node extraction should use adapter diagnostic"))
        return 1;

    auto emptyNode = mitk::DataNode::New();
    const auto emptyNodeResult =
        adapter.ExtractInputImage(emptyNode.GetPointer());
    if (Expect(!emptyNodeResult.Succeeded,
               "empty node extraction should fail"))
        return 1;
    if (Expect(emptyNodeResult.Message ==
                   QStringLiteral("Selected node does not contain a usable image."),
               "empty node extraction should use usable-image diagnostic"))
        return 1;

    auto sourceVtk = MakeVtkImage();
    auto sourceMitk = MakeMitkImage(sourceVtk);
    auto imageNode = mitk::DataNode::New();
    imageNode->SetData(sourceMitk);

    const auto inputResult =
        adapter.ExtractInputImage(imageNode.GetPointer());
    if (Expect(inputResult.Succeeded,
               "valid MITK image node extraction should succeed"))
        return 1;
    if (Expect(inputResult.Image != nullptr,
               "valid MITK image extraction should return vtk image"))
        return 1;
    const int* inputDimensions = inputResult.Image->GetDimensions();
    if (Expect(inputDimensions[0] == 4 &&
                   inputDimensions[1] == 3 &&
                   inputDimensions[2] == 2,
               "extracted vtk image should preserve dimensions"))
        return 1;

    const auto nullCreateResult = adapter.CreateMitkImage(nullptr);
    if (Expect(!nullCreateResult.Succeeded,
               "null VTK image conversion should fail"))
        return 1;
    if (Expect(nullCreateResult.Message ==
                   QStringLiteral("Image preprocessing output image is required."),
               "null VTK image conversion should use adapter diagnostic"))
        return 1;

    auto outputVtk = MakeVtkImage();
    const auto createResult = adapter.CreateMitkImage(outputVtk);
    if (Expect(createResult.Succeeded,
               "valid VTK image conversion should succeed"))
        return 1;
    if (Expect(createResult.Image.IsNotNull(),
               "valid VTK image conversion should return MITK image"))
        return 1;
    auto* createdVtk = createResult.Image->GetVtkImageData();
    if (Expect(createdVtk != nullptr,
               "created MITK image should expose vtk image data"))
        return 1;
    if (Expect(createdVtk != outputVtk.GetPointer(),
               "created MITK image should deep-copy the VTK image"))
        return 1;

    const int* createdDimensions = createdVtk->GetDimensions();
    if (Expect(createdDimensions[0] == 4 &&
                   createdDimensions[1] == 3 &&
                   createdDimensions[2] == 2,
               "created MITK image should preserve VTK dimensions"))
        return 1;

    return 0;
}
