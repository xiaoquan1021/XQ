#include "Infrastructure/xq_ImagePreprocessingAlgorithmAdapter.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <cmath>
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

bool NearlyEqual(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 0.000001;
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

vtkSmartPointer<vtkImageData> MakeBinaryImage()
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
                *voxel =
                    (x >= 1 && x <= 3 &&
                     y >= 1 && y <= 3 &&
                     z >= 1 && z <= 3)
                        ? 1.0f
                        : 0.0f;
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

QVariantMap BinaryThresholdParameters()
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("lower"), 3.0);
    parameters.insert(QStringLiteral("upper"), 6.0);
    parameters.insert(QStringLiteral("inside-value"), 1.0);
    parameters.insert(QStringLiteral("outside-value"), 0.0);
    return parameters;
}

QVariantMap MorphologyParameters(int radius)
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("radius"), radius);
    return parameters;
}

QVariantList Point(int x, int y, int z)
{
    QVariantList point;
    point.append(x);
    point.append(y);
    point.append(z);
    return point;
}

QVariantMap ConnectedThresholdParameters()
{
    QVariantList seeds;
    seeds.append(QVariant::fromValue(Point(1, 1, 1)));

    QVariantMap parameters;
    parameters.insert(QStringLiteral("lower"), 3.0);
    parameters.insert(QStringLiteral("upper"), 6.0);
    parameters.insert(QStringLiteral("seeds"), seeds);
    return parameters;
}

QVariantMap CropParameters(int ox, int oy, int oz,
                           int sx, int sy, int sz)
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("origin-x"), ox);
    parameters.insert(QStringLiteral("origin-y"), oy);
    parameters.insert(QStringLiteral("origin-z"), oz);
    parameters.insert(QStringLiteral("size-x"), sx);
    parameters.insert(QStringLiteral("size-y"), sy);
    parameters.insert(QStringLiteral("size-z"), sz);
    return parameters;
}

QVariantMap ResampleParameters(double sx, double sy, double sz)
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("spacing-x"), sx);
    parameters.insert(QStringLiteral("spacing-y"), sy);
    parameters.insert(QStringLiteral("spacing-z"), sz);
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

    const auto nullThresholdResult =
        adapter.RunBinaryThreshold(nullptr, BinaryThresholdParameters());
    if (Expect(!nullThresholdResult.Succeeded,
               "null binary-threshold input should fail"))
        return 1;
    if (Expect(nullThresholdResult.Message ==
                   QStringLiteral("BinaryThreshold: input image is null."),
               "null binary-threshold input should use legacy diagnostic"))
        return 1;

    const auto missingThresholdParameterResult =
        adapter.RunBinaryThreshold(MakeImage(), QVariantMap());
    if (Expect(!missingThresholdParameterResult.Succeeded,
               "missing binary-threshold parameters should fail"))
        return 1;
    if (Expect(missingThresholdParameterResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: lower."),
               "missing binary-threshold parameter should use domain validation"))
        return 1;
    if (Expect(missingThresholdParameterResult.Image == nullptr,
               "invalid binary-threshold parameters should not produce image"))
        return 1;

    input = MakeImage();
    inputDimensions = input->GetDimensions();
    const auto thresholdResult =
        adapter.RunBinaryThreshold(input, BinaryThresholdParameters());
    if (Expect(thresholdResult.Succeeded,
               "valid binary-threshold request should succeed"))
        return 1;
    if (Expect(thresholdResult.Image != nullptr,
               "valid binary-threshold request should return image"))
        return 1;
    outputDimensions = thresholdResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == inputDimensions[0] &&
                   outputDimensions[1] == inputDimensions[1] &&
                   outputDimensions[2] == inputDimensions[2],
               "binary threshold should preserve image dimensions"))
        return 1;

    const auto* insideVoxel =
        static_cast<float*>(thresholdResult.Image->GetScalarPointer(1, 1, 1));
    if (Expect(insideVoxel && *insideVoxel == 1.0f,
               "binary threshold should write inside value for in-range voxel"))
        return 1;
    const auto* outsideVoxel =
        static_cast<float*>(thresholdResult.Image->GetScalarPointer(0, 0, 0));
    if (Expect(outsideVoxel && *outsideVoxel == 0.0f,
               "binary threshold should write outside value for out-of-range voxel"))
        return 1;

    const auto nullConnectedThresholdResult =
        adapter.RunConnectedThreshold(nullptr, ConnectedThresholdParameters());
    if (Expect(!nullConnectedThresholdResult.Succeeded,
               "null connected-threshold input should fail"))
        return 1;
    if (Expect(nullConnectedThresholdResult.Message ==
                   QStringLiteral("ConnectedThreshold: input image is null."),
               "null connected-threshold input should use legacy diagnostic"))
        return 1;

    const auto missingSeedsResult =
        adapter.RunConnectedThreshold(MakeImage(), QVariantMap());
    if (Expect(!missingSeedsResult.Succeeded,
               "missing connected-threshold seeds should fail"))
        return 1;
    if (Expect(missingSeedsResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: lower."),
               "missing connected-threshold parameter should use domain validation"))
        return 1;
    if (Expect(missingSeedsResult.Image == nullptr,
               "invalid connected-threshold parameters should not produce image"))
        return 1;

    input = MakeImage();
    inputDimensions = input->GetDimensions();
    const auto connectedThresholdResult =
        adapter.RunConnectedThreshold(input, ConnectedThresholdParameters());
    if (Expect(connectedThresholdResult.Succeeded,
               "valid connected-threshold request should succeed"))
        return 1;
    if (Expect(connectedThresholdResult.Image != nullptr,
               "valid connected-threshold request should return image"))
        return 1;
    outputDimensions = connectedThresholdResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == inputDimensions[0] &&
                   outputDimensions[1] == inputDimensions[1] &&
                   outputDimensions[2] == inputDimensions[2],
               "connected threshold should preserve image dimensions"))
        return 1;

    const auto* connectedSeedVoxel = static_cast<float*>(
        connectedThresholdResult.Image->GetScalarPointer(1, 1, 1));
    if (Expect(connectedSeedVoxel && *connectedSeedVoxel == 1.0f,
               "connected threshold should select the seeded in-range voxel"))
        return 1;

    const auto nullMorphologyResult =
        adapter.RunMorphologyOpenClose(nullptr, MorphologyParameters(1));
    if (Expect(!nullMorphologyResult.Succeeded,
               "null morphology input should fail"))
        return 1;
    if (Expect(nullMorphologyResult.Message ==
                   QStringLiteral("MorphologicalOpenClose: input image is null."),
               "null morphology input should use legacy diagnostic"))
        return 1;

    const auto missingMorphologyRadiusResult =
        adapter.RunMorphologyOpenClose(MakeBinaryImage(), QVariantMap());
    if (Expect(!missingMorphologyRadiusResult.Succeeded,
               "missing morphology radius should fail"))
        return 1;
    if (Expect(missingMorphologyRadiusResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: radius."),
               "missing morphology radius should use domain validation"))
        return 1;
    if (Expect(missingMorphologyRadiusResult.Image == nullptr,
               "invalid morphology parameters should not produce image"))
        return 1;

    const auto invalidMorphologyRadiusResult =
        adapter.RunMorphologyOpenClose(MakeBinaryImage(), MorphologyParameters(0));
    if (Expect(!invalidMorphologyRadiusResult.Succeeded,
               "invalid morphology radius should fail"))
        return 1;
    if (Expect(invalidMorphologyRadiusResult.Message ==
                   QStringLiteral("MorphologicalOpenClose: radius must be >= 1."),
               "invalid morphology radius should use legacy diagnostic"))
        return 1;

    input = MakeBinaryImage();
    inputDimensions = input->GetDimensions();
    const auto morphologyResult =
        adapter.RunMorphologyOpenClose(input, MorphologyParameters(1));
    if (Expect(morphologyResult.Succeeded,
               "valid morphology request should succeed"))
        return 1;
    if (Expect(morphologyResult.Image != nullptr,
               "valid morphology request should return image"))
        return 1;
    outputDimensions = morphologyResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == inputDimensions[0] &&
                   outputDimensions[1] == inputDimensions[1] &&
                   outputDimensions[2] == inputDimensions[2],
               "morphology should preserve image dimensions"))
        return 1;

    const auto nullCropResult =
        adapter.RunCrop(nullptr, CropParameters(1, 1, 1, 3, 2, 4));
    if (Expect(!nullCropResult.Succeeded,
               "null crop input should fail"))
        return 1;
    if (Expect(nullCropResult.Message ==
                   QStringLiteral("Crop: input image is null."),
               "null crop input should use legacy diagnostic"))
        return 1;

    const auto missingCropParameterResult =
        adapter.RunCrop(MakeImage(), QVariantMap());
    if (Expect(!missingCropParameterResult.Succeeded,
               "missing crop parameters should fail"))
        return 1;
    if (Expect(missingCropParameterResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: origin-x."),
               "missing crop parameter should use domain validation"))
        return 1;
    if (Expect(missingCropParameterResult.Image == nullptr,
               "invalid crop parameters should not produce image"))
        return 1;

    const auto outOfBoundsCropResult =
        adapter.RunCrop(MakeImage(), CropParameters(4, 4, 4, 2, 2, 2));
    if (Expect(!outOfBoundsCropResult.Succeeded,
               "out-of-bounds crop should fail"))
        return 1;
    if (Expect(outOfBoundsCropResult.Message ==
                   QStringLiteral("Crop: requested region [4,4,4] + size [2,2,2] is outside the image dimensions [5,5,5]."),
               "out-of-bounds crop should use legacy diagnostic"))
        return 1;

    const auto cropResult =
        adapter.RunCrop(MakeImage(), CropParameters(1, 1, 0, 3, 2, 4));
    if (Expect(cropResult.Succeeded,
               "valid crop request should succeed"))
        return 1;
    if (Expect(cropResult.Image != nullptr,
               "valid crop request should return image"))
        return 1;
    outputDimensions = cropResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == 3 &&
                   outputDimensions[1] == 2 &&
                   outputDimensions[2] == 4,
               "crop should return requested output dimensions"))
        return 1;

    const auto nullResampleResult =
        adapter.RunResample(nullptr, ResampleParameters(0.5, 1.0, 2.0));
    if (Expect(!nullResampleResult.Succeeded,
               "null resample input should fail"))
        return 1;
    if (Expect(nullResampleResult.Message ==
                   QStringLiteral("Resample: input image is null."),
               "null resample input should use legacy diagnostic"))
        return 1;

    const auto missingResampleParameterResult =
        adapter.RunResample(MakeImage(), QVariantMap());
    if (Expect(!missingResampleParameterResult.Succeeded,
               "missing resample parameters should fail"))
        return 1;
    if (Expect(missingResampleParameterResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: spacing-x."),
               "missing resample parameter should use domain validation"))
        return 1;
    if (Expect(missingResampleParameterResult.Image == nullptr,
               "invalid resample parameters should not produce image"))
        return 1;

    const auto invalidResampleSpacingResult =
        adapter.RunResample(MakeImage(), ResampleParameters(0.5, 0.0, 2.0));
    if (Expect(!invalidResampleSpacingResult.Succeeded,
               "non-positive resample spacing should fail"))
        return 1;
    if (Expect(invalidResampleSpacingResult.Message ==
                   QStringLiteral("Resample: output spacing must be positive in all axes."),
               "non-positive resample spacing should use legacy diagnostic"))
        return 1;

    const auto resampleResult =
        adapter.RunResample(MakeImage(), ResampleParameters(0.5, 1.0, 2.0));
    if (Expect(resampleResult.Succeeded,
               "valid resample request should succeed"))
        return 1;
    if (Expect(resampleResult.Image != nullptr,
               "valid resample request should return image"))
        return 1;
    outputDimensions = resampleResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == 10 &&
                   outputDimensions[1] == 5 &&
                   outputDimensions[2] == 3,
               "resample should return expected output dimensions"))
        return 1;
    const double* outputSpacing = resampleResult.Image->GetSpacing();
    if (Expect(outputSpacing &&
                   NearlyEqual(outputSpacing[0], 0.5) &&
                   NearlyEqual(outputSpacing[1], 1.0) &&
                   NearlyEqual(outputSpacing[2], 2.0),
               "resample should return requested output spacing"))
        return 1;

    const auto unknownDispatchResult =
        adapter.RunOperation(QStringLiteral("missing-operation"),
                             MakeImage(),
                             QVariantMap());
    if (Expect(!unknownDispatchResult.Succeeded,
               "unknown operation dispatch should fail"))
        return 1;
    if (Expect(unknownDispatchResult.Message ==
                   QStringLiteral("Image preprocessing operation was not found."),
               "unknown operation dispatch should use domain diagnostic"))
        return 1;

    const auto dispatchedGaussianResult =
        adapter.RunOperation(QStringLiteral("gaussian-smoothing"),
                             MakeImage(),
                             GaussianParameters(0.75));
    if (Expect(dispatchedGaussianResult.Succeeded,
               "dispatch should run Gaussian smoothing"))
        return 1;

    const auto dispatchedBinaryThresholdResult =
        adapter.RunOperation(QStringLiteral("binary-threshold"),
                             MakeImage(),
                             BinaryThresholdParameters());
    if (Expect(dispatchedBinaryThresholdResult.Succeeded,
               "dispatch should run binary threshold"))
        return 1;

    const auto dispatchedConnectedThresholdResult =
        adapter.RunOperation(QStringLiteral("connected-threshold"),
                             MakeImage(),
                             ConnectedThresholdParameters());
    if (Expect(dispatchedConnectedThresholdResult.Succeeded,
               "dispatch should run connected threshold"))
        return 1;

    const auto dispatchedMorphologyResult =
        adapter.RunOperation(QStringLiteral("morphology-open-close"),
                             MakeBinaryImage(),
                             MorphologyParameters(1));
    if (Expect(dispatchedMorphologyResult.Succeeded,
               "dispatch should run morphology open/close"))
        return 1;

    const auto dispatchedCropResult =
        adapter.RunOperation(QStringLiteral("crop"),
                             MakeImage(),
                             CropParameters(1, 1, 0, 3, 2, 4));
    if (Expect(dispatchedCropResult.Succeeded,
               "dispatch should run crop"))
        return 1;
    outputDimensions = dispatchedCropResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == 3 &&
                   outputDimensions[1] == 2 &&
                   outputDimensions[2] == 4,
               "crop dispatch should preserve crop output dimensions"))
        return 1;

    const auto dispatchedResampleResult =
        adapter.RunOperation(QStringLiteral("resample"),
                             MakeImage(),
                             ResampleParameters(0.5, 1.0, 2.0));
    if (Expect(dispatchedResampleResult.Succeeded,
               "dispatch should run resample"))
        return 1;
    outputDimensions = dispatchedResampleResult.Image->GetDimensions();
    if (Expect(outputDimensions[0] == 10 &&
                   outputDimensions[1] == 5 &&
                   outputDimensions[2] == 3,
               "resample dispatch should preserve resample output dimensions"))
        return 1;

    return 0;
}
