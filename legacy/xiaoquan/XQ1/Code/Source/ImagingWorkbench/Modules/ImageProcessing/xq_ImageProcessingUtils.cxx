#include "xq_ImageProcessingUtils.h"

#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryDilateImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkIdentityTransform.h>
#include <itkImage.h>
#include <itkImageToVTKImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkVTKImageToImageFilter.h>

#include <itkRegionOfInterestImageFilter.h>

#include <cmath>
#include <sstream>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

namespace
{

// Wrap an ITK exception into an xq_ImageResult failure.
template <typename F>
xq_ImageResult CatchItk(F&& fn)
{
    try
    {
        return fn();
    }
    catch (const itk::ExceptionObject& e)
    {
        xq_ImageResult r;
        r.ok = false;
        r.diagnostic = std::string("ITK exception: ") + e.GetDescription();
        return r;
    }
    catch (const std::exception& e)
    {
        xq_ImageResult r;
        r.ok = false;
        r.diagnostic = std::string("std::exception: ") + e.what();
        return r;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// VtkImageToItkFloat3D
// ---------------------------------------------------------------------------
xq_ImageProcessingUtils::ItkFloat3D::Pointer
xq_ImageProcessingUtils::VtkImageToItkFloat3D(vtkImageData* vtkImage)
{
    if (!vtkImage)
        return nullptr;

    using ConnectorType = itk::VTKImageToImageFilter<ItkFloat3D>;
    auto connector = ConnectorType::New();
    connector->SetInput(vtkImage);
    try
    {
        connector->Update();
    }
    catch (const itk::ExceptionObject&)
    {
        return nullptr;
    }
    return connector->GetOutput();
}

// ---------------------------------------------------------------------------
// ItkFloat3DToVtkImage
// ---------------------------------------------------------------------------
vtkSmartPointer<vtkImageData>
xq_ImageProcessingUtils::ItkFloat3DToVtkImage(ItkFloat3D::Pointer itkImage)
{
    if (!itkImage)
        return nullptr;

    using ConnectorType = itk::ImageToVTKImageFilter<ItkFloat3D>;
    auto connector = ConnectorType::New();
    connector->SetInput(itkImage);
    try
    {
        connector->Update();
    }
    catch (const itk::ExceptionObject&)
    {
        return nullptr;
    }
    // Deep-copy: ITK connector exports the ITK buffer without copying,
    // so the VTK image would hold a dangling pointer once the ITK
    // pipeline objects are destroyed.
    auto shallow = connector->GetOutput();
    auto deep = vtkSmartPointer<vtkImageData>::New();
    deep->DeepCopy(shallow);
    return deep;
}

// ---------------------------------------------------------------------------
// BinaryThreshold
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::BinaryThreshold(
    vtkImageData* input,
    double lower, double upper,
    double insideValue, double outsideValue)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "BinaryThreshold: input image is null.";
        return r;
    }

    int dims[3];
    input->GetDimensions(dims);
    if (dims[0] < 1 || dims[1] < 1 || dims[2] < 1)
    {
        r.diagnostic = "BinaryThreshold: input image has zero dimension.";
        return r;
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "BinaryThreshold: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        using FilterType = itk::BinaryThresholdImageFilter<ItkFloat3D, ItkFloat3D>;
        auto filter = FilterType::New();
        filter->SetInput(itkIn);
        filter->SetLowerThreshold(lower);
        filter->SetUpperThreshold(upper);
        filter->SetInsideValue(insideValue);
        filter->SetOutsideValue(outsideValue);
        filter->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(filter->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// ConnectedThreshold
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::ConnectedThreshold(
    vtkImageData* input,
    double lower, double upper,
    const std::vector<std::array<int, 3>>& seeds)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "ConnectedThreshold: input image is null.";
        return r;
    }

    if (seeds.empty())
    {
        r.diagnostic = "ConnectedThreshold: no seeds provided.";
        return r;
    }

    int dims[3];
    input->GetDimensions(dims);

    for (const auto& seed : seeds)
    {
        if (seed[0] < 0 || seed[0] >= dims[0] ||
            seed[1] < 0 || seed[1] >= dims[1] ||
            seed[2] < 0 || seed[2] >= dims[2])
        {
            std::ostringstream oss;
            oss << "ConnectedThreshold: seed (" << seed[0] << "," << seed[1]
                << "," << seed[2] << ") is outside image dimensions (" << dims[0]
                << "," << dims[1] << "," << dims[2] << ").";
            r.diagnostic = oss.str();
            return r;
        }
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "ConnectedThreshold: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        using FilterType = itk::ConnectedThresholdImageFilter<ItkFloat3D, ItkFloat3D>;
        auto filter = FilterType::New();
        filter->SetInput(itkIn);
        filter->SetLower(lower);
        filter->SetUpper(upper);
        filter->SetReplaceValue(1.0);

        for (const auto& seed : seeds)
        {
            ItkFloat3D::IndexType idx;
            idx[0] = static_cast<long>(seed[0]);
            idx[1] = static_cast<long>(seed[1]);
            idx[2] = static_cast<long>(seed[2]);
            filter->AddSeed(idx);
        }

        filter->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(filter->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// SmoothGaussian
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::SmoothGaussian(vtkImageData* input, double sigma)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "SmoothGaussian: input image is null.";
        return r;
    }

    if (sigma <= 0.0)
    {
        r.diagnostic = "SmoothGaussian: sigma must be positive.";
        return r;
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "SmoothGaussian: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        using FilterType = itk::SmoothingRecursiveGaussianImageFilter<ItkFloat3D, ItkFloat3D>;
        auto filter = FilterType::New();
        filter->SetInput(itkIn);
        filter->SetSigma(sigma);
        filter->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(filter->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// MorphologicalOpenClose
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::MorphologicalOpenClose(vtkImageData* input, int radius)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "MorphologicalOpenClose: input image is null.";
        return r;
    }

    if (radius < 1)
    {
        r.diagnostic = "MorphologicalOpenClose: radius must be >= 1.";
        return r;
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "MorphologicalOpenClose: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        using PixelType = ItkFloat3D::PixelType;
        using StructElType = itk::BinaryBallStructuringElement<PixelType, 3>;
        using ErodeFilterType = itk::BinaryErodeImageFilter<ItkFloat3D, ItkFloat3D, StructElType>;
        using DilateFilterType = itk::BinaryDilateImageFilter<ItkFloat3D, ItkFloat3D, StructElType>;

        StructElType ball;
        ball.SetRadius(radius);
        ball.CreateStructuringElement();

        // opening = dilate(erode(input))
        auto erode = ErodeFilterType::New();
        erode->SetInput(itkIn);
        erode->SetKernel(ball);
        erode->SetErodeValue(1.0);
        erode->SetBackgroundValue(0.0);

        auto dilateOpen = DilateFilterType::New();
        dilateOpen->SetInput(erode->GetOutput());
        dilateOpen->SetKernel(ball);
        dilateOpen->SetDilateValue(1.0);
        dilateOpen->SetBackgroundValue(0.0);

        // closing = erode(dilate(opened))
        auto dilateClose = DilateFilterType::New();
        dilateClose->SetInput(dilateOpen->GetOutput());
        dilateClose->SetKernel(ball);
        dilateClose->SetDilateValue(1.0);
        dilateClose->SetBackgroundValue(0.0);

        auto erodeClose = ErodeFilterType::New();
        erodeClose->SetInput(dilateClose->GetOutput());
        erodeClose->SetKernel(ball);
        erodeClose->SetErodeValue(1.0);
        erodeClose->SetBackgroundValue(0.0);

        erodeClose->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(erodeClose->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// Crop
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::Crop(
    vtkImageData* input,
    int ox, int oy, int oz,
    int sx, int sy, int sz)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "Crop: input image is null.";
        return r;
    }

    int dims[3];
    input->GetDimensions(dims);

    if (ox < 0 || oy < 0 || oz < 0 ||
        ox + sx > dims[0] || oy + sy > dims[1] || oz + sz > dims[2] ||
        sx < 1 || sy < 1 || sz < 1)
    {
        std::ostringstream oss;
        oss << "Crop: requested region [" << ox << "," << oy << "," << oz
            << "] + size [" << sx << "," << sy << "," << sz << "]"
            << " is outside the image dimensions [" << dims[0] << ","
            << dims[1] << "," << dims[2] << "].";
        r.diagnostic = oss.str();
        return r;
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "Crop: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        ItkFloat3D::RegionType desiredRegion;
        desiredRegion.SetIndex(0, static_cast<long>(ox));
        desiredRegion.SetIndex(1, static_cast<long>(oy));
        desiredRegion.SetIndex(2, static_cast<long>(oz));
        desiredRegion.SetSize(0, static_cast<unsigned long>(sx));
        desiredRegion.SetSize(1, static_cast<unsigned long>(sy));
        desiredRegion.SetSize(2, static_cast<unsigned long>(sz));

        using ROIFilterType = itk::RegionOfInterestImageFilter<ItkFloat3D, ItkFloat3D>;
        auto roiFilter = ROIFilterType::New();
        roiFilter->SetInput(itkIn);
        roiFilter->SetRegionOfInterest(desiredRegion);
        roiFilter->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(roiFilter->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// Resample
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::Resample(
    vtkImageData* input,
    double spacingX, double spacingY, double spacingZ)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "Resample: input image is null.";
        return r;
    }

    if (spacingX <= 0.0 || spacingY <= 0.0 || spacingZ <= 0.0)
    {
        r.diagnostic = "Resample: output spacing must be positive in all axes.";
        return r;
    }

    return CatchItk([&]() -> xq_ImageResult {
        auto itkIn = VtkImageToItkFloat3D(input);
        if (!itkIn)
        {
            xq_ImageResult fail;
            fail.diagnostic = "Resample: VtkImageToItkFloat3D conversion failed.";
            return fail;
        }

        using FilterType = itk::ResampleImageFilter<ItkFloat3D, ItkFloat3D>;
        auto filter = FilterType::New();
        filter->SetInput(itkIn);

        // Identity transform: output coordinates = input coordinates
        using TransformType = itk::IdentityTransform<double, 3>;
        auto transform = TransformType::New();
        filter->SetTransform(transform);

        using InterpolatorType = itk::LinearInterpolateImageFunction<ItkFloat3D, double>;
        auto interpolator = InterpolatorType::New();
        filter->SetInterpolator(interpolator);

        const double outSpacing[3] = {spacingX, spacingY, spacingZ};
        filter->SetOutputSpacing(outSpacing);
        filter->SetOutputOrigin(itkIn->GetOrigin());
        filter->SetOutputDirection(itkIn->GetDirection());

        // Compute output size
        const auto& inSize = itkIn->GetLargestPossibleRegion().GetSize();
        const auto& inSpacing = itkIn->GetSpacing();

        ItkFloat3D::SizeType outSize;
        outSize[0] = static_cast<unsigned long>(
            std::ceil(inSize[0] * inSpacing[0] / spacingX));
        outSize[1] = static_cast<unsigned long>(
            std::ceil(inSize[1] * inSpacing[1] / spacingY));
        outSize[2] = static_cast<unsigned long>(
            std::ceil(inSize[2] * inSpacing[2] / spacingZ));
        filter->SetSize(outSize);

        filter->Update();

        xq_ImageResult ok;
        ok.ok = true;
        ok.image = ItkFloat3DToVtkImage(filter->GetOutput());
        return ok;
    });
}

// ---------------------------------------------------------------------------
// MarchingCubes
// ---------------------------------------------------------------------------
xq_ImageResult
xq_ImageProcessingUtils::MarchingCubes(vtkImageData* input, double isovalue)
{
    xq_ImageResult r;

    if (!input)
    {
        r.diagnostic = "MarchingCubes: input image is null.";
        return r;
    }

    int dims[3];
    input->GetDimensions(dims);
    if (dims[0] < 2 || dims[1] < 2 || dims[2] < 2)
    {
        r.diagnostic = "MarchingCubes: image must be at least 2 voxels in each dimension.";
        return r;
    }

    try
    {
        vtkNew<vtkMarchingCubes> mc;
        mc->SetInputData(input);
        mc->SetValue(0, isovalue);
        mc->ComputeNormalsOn();
        mc->Update();

        auto output = mc->GetOutput();
        if (!output || output->GetNumberOfPoints() == 0)
        {
            r.diagnostic = "MarchingCubes: no surface generated at isovalue " +
                           std::to_string(isovalue) + ".";
            return r;
        }

        r.ok = true;
        r.surface = output;
        return r;
    }
    catch (const std::exception& e)
    {
        r.diagnostic = std::string("MarchingCubes: VTK exception: ") + e.what();
        return r;
    }
}
