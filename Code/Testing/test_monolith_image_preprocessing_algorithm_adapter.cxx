#include "Infrastructure/xq_ImagePreprocessingAlgorithmAdapter.h"

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

QVariantMap GaussianParameters(double sigma)
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("sigma"), sigma);
    return parameters;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingAlgorithmAdapter adapter;

    const auto nullResult =
        adapter.RunGaussianSmoothing(nullptr, GaussianParameters(1.0));
    if (Expect(!nullResult.Succeeded,
               "null Gaussian smoothing input should fail"))
        return 1;
    if (Expect(nullResult.Message ==
                   QStringLiteral("SmoothGaussian: input image is null."),
               "null input should use legacy diagnostic"))
        return 1;

    const auto missingSigmaResult =
        adapter.RunGaussianSmoothing(MakeImage(), QVariantMap());
    if (Expect(!missingSigmaResult.Succeeded,
               "missing Gaussian smoothing sigma should fail"))
        return 1;
    if (Expect(missingSigmaResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: sigma."),
               "missing sigma should use domain validation diagnostic"))
        return 1;
    if (Expect(missingSigmaResult.Image == nullptr,
               "invalid Gaussian smoothing parameters should not produce image"))
        return 1;

    auto input = MakeImage();
    const int* inputDimensions = input->GetDimensions();
    const auto smoothResult =
        adapter.RunGaussianSmoothing(input, GaussianParameters(0.75));
    if (Expect(smoothResult.Succeeded,
               "valid Gaussian smoothing request should succeed"))
        return 1;
    if (Expect(smoothResult.Image != nullptr,
               "valid Gaussian smoothing request should return image"))
        return 1;
    const int* outputDimensions = smoothResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == inputDimensions[0] &&
                   outputDimensions[1] == inputDimensions[1] &&
                   outputDimensions[2] == inputDimensions[2],
               "Gaussian smoothing should preserve image dimensions"))
        return 1;

    return 0;
}
