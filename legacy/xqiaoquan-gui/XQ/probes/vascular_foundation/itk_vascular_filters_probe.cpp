#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkCurvatureAnisotropicDiffusionImageFilter.h>
#include <itkHessianToObjectnessMeasureImageFilter.h>
#include <itkImage.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkMultiScaleHessianBasedMeasureImageFilter.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkSymmetricSecondRankTensor.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

template <typename T>
bool finite(T value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

int main()
{
    constexpr unsigned int Dimension = 3;
    using FloatImage = itk::Image<float, Dimension>;
    using BinaryImage = itk::Image<unsigned char, Dimension>;
    using LabelImage = itk::Image<unsigned int, Dimension>;

    FloatImage::SizeType size;
    size.Fill(25);
    FloatImage::IndexType start;
    start.Fill(0);
    FloatImage::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    auto image = FloatImage::New();
    image->SetRegions(region);
    FloatImage::SpacingType spacing;
    spacing[0] = 0.7;
    spacing[1] = 0.9;
    spacing[2] = 1.3;
    image->SetSpacing(spacing);
    image->Allocate();

    itk::ImageRegionIterator<FloatImage> writer(image, region);
    for (writer.GoToBegin(); !writer.IsAtEnd(); ++writer) {
        const auto index = writer.GetIndex();
        const double z = static_cast<double>(index[2]);
        const double cx = 12.0 + 0.08 * (z - 12.0);
        const double cy = 12.0 - 0.05 * (z - 12.0);
        const double dx = static_cast<double>(index[0]) - cx;
        const double dy = static_cast<double>(index[1]) - cy;
        const double tube = 220.0 * std::exp(-(dx * dx + dy * dy) / 7.0);
        const double deterministicNoise = static_cast<double>((index[0] * 17 + index[1] * 11
                                                                + index[2] * 5) % 13) - 6.0;
        writer.Set(static_cast<float>(tube + deterministicNoise));
    }

    using Diffusion = itk::CurvatureAnisotropicDiffusionImageFilter<FloatImage, FloatImage>;
    auto diffusion = Diffusion::New();
    diffusion->SetInput(image);
    diffusion->SetNumberOfIterations(3);
    diffusion->SetTimeStep(0.04);
    diffusion->SetConductanceParameter(2.0);

    using HessianPixel = itk::SymmetricSecondRankTensor<double, Dimension>;
    using HessianImage = itk::Image<HessianPixel, Dimension>;
    using Objectness = itk::HessianToObjectnessMeasureImageFilter<HessianImage, FloatImage>;
    auto objectness = Objectness::New();
    objectness->SetBrightObject(true);
    objectness->SetScaleObjectnessMeasure(false);
    objectness->SetObjectDimension(1);
    objectness->SetAlpha(0.5);
    objectness->SetBeta(0.5);
    objectness->SetGamma(5.0);

    using Multiscale =
        itk::MultiScaleHessianBasedMeasureImageFilter<FloatImage, HessianImage, FloatImage>;
    auto multiscale = Multiscale::New();
    multiscale->SetInput(diffusion->GetOutput());
    multiscale->SetHessianToMeasureFilter(objectness);
    multiscale->SetSigmaMinimum(0.8);
    multiscale->SetSigmaMaximum(2.0);
    multiscale->SetNumberOfSigmaSteps(3);
    multiscale->SetSigmaStepMethodToLogarithmic();
    multiscale->Update();

    float maximum = 0.0F;
    itk::ImageRegionConstIterator<FloatImage> measure(multiscale->GetOutput(), region);
    for (measure.GoToBegin(); !measure.IsAtEnd(); ++measure) {
        if (!finite(measure.Get())) {
            std::cerr << "non-finite vesselness value\n";
            return EXIT_FAILURE;
        }
        maximum = std::max(maximum, measure.Get());
    }
    if (!(maximum > 0.0F)) {
        std::cerr << "vesselness did not produce a positive response\n";
        return EXIT_FAILURE;
    }

    using Threshold = itk::BinaryThresholdImageFilter<FloatImage, BinaryImage>;
    auto threshold = Threshold::New();
    threshold->SetInput(multiscale->GetOutput());
    threshold->SetLowerThreshold(maximum * 0.18F);
    threshold->SetUpperThreshold(std::numeric_limits<float>::max());
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);

    using Kernel = itk::BinaryBallStructuringElement<unsigned char, Dimension>;
    Kernel kernel;
    Kernel::SizeType radius;
    radius.Fill(1);
    kernel.SetRadius(radius);
    kernel.CreateStructuringElement();

    using Closing = itk::BinaryMorphologicalClosingImageFilter<BinaryImage, BinaryImage, Kernel>;
    auto closing = Closing::New();
    closing->SetInput(threshold->GetOutput());
    closing->SetKernel(kernel);
    closing->SetForegroundValue(1);

    using Connected = itk::ConnectedComponentImageFilter<BinaryImage, LabelImage>;
    auto connected = Connected::New();
    connected->SetInput(closing->GetOutput());

    using Relabel = itk::RelabelComponentImageFilter<LabelImage, LabelImage>;
    auto relabel = Relabel::New();
    relabel->SetInput(connected->GetOutput());
    relabel->Update();
    if (relabel->GetNumberOfObjects() == 0) {
        std::cerr << "connected-component stage produced no object\n";
        return EXIT_FAILURE;
    }

    using Distance = itk::SignedMaurerDistanceMapImageFilter<BinaryImage, FloatImage>;
    auto distance = Distance::New();
    distance->SetInput(closing->GetOutput());
    distance->SetUseImageSpacing(true);
    distance->SetInsideIsPositive(true);
    distance->SetSquaredDistance(false);
    distance->Update();

    std::size_t foreground = 0;
    float maximumDistance = 0.0F;
    itk::ImageRegionConstIterator<BinaryImage> binary(closing->GetOutput(), region);
    itk::ImageRegionConstIterator<FloatImage> distances(distance->GetOutput(), region);
    for (binary.GoToBegin(), distances.GoToBegin(); !binary.IsAtEnd(); ++binary, ++distances) {
        if (!finite(distances.Get())) {
            std::cerr << "non-finite distance-map value\n";
            return EXIT_FAILURE;
        }
        if (binary.Get() != 0) {
            ++foreground;
            maximumDistance = std::max(maximumDistance, distances.Get());
        }
    }

    if (foreground < 25 || maximumDistance <= 0.0F) {
        std::cerr << "vascular filter chain produced a degenerate mask\n";
        return EXIT_FAILURE;
    }

    std::cout << "PASS itk_vascular_filters_probe"
              << " vesselness_max=" << maximum
              << " foreground=" << foreground
              << " components=" << relabel->GetNumberOfObjects()
              << " distance_max_mm=" << maximumDistance << '\n';
    return EXIT_SUCCESS;
}
