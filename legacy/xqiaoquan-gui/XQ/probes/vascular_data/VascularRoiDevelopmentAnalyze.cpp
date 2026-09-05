#include "adapters/gdcm/GdcmItkDicomLabelReader.h"
#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkVascularPreprocessor.h"
#include "adapters/itk/ItkVascularRoiPriorReader.h"
#include "adapters/itk/ItkVascularSegmentationEvaluator.h"
#include "core/source/ResidentVoxelSource.h"
#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include "itkAndImageFilter.h"
#include "itkArrivalFunctionToPathFilter.h"
#include "itkBinaryBallStructuringElement.h"
#include "itkBinaryContourImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryReconstructionByDilationImageFilter.h"
#include "itkBinaryThinningImageFilter3D.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkFastMarchingUpwindGradientImageFilter.h"
#include "itkHessian3DToVesselnessMeasureImageFilter.h"
#include "itkImage.h"
#include "itkImportImageFilter.h"
#include "itkIterateNeighborhoodOptimizer.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkMaskImageFilter.h"
#include "itkMultiScaleHessianBasedMeasureImageFilter.h"
#include "itkOrImageFilter.h"
#include "itkPathIterator.h"
#include "itkPolyLineParametricPath.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkSpeedFunctionToPathFilter.h"
#include "itkThresholdSegmentationLevelSetImageFilter.h"

#if defined(XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY)
#include "core/image/IAutomaticVesselSegmenter.h"

#include "itkIdentityTransform.h"
#include "itkIntensityWindowingImageFilter.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkRecursiveGaussianImageFilter.h"
#include "itkResampleImageFilter.h"

#include "tubeConvertTubesToImage.h"
#include "tubeResampleImage.h"
#include "tubeSegmentTubes.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr unsigned int kDimension = 3;
using FloatImage = itk::Image<float, kDimension>;
using BinaryImage = itk::Image<unsigned char, kDimension>;
using LabelImage = itk::Image<std::uint32_t, kDimension>;
using FloatImporter = itk::ImportImageFilter<float, kDimension>;
using BinaryImporter = itk::ImportImageFilter<unsigned char, kDimension>;
using Threshold = itk::BinaryThresholdImageFilter<FloatImage, BinaryImage>;
using And = itk::AndImageFilter<BinaryImage, BinaryImage, BinaryImage>;
using Or = itk::OrImageFilter<BinaryImage, BinaryImage, BinaryImage>;
using FloatMask = itk::MaskImageFilter<FloatImage, BinaryImage, FloatImage>;
using Kernel = itk::BinaryBallStructuringElement<unsigned char, kDimension>;
using Contour = itk::BinaryContourImageFilter<BinaryImage, BinaryImage>;
using Dilate = itk::BinaryDilateImageFilter<BinaryImage, BinaryImage, Kernel>;
using Reconstruction =
    itk::BinaryReconstructionByDilationImageFilter<BinaryImage>;
using Connected = itk::ConnectedComponentImageFilter<BinaryImage, LabelImage>;
using Relabel = itk::RelabelComponentImageFilter<LabelImage, LabelImage>;
using Thinning = itk::BinaryThinningImageFilter3D<BinaryImage, BinaryImage>;
using Distance =
    itk::SignedMaurerDistanceMapImageFilter<BinaryImage, FloatImage>;
using MinimalPath = itk::PolyLineParametricPath<kDimension>;
using MinimalPathFilter =
    itk::SpeedFunctionToPathFilter<FloatImage, MinimalPath>;
using ArrivalPathFilter =
    itk::ArrivalFunctionToPathFilter<FloatImage, MinimalPath>;
using MinimalPathIterator = itk::PathIterator<BinaryImage, MinimalPath>;
using MinimalPathOptimizer = itk::IterateNeighborhoodOptimizer;
using ArrivalTimeMarcher =
    itk::FastMarchingUpwindGradientImageFilter<FloatImage, FloatImage>;
using ThresholdLevelSet =
    itk::ThresholdSegmentationLevelSetImageFilter<FloatImage, FloatImage>;
using Diffusion =
    itk::CurvatureAnisotropicDiffusionImageFilter<FloatImage, FloatImage>;
using SatoHessianPixel = itk::SymmetricSecondRankTensor<double, kDimension>;
using SatoHessianImage = itk::Image<SatoHessianPixel, kDimension>;
using SatoMeasure = itk::Hessian3DToVesselnessMeasureImageFilter<float>;
using SatoMultiScale = itk::MultiScaleHessianBasedMeasureImageFilter<
    FloatImage, SatoHessianImage, FloatImage>;

#if defined(XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY)
using TubeTkFloatLinearInterpolator =
    itk::LinearInterpolateImageFunction<FloatImage, double>;
using TubeTkBinaryNearestInterpolator =
    itk::NearestNeighborInterpolateImageFunction<BinaryImage, double>;
using TubeTkFloatMask =
    itk::MaskImageFilter<FloatImage, BinaryImage, FloatImage>;
using TubeTkWindow =
    itk::IntensityWindowingImageFilter<FloatImage, FloatImage>;
using TubeTkSegmenter = tube::SegmentTubes<FloatImage>;
using TubeTkRasterizer = tube::ConvertTubesToImage<BinaryImage>;
#if defined(XQ_DEVELOPMENT_TUBETK_ALL_ENDPOINTS)
constexpr const char* kTubeTkExperimentPrefix =
    "roi.schema4_experiment11";
#elif defined(XQ_DEVELOPMENT_TUBETK_UNCLIPPED_INPUT)
constexpr const char* kTubeTkExperimentPrefix =
    "roi.schema4_experiment10";
#else
constexpr const char* kTubeTkExperimentPrefix =
    "roi.schema4_experiment9";
#endif
#endif

struct QuickMetrics {
    std::size_t predicted = 0;
    std::size_t intersection = 0;
    double dice = 0.0;
    double precision = 0.0;
    double recall = 0.0;
};

struct AdditionPolicyResult {
    std::vector<unsigned char> bestDiceValues;
    std::vector<unsigned char> bestRecallValues;
    std::vector<unsigned char> fixedTopologyValues;
    std::string bestDiceName;
    std::string bestRecallName;
    std::string fixedTopologyName;
    QuickMetrics bestDiceMetrics;
    QuickMetrics bestRecallMetrics;
    QuickMetrics fixedTopologyMetrics;
};

struct SkeletonEndpointSet {
    BinaryImage::Pointer skeleton;
    BinaryImage::Pointer endpoints;
    FloatImage::Pointer endpointDistance;
    std::vector<BinaryImage::IndexType> endpointIndices;
    std::vector<std::array<double, 3>> endpointOutwardDirections;
    std::size_t skeletonVoxelCount = 0;
};

bool sameGeometry(const xq::ImageGeometry& left,
                  const xq::ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || left.spacing[axis] != right.spacing[axis]
            || left.origin[axis] != right.origin[axis]) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (left.direction[axis][column]
                != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

template <class TImage, class TImporter, class TValue>
typename TImage::Pointer importImage(const xq::ImageGeometry& geometry,
                                     TValue* values,
                                     std::size_t voxelCount)
{
    typename TImage::SizeType size;
    typename TImage::IndexType start;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        size[axis] = static_cast<typename TImage::SizeType::SizeValueType>(
            geometry.dimensions[axis]);
        start[axis] = 0;
    }
    typename TImage::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    typename TImporter::Pointer importer = TImporter::New();
    importer->SetRegion(region);
    typename TImage::SpacingType spacing;
    typename TImage::PointType origin;
    typename TImage::DirectionType direction;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        spacing[axis] = geometry.spacing[axis];
        origin[axis] = geometry.origin[axis];
        for (unsigned int column = 0; column < kDimension; ++column) {
            direction[axis][column] = geometry.direction[axis][column];
        }
    }
    importer->SetSpacing(spacing);
    importer->SetOrigin(origin);
    importer->SetDirection(direction);
    importer->SetImportPointer(values, voxelCount, false);
    importer->Update();
    typename TImage::Pointer output = importer->GetOutput();
    output->DisconnectPipeline();
    return output;
}

void printMetrics(const char* prefix,
                  const xq::VascularSegmentationEvaluationResult& evaluated)
{
    std::printf("%s.status=%s\n", prefix,
                xq::vascularSegmentationEvaluationStatusToken(evaluated.status));
    if (!evaluated.ok()) {
        return;
    }
    const xq::VascularSegmentationMetrics& metrics = *evaluated.metrics;
    const double precision = static_cast<double>(metrics.intersectionVoxelCount)
        / static_cast<double>(metrics.predictionForegroundVoxelCount);
    const double recall = static_cast<double>(metrics.intersectionVoxelCount)
        / static_cast<double>(metrics.referenceForegroundVoxelCount);
    std::printf(
        "%s.predicted=%zu,intersection=%zu,dice=%.9g,precision=%.9g,recall=%.9g,components=%zu,largest_component_fraction=%.9g,prediction_to_reference_p95_mm=%.9g,reference_to_prediction_p95_mm=%.9g,hd95_mm=%.9g,assd_mm=%.9g,cldice=%.9g\n",
        prefix, metrics.predictionForegroundVoxelCount,
        metrics.intersectionVoxelCount, metrics.dice, precision, recall,
        metrics.predictionComponentCount,
        metrics.largestPredictionComponentFraction,
        metrics.predictionToReferenceSurfaceP95Mm,
        metrics.referenceToPredictionSurfaceP95Mm, metrics.hd95Mm,
        metrics.assdMm, metrics.clDice);
}

xq::XQSegmentationMask materialize(const xq::ImageGeometry& geometry,
                                   const unsigned char* values,
                                   std::size_t voxelCount)
{
    xq::XQSegmentationMask mask(geometry.dimensions);
    mask.setGeometry(geometry);
    mask.setLabels({xq::SegmentationLabel{1, "vessel"}});
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (values[index] != 0) {
            mask.setLabelAt(index, 1);
        }
    }
    return mask;
}

BinaryImage::Pointer binaryAnd(BinaryImage* left, BinaryImage* right)
{
    And::Pointer filter = And::New();
    filter->SetInput1(left);
    filter->SetInput2(right);
    filter->Update();
    BinaryImage::Pointer output = filter->GetOutput();
    output->DisconnectPipeline();
    return output;
}

BinaryImage::Pointer binaryOr(BinaryImage* left, BinaryImage* right)
{
    Or::Pointer filter = Or::New();
    filter->SetInput1(left);
    filter->SetInput2(right);
    filter->Update();
    BinaryImage::Pointer output = filter->GetOutput();
    output->DisconnectPipeline();
    return output;
}

BinaryImage::Pointer threshold(FloatImage* input,
                               double lower,
                               double upper)
{
    Threshold::Pointer filter = Threshold::New();
    filter->SetInput(input);
    filter->SetLowerThreshold(static_cast<float>(lower));
    filter->SetUpperThreshold(static_cast<float>(upper));
    filter->SetInsideValue(1);
    filter->SetOutsideValue(0);
    filter->Update();
    BinaryImage::Pointer output = filter->GetOutput();
    output->DisconnectPipeline();
    return output;
}

FloatImage::Pointer computeSatoResponse(
    FloatImage* input,
    const xq::VascularPreprocessProfileV1& profile)
{
    Diffusion::Pointer diffusion = Diffusion::New();
    diffusion->SetInput(input);
    diffusion->SetNumberOfIterations(profile.diffusionIterations);
    diffusion->SetTimeStep(profile.diffusionTimeStep);
    diffusion->SetConductanceParameter(profile.diffusionConductance);
    diffusion->SetUseImageSpacing(true);
    diffusion->Update();

    SatoMeasure::Pointer sato = SatoMeasure::New();
    std::printf(
        "roi.sato.alpha1=%.9g,alpha2=%.9g,sigma_mm=%.9g..%.9g,steps=%u\n",
        sato->GetAlpha1(), sato->GetAlpha2(), profile.sigmaMinimumMm,
        profile.sigmaMaximumMm, profile.sigmaSteps);

    SatoMultiScale::Pointer multiScale = SatoMultiScale::New();
    multiScale->SetInput(diffusion->GetOutput());
    multiScale->SetHessianToMeasureFilter(sato);
    multiScale->SetSigmaMinimum(profile.sigmaMinimumMm);
    multiScale->SetSigmaMaximum(profile.sigmaMaximumMm);
    multiScale->SetNumberOfSigmaSteps(profile.sigmaSteps);
    multiScale->SetNonNegativeHessianBasedMeasure(true);
    if (profile.sigmaStepMethod == xq::VascularSigmaStepMethod::Logarithmic) {
        multiScale->SetSigmaStepMethodToLogarithmic();
    } else {
        multiScale->SetSigmaStepMethodToEquispaced();
    }
    multiScale->Update();

    FloatImage::Pointer output = multiScale->GetOutput();
    output->DisconnectPipeline();
    return output;
}

BinaryImage::Pointer dilatePhysical(BinaryImage* input,
                                    const xq::ImageGeometry& geometry,
                                    double radiusMm)
{
    Kernel::SizeType radius;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        radius[axis] = static_cast<unsigned long>(std::ceil(
            radiusMm / geometry.spacing[axis]));
    }
    Kernel kernel;
    kernel.SetRadius(radius);
    kernel.CreateStructuringElement();
    Dilate::Pointer filter = Dilate::New();
    filter->SetInput(input);
    filter->SetKernel(kernel);
    filter->SetDilateValue(1);
    filter->Update();
    BinaryImage::Pointer output = filter->GetOutput();
    output->DisconnectPipeline();
    return output;
}

BinaryImage::Pointer reconstruct(BinaryImage* marker, BinaryImage* mask)
{
    Reconstruction::Pointer filter = Reconstruction::New();
    filter->SetMarkerImage(marker);
    filter->SetMaskImage(mask);
    filter->SetForegroundValue(1);
    filter->SetFullyConnected(true);
    filter->Update();
    BinaryImage::Pointer output = filter->GetOutput();
    output->DisconnectPipeline();
    return output;
}

#if defined(XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY)
template <typename TImage, typename TInterpolator>
typename TImage::Pointer resampleToReference(const TImage* input,
                                             const TImage* reference,
                                             TInterpolator* interpolator)
{
    if (input == nullptr || reference == nullptr || interpolator == nullptr) {
        return nullptr;
    }
    using ResampleFilter = itk::ResampleImageFilter<TImage, TImage>;
    using IdentityTransform = itk::IdentityTransform<double, kDimension>;
    typename ResampleFilter::Pointer resampler = ResampleFilter::New();
    typename IdentityTransform::Pointer identity = IdentityTransform::New();
    identity->SetIdentity();
    resampler->SetInput(input);
    resampler->SetReferenceImage(reference);
    resampler->UseReferenceImageOn();
    resampler->SetTransform(identity);
    resampler->SetInterpolator(interpolator);
    resampler->SetDefaultPixelValue(
        static_cast<typename TImage::PixelType>(0));
    resampler->Update();
    typename TImage::Pointer output = resampler->GetOutput();
    output->DisconnectPipeline();
    return output;
}

FloatImage::Pointer blurRecursiveGaussian(FloatImage* input, double sigmaMm)
{
    if (input == nullptr) {
        return nullptr;
    }
    FloatImage::Pointer current = input;
    for (unsigned int direction = 0; direction < kDimension; ++direction) {
        using GaussianFilter =
            itk::RecursiveGaussianImageFilter<FloatImage, FloatImage>;
        GaussianFilter::Pointer gaussian = GaussianFilter::New();
        gaussian->SetInput(current);
        gaussian->SetNormalizeAcrossScale(true);
        gaussian->SetSigma(sigmaMm);
        gaussian->SetOrder(itk::GaussianOrderEnum::ZeroOrder);
        gaussian->SetDirection(direction);
        gaussian->Update();
        current = gaussian->GetOutput();
        current->DisconnectPipeline();
    }
    return current;
}

std::size_t countForeground(BinaryImage* image)
{
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        return 0;
    }
    const std::size_t voxelCount = static_cast<std::size_t>(
        image->GetLargestPossibleRegion().GetNumberOfPixels());
    const unsigned char* values = image->GetBufferPointer();
    std::size_t count = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (values[index] != 0) {
            ++count;
        }
    }
    return count;
}
#endif

struct LevelSetProbeResult {
    std::vector<unsigned char> values;
    std::size_t componentCount = 0;
};

LevelSetProbeResult runThresholdLevelSet(
    const char* prefix,
    BinaryImage* initialSurface,
    FloatImage* featureImage,
    BinaryImage* reconstructionMarker,
    float lowerThreshold,
    float upperThreshold,
    unsigned int iterations,
    double curvatureScaling,
    std::size_t voxelCount)
{
    LevelSetProbeResult result;
    Distance::Pointer initial = Distance::New();
    initial->SetInput(initialSurface);
    initial->SetBackgroundValue(0);
    initial->SetInsideIsPositive(false);
    initial->SetUseImageSpacing(true);
    initial->SetSquaredDistance(false);

    ThresholdLevelSet::Pointer filter = ThresholdLevelSet::New();
    filter->SetInput(initial->GetOutput());
    filter->SetFeatureImage(featureImage);
    filter->SetIsoSurfaceValue(0.0);
    filter->SetLowerThreshold(lowerThreshold);
    filter->SetUpperThreshold(upperThreshold);
    filter->SetPropagationScaling(1.0);
    filter->SetCurvatureScaling(curvatureScaling);
    filter->SetEdgeWeight(0.0);
    filter->SetSmoothingIterations(0);
    filter->SetMaximumRMSError(0.001);
    filter->SetNumberOfIterations(iterations);
    filter->UseImageSpacingOn();
    filter->Update();

    BinaryImage::Pointer evolvedInside = threshold(
        filter->GetOutput(), (std::numeric_limits<float>::lowest)(), 0.0);
    BinaryImage::Pointer connectedDomain = binaryOr(
        evolvedInside, reconstructionMarker);
    BinaryImage::Pointer connectedResult = reconstruct(
        reconstructionMarker, connectedDomain);
    const unsigned char* outputValues =
        connectedResult->GetBufferPointer();
    result.values.assign(outputValues, outputValues + voxelCount);

    Connected::Pointer connected = Connected::New();
    connected->SetInput(connectedResult);
    connected->SetFullyConnected(true);
    Relabel::Pointer relabel = Relabel::New();
    relabel->SetInput(connected->GetOutput());
    relabel->Update();
    result.componentCount = relabel->GetNumberOfObjects();
    std::printf(
        "%s.lower_vesselness=%.9g,ct_hu=80..300,iterations=%u,elapsed_iterations=%llu,rms=%.9g,curvature=%.9g,edge_weight=0,smoothing_iterations=0,final_components=%zu\n",
        prefix, static_cast<double>(lowerThreshold), iterations,
        static_cast<unsigned long long>(filter->GetElapsedIterations()),
        filter->GetRMSChange(), curvatureScaling, result.componentCount);
    return result;
}

SkeletonEndpointSet extractSkeletonEndpoints(
    BinaryImage* input,
    const xq::ImageGeometry& geometry,
    std::size_t voxelCount)
{
    SkeletonEndpointSet result;
    Thinning::Pointer thinning = Thinning::New();
    thinning->SetInput(input);
    thinning->Update();
    result.skeleton = thinning->GetOutput();
    result.skeleton->DisconnectPipeline();

    const unsigned char* skeletonValues =
        result.skeleton->GetBufferPointer();
    std::vector<unsigned char> endpointValues(voxelCount, 0);
    const int dimX = geometry.dimensions[0];
    const int dimY = geometry.dimensions[1];
    const int dimZ = geometry.dimensions[2];
    for (int z = 0; z < dimZ; ++z) {
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x) {
                const std::size_t flat = static_cast<std::size_t>(x)
                    + static_cast<std::size_t>(dimX)
                        * (static_cast<std::size_t>(y)
                           + static_cast<std::size_t>(dimY)
                               * static_cast<std::size_t>(z));
                if (skeletonValues[flat] == 0) {
                    continue;
                }
                ++result.skeletonVoxelCount;
                int neighbors = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0 && dz == 0) {
                                continue;
                            }
                            const int nx = x + dx;
                            const int ny = y + dy;
                            const int nz = z + dz;
                            if (nx < 0 || ny < 0 || nz < 0
                                || nx >= dimX || ny >= dimY || nz >= dimZ) {
                                continue;
                            }
                            const std::size_t neighborFlat =
                                static_cast<std::size_t>(nx)
                                + static_cast<std::size_t>(dimX)
                                    * (static_cast<std::size_t>(ny)
                                       + static_cast<std::size_t>(dimY)
                                           * static_cast<std::size_t>(nz));
                            if (skeletonValues[neighborFlat] != 0) {
                                ++neighbors;
                            }
                        }
                    }
                }
                if (neighbors == 1) {
                    endpointValues[flat] = 1;
                    BinaryImage::IndexType endpoint;
                    endpoint[0] = x;
                    endpoint[1] = y;
                    endpoint[2] = z;
                    result.endpointIndices.push_back(endpoint);
                }
            }
        }
    }

    constexpr double tangentNeighborhoodRadiusMm = 5.0;
    for (const BinaryImage::IndexType& endpoint : result.endpointIndices) {
        BinaryImage::PointType endpointPoint;
        result.skeleton->TransformIndexToPhysicalPoint(endpoint,
                                                       endpointPoint);
        std::array<double, 3> centroid = {0.0, 0.0, 0.0};
        std::size_t centroidCount = 0;
        int radius[3] = {};
        for (int axis = 0; axis < 3; ++axis) {
            radius[axis] = static_cast<int>(std::ceil(
                tangentNeighborhoodRadiusMm / geometry.spacing[axis]));
        }
        for (int z = (std::max)(0, static_cast<int>(endpoint[2]) - radius[2]);
             z <= (std::min)(dimZ - 1,
                             static_cast<int>(endpoint[2]) + radius[2]);
             ++z) {
            for (int y = (std::max)(
                     0, static_cast<int>(endpoint[1]) - radius[1]);
                 y <= (std::min)(dimY - 1,
                                 static_cast<int>(endpoint[1]) + radius[1]);
                 ++y) {
                for (int x = (std::max)(
                         0, static_cast<int>(endpoint[0]) - radius[0]);
                     x <= (std::min)(dimX - 1,
                                     static_cast<int>(endpoint[0]) + radius[0]);
                     ++x) {
                    BinaryImage::IndexType index;
                    index[0] = x;
                    index[1] = y;
                    index[2] = z;
                    if (result.skeleton->GetPixel(index) == 0
                        || index == endpoint) {
                        continue;
                    }
                    BinaryImage::PointType point;
                    result.skeleton->TransformIndexToPhysicalPoint(index,
                                                                   point);
                    double squaredDistance = 0.0;
                    for (int axis = 0; axis < 3; ++axis) {
                        const double delta = point[axis]
                            - endpointPoint[axis];
                        squaredDistance += delta * delta;
                    }
                    if (squaredDistance
                        > tangentNeighborhoodRadiusMm
                            * tangentNeighborhoodRadiusMm) {
                        continue;
                    }
                    for (int axis = 0; axis < 3; ++axis) {
                        centroid[axis] += point[axis];
                    }
                    ++centroidCount;
                }
            }
        }
        std::array<double, 3> outward = {0.0, 0.0, 0.0};
        if (centroidCount != 0) {
            double norm = 0.0;
            for (int axis = 0; axis < 3; ++axis) {
                centroid[axis] /= static_cast<double>(centroidCount);
                outward[axis] = endpointPoint[axis] - centroid[axis];
                norm += outward[axis] * outward[axis];
            }
            norm = std::sqrt(norm);
            if (norm > 0.0) {
                for (double& value : outward) {
                    value /= norm;
                }
            }
        }
        result.endpointOutwardDirections.push_back(outward);
    }

    result.endpoints = BinaryImage::New();
    result.endpoints->CopyInformation(result.skeleton);
    result.endpoints->SetRegions(result.skeleton->GetLargestPossibleRegion());
    result.endpoints->Allocate();
    std::copy(endpointValues.begin(), endpointValues.end(),
              result.endpoints->GetBufferPointer());

    Distance::Pointer endpointDistance = Distance::New();
    endpointDistance->SetInput(result.endpoints);
    endpointDistance->SetUseImageSpacing(true);
    endpointDistance->SetSquaredDistance(false);
    endpointDistance->SetInsideIsPositive(false);
    endpointDistance->Update();
    result.endpointDistance = endpointDistance->GetOutput();
    result.endpointDistance->DisconnectPipeline();
    return result;
}

#if defined(XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY)
struct TubeTkEndpointRecoveryResult {
    bool ok = false;
    std::string status = "not_run";
    std::vector<unsigned char> values;
    std::size_t clippedSeedCount = 0;
    std::size_t extractedTubeCount = 0;
    std::size_t extractedTubePointCount = 0;
    std::size_t rasterizedVoxelCount = 0;
    std::size_t clippedTubeVoxelCount = 0;
    std::size_t addedVoxelCount = 0;
    std::size_t finalComponentCount = 0;
};

TubeTkEndpointRecoveryResult runTubeTkEndpointRecovery(
    FloatImage* ct,
    FloatImage* vesselness,
    BinaryImage* organ,
    BinaryImage* coarse,
    BinaryImage* roiDomain,
    BinaryImage* bridgeOnly,
    const SkeletonEndpointSet& bridgeEndpoints,
    std::size_t voxelCount)
{
    TubeTkEndpointRecoveryResult result;
    const auto fail = [&result](const char* status) {
        result.status = status;
        return result;
    };

    try {
        const xq::AutomaticVesselSegmentationProfileV2 profile;
        if (!profile.makeHighResolutionIsotropic
            || profile.vesselnessMaskMinimum != 0.0
            || profile.vesselnessMaskMaximum != 1000.0
            || profile.inputBlurSigmaMm != 0.4
            || profile.inputWindowMinimum != 0.5
            || profile.inputWindowMaximum != 300.0
            || profile.inputWindowOutputMinimum != 0.0
            || profile.inputWindowOutputMaximum != 300.0
            || profile.minimumCurvature != 0.0
            || profile.minimumRoundness != 0.02
            || profile.minimumRidgeness != 0.5
            || profile.minimumLevelness != 0.0
            || profile.radiusInObjectSpaceMm != 0.8
            || profile.borderInIndexSpace != 3
            || !profile.optimizeRadius
            || !profile.rasterizeWithRadius) {
            return fail("legacy_profile_drift");
        }

        using HighResolutionResampler = tube::ResampleImage<FloatImage>;
        HighResolutionResampler::Pointer highResolution =
            HighResolutionResampler::New();
        highResolution->SetInput(ct);
        highResolution->SetMakeHighResIso(
            profile.makeHighResolutionIsotropic);
        highResolution->SetInterpolator("Linear");
        highResolution->Update();
        FloatImage::Pointer workingInput = highResolution->GetOutput();
        if (workingInput == nullptr) {
            return fail("working_ct_resample_failed");
        }
        workingInput->DisconnectPipeline();

        TubeTkFloatLinearInterpolator::Pointer linearInterpolator =
            TubeTkFloatLinearInterpolator::New();
        TubeTkBinaryNearestInterpolator::Pointer nearestInterpolator =
            TubeTkBinaryNearestInterpolator::New();
        FloatImage::Pointer workingVesselness = resampleToReference(
            vesselness, workingInput.GetPointer(),
            linearInterpolator.GetPointer());

        using BinaryWorkingResampler = tube::ResampleImage<BinaryImage>;
        BinaryWorkingResampler::Pointer highResolutionOrgan =
            BinaryWorkingResampler::New();
        highResolutionOrgan->SetInput(organ);
        highResolutionOrgan->SetMakeHighResIso(
            profile.makeHighResolutionIsotropic);
        highResolutionOrgan->SetInterpolator("NearestNeighbor");
        highResolutionOrgan->Update();
        BinaryImage::Pointer workingOrgan = highResolutionOrgan->GetOutput();
        if (workingOrgan == nullptr) {
            return fail("working_organ_resample_failed");
        }
        workingOrgan->DisconnectPipeline();
        BinaryImage::Pointer workingCoarse = resampleToReference(
            coarse, workingOrgan.GetPointer(),
            nearestInterpolator.GetPointer());
        if (workingVesselness == nullptr || workingCoarse == nullptr
            || workingInput->GetLargestPossibleRegion().GetSize()
                != workingVesselness->GetLargestPossibleRegion().GetSize()
            || workingInput->GetLargestPossibleRegion().GetSize()
                != workingOrgan->GetLargestPossibleRegion().GetSize()
            || workingInput->GetLargestPossibleRegion().GetSize()
                != workingCoarse->GetLargestPossibleRegion().GetSize()) {
            return fail("working_grid_mismatch");
        }

        BinaryImage::Pointer vesselnessRangeMask = threshold(
            workingVesselness, profile.vesselnessMaskMinimum,
            profile.vesselnessMaskMaximum);
#if defined(XQ_DEVELOPMENT_TUBETK_UNCLIPPED_INPUT)
        BinaryImage::Pointer preparedInputMask = vesselnessRangeMask;
#else
        BinaryImage::Pointer domainMask = binaryOr(
            workingOrgan, workingCoarse);
        BinaryImage::Pointer preparedInputMask = binaryAnd(
            domainMask, vesselnessRangeMask);
#endif
        if (countForeground(preparedInputMask) == 0) {
            return fail("empty_prepared_input_domain");
        }

        TubeTkFloatMask::Pointer maskInput = TubeTkFloatMask::New();
        maskInput->SetInput(workingInput);
        maskInput->SetMaskImage(preparedInputMask);
        maskInput->SetMaskingValue(0);
        maskInput->SetOutsideValue(0.0f);
        maskInput->Update();
        FloatImage::Pointer maskedInput = maskInput->GetOutput();
        maskedInput->DisconnectPipeline();

        FloatImage::Pointer blurredInput = blurRecursiveGaussian(
            maskedInput.GetPointer(), profile.inputBlurSigmaMm);
        TubeTkWindow::Pointer window = TubeTkWindow::New();
        window->SetInput(blurredInput);
        window->SetWindowMinimum(profile.inputWindowMinimum);
        window->SetWindowMaximum(profile.inputWindowMaximum);
        window->SetOutputMinimum(profile.inputWindowOutputMinimum);
        window->SetOutputMaximum(profile.inputWindowOutputMaximum);
        window->Update();
        FloatImage::Pointer tubeInput = window->GetOutput();
        tubeInput->DisconnectPipeline();

        BinaryImage::Pointer clippedEndpointMask;
#if defined(XQ_DEVELOPMENT_TUBETK_ALL_ENDPOINTS)
        clippedEndpointMask = bridgeEndpoints.endpoints;
#else
        clippedEndpointMask = binaryAnd(
            bridgeEndpoints.endpoints, roiDomain);
#endif
        TubeTkSegmenter::PointListType seedPoints;
        seedPoints.reserve(bridgeEndpoints.endpointIndices.size());
        for (const BinaryImage::IndexType& endpoint :
             bridgeEndpoints.endpointIndices) {
            if (clippedEndpointMask->GetPixel(endpoint) == 0) {
                continue;
            }
            FloatImage::PointType physicalPoint;
            bridgeOnly->TransformIndexToPhysicalPoint(endpoint,
                                                       physicalPoint);
            itk::ContinuousIndex<double, kDimension> workingIndex;
            if (!tubeInput->TransformPhysicalPointToContinuousIndex(
                    physicalPoint, workingIndex)) {
                continue;
            }
            TubeTkSegmenter::PointType seedPoint;
            for (unsigned int axis = 0; axis < kDimension; ++axis) {
                seedPoint[axis] = physicalPoint[axis];
            }
            seedPoints.push_back(seedPoint);
        }
        result.clippedSeedCount = seedPoints.size();
        if (seedPoints.empty()) {
            return fail("empty_roi_clipped_endpoint_seed_list");
        }

        TubeTkSegmenter::Pointer segmentTubes = TubeTkSegmenter::New();
        segmentTubes->SetInput(tubeInput);
        segmentTubes->SetMinCurvature(profile.minimumCurvature);
        segmentTubes->SetMinRoundness(profile.minimumRoundness);
        segmentTubes->SetMinRidgeness(profile.minimumRidgeness);
        segmentTubes->SetMinLevelness(profile.minimumLevelness);
        segmentTubes->SetRadiusInObjectSpace(profile.radiusInObjectSpaceMm);
        segmentTubes->SetBorderInIndexSpace(
            static_cast<int>(profile.borderInIndexSpace));
        segmentTubes->SetOptimizeRadius(profile.optimizeRadius);
        segmentTubes->SetSeedsInObjectSpaceList(seedPoints);
        segmentTubes->ProcessSeeds();

        TubeTkSegmenter::TubeGroupType::Pointer tubeGroup =
            segmentTubes->GetTubeGroup();
        if (tubeGroup == nullptr) {
            return fail("tube_group_missing");
        }
        char tubeName[] = "Tube";
        using TubeChildrenList =
            TubeTkSegmenter::TubeGroupType::ChildrenListType;
        std::unique_ptr<TubeChildrenList> tubeChildren(
            tubeGroup->GetChildren(tubeGroup->GetMaximumDepth(), tubeName));
        if (tubeChildren == nullptr || tubeChildren->empty()) {
            return fail("no_ridges_extracted");
        }
        result.extractedTubeCount = tubeChildren->size();
        for (const auto& child : *tubeChildren) {
            auto* tubeObject = dynamic_cast<TubeTkSegmenter::TubeType*>(
                child.GetPointer());
            if (tubeObject == nullptr) {
                return fail("unexpected_tube_object_type");
            }
            result.extractedTubePointCount +=
                tubeObject->GetPoints().size();
        }
        if (result.extractedTubePointCount == 0) {
            return fail("empty_extracted_ridges");
        }

        TubeTkRasterizer::Pointer rasterizer = TubeTkRasterizer::New();
        rasterizer->SetUseRadius(profile.rasterizeWithRadius);
        rasterizer->SetTemplateImage(workingCoarse);
        rasterizer->SetInput(tubeGroup);
        rasterizer->Update();
        BinaryImage::Pointer rasterizedTubes = rasterizer->GetOutput();
        if (rasterizedTubes == nullptr) {
            return fail("tube_rasterization_failed");
        }
        rasterizedTubes->DisconnectPipeline();
        result.rasterizedVoxelCount = countForeground(rasterizedTubes);
        if (result.rasterizedVoxelCount == 0) {
            return fail("empty_rasterized_tubes");
        }

        BinaryImage::Pointer backMapped = resampleToReference(
            rasterizedTubes.GetPointer(), coarse,
            nearestInterpolator.GetPointer());
        if (backMapped == nullptr
            || static_cast<std::size_t>(
                   backMapped->GetLargestPossibleRegion().GetNumberOfPixels())
                != voxelCount) {
            return fail("tube_back_map_failed");
        }
        BinaryImage::Pointer clippedTubes;
#if defined(XQ_DEVELOPMENT_TUBETK_UNCLIPPED_INPUT)
        clippedTubes = backMapped;
#else
        clippedTubes = binaryAnd(backMapped, roiDomain);
#endif
        result.clippedTubeVoxelCount = countForeground(clippedTubes);
        BinaryImage::Pointer merged = binaryOr(bridgeOnly, clippedTubes);
        const unsigned char* bridgeValues = bridgeOnly->GetBufferPointer();
        const unsigned char* mergedValues = merged->GetBufferPointer();
        if (bridgeValues == nullptr || mergedValues == nullptr) {
            return fail("merged_buffer_missing");
        }
        result.values.assign(mergedValues, mergedValues + voxelCount);
        for (std::size_t index = 0; index < voxelCount; ++index) {
            if (bridgeValues[index] == 0 && mergedValues[index] != 0) {
                ++result.addedVoxelCount;
            }
        }

        Connected::Pointer connected = Connected::New();
        connected->SetInput(merged);
        connected->SetFullyConnected(true);
        Relabel::Pointer relabel = Relabel::New();
        relabel->SetInput(connected->GetOutput());
        relabel->SetSortByObjectSize(true);
        relabel->Update();
        result.finalComponentCount = relabel->GetNumberOfObjects();
        result.ok = true;
        result.status = "ok";
        return result;
    } catch (const itk::ExceptionObject&) {
        return fail("itk_exception");
    } catch (const std::exception&) {
        return fail("standard_exception");
    } catch (const char*) {
        return fail("upstream_string_exception");
    } catch (...) {
        return fail("unknown_exception");
    }
}
#endif

QuickMetrics quickMetrics(
    const unsigned char* values,
    std::size_t voxelCount,
    const std::vector<xq::XQSegmentationMask::LabelType>& reference,
    std::size_t referenceForeground)
{
    QuickMetrics metrics;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (values[index] == 0) {
            continue;
        }
        ++metrics.predicted;
        if (reference[index] != 0) {
            ++metrics.intersection;
        }
    }
    metrics.precision = metrics.predicted > 0
        ? static_cast<double>(metrics.intersection)
            / static_cast<double>(metrics.predicted)
        : 0.0;
    metrics.recall = referenceForeground > 0
        ? static_cast<double>(metrics.intersection)
            / static_cast<double>(referenceForeground)
        : 0.0;
    metrics.dice = metrics.predicted + referenceForeground > 0
        ? 2.0 * static_cast<double>(metrics.intersection)
            / static_cast<double>(metrics.predicted + referenceForeground)
        : 0.0;
    return metrics;
}

AdditionPolicyResult analyzeAdditionComponents(
    const char* prefix,
    BinaryImage* reconstructed,
    BinaryImage* baseline,
    BinaryImage* primaryPrior,
    BinaryImage* secondaryPrior,
    FloatImage* vesselness,
    FloatImage* endpointDistance,
    const std::vector<xq::XQSegmentationMask::LabelType>& reference,
    std::size_t referenceForeground,
    const xq::ImageGeometry& geometry,
    std::size_t voxelCount,
    std::size_t maximumReportedComponents)
{
    AdditionPolicyResult result;
    const QuickMetrics baselineMetrics = quickMetrics(
        baseline->GetBufferPointer(), voxelCount, reference,
        referenceForeground);
    std::vector<unsigned char> additionValues(voxelCount, 0);
    const unsigned char* reconstructedValues =
        reconstructed->GetBufferPointer();
    const unsigned char* baselineValues = baseline->GetBufferPointer();
    std::size_t additionVoxelCount = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (reconstructedValues[index] != 0 && baselineValues[index] == 0) {
            additionValues[index] = 1;
            ++additionVoxelCount;
        }
    }
    BinaryImage::Pointer additions = importImage<BinaryImage, BinaryImporter>(
        geometry, additionValues.data(), voxelCount);
    Connected::Pointer connected = Connected::New();
    connected->SetInput(additions);
    connected->SetFullyConnected(true);
    Relabel::Pointer relabel = Relabel::New();
    relabel->SetInput(connected->GetOutput());
    relabel->Update();
    LabelImage::Pointer labels = relabel->GetOutput();
    labels->DisconnectPipeline();

    BinaryImage::Pointer baselineNeighborhood = dilatePhysical(
        baseline, geometry, 0.01);
    struct ComponentAudit {
        std::uint32_t label = 0;
        std::size_t voxels = 0;
        std::size_t referenceVoxels = 0;
        std::size_t primaryPriorVoxels = 0;
        std::size_t secondaryPriorVoxels = 0;
        std::size_t baselineContactVoxels = 0;
        double vesselnessSum = 0.0;
        double vesselnessMaximum = 0.0;
        double minimumEndpointDistanceMm =
            (std::numeric_limits<double>::infinity)();
        int minimumIndex[3] = {
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)()
        };
        int maximumIndex[3] = {
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)()
        };
    };
    const std::size_t componentCount = relabel->GetNumberOfObjects();
    std::vector<ComponentAudit> components(componentCount + 1);
    for (std::size_t label = 1; label <= componentCount; ++label) {
        components[label].label = static_cast<std::uint32_t>(label);
    }
    const std::uint32_t* labelValues = labels->GetBufferPointer();
    const unsigned char* primaryValues = primaryPrior->GetBufferPointer();
    const unsigned char* secondaryValues = secondaryPrior->GetBufferPointer();
    const unsigned char* contactValues =
        baselineNeighborhood->GetBufferPointer();
    const float* vesselnessValues = vesselness->GetBufferPointer();
    const float* endpointDistanceValues =
        endpointDistance->GetBufferPointer();
    std::size_t flat = 0;
    for (int z = 0; z < geometry.dimensions[2]; ++z) {
        for (int y = 0; y < geometry.dimensions[1]; ++y) {
            for (int x = 0; x < geometry.dimensions[0]; ++x, ++flat) {
                const std::uint32_t label = labelValues[flat];
                if (label == 0 || label > componentCount) {
                    continue;
                }
                ComponentAudit& component = components[label];
                ++component.voxels;
                if (reference[flat] != 0) {
                    ++component.referenceVoxels;
                }
                if (primaryValues[flat] != 0) {
                    ++component.primaryPriorVoxels;
                }
                if (secondaryValues[flat] != 0) {
                    ++component.secondaryPriorVoxels;
                }
                if (contactValues[flat] != 0) {
                    ++component.baselineContactVoxels;
                    component.minimumEndpointDistanceMm = (std::min)(
                        component.minimumEndpointDistanceMm,
                        std::abs(static_cast<double>(
                            endpointDistanceValues[flat])));
                }
                const double response = vesselnessValues[flat];
                component.vesselnessSum += response;
                component.vesselnessMaximum = (std::max)(
                    component.vesselnessMaximum, response);
                const int index[3] = {x, y, z};
                for (int axis = 0; axis < 3; ++axis) {
                    component.minimumIndex[axis] = (std::min)(
                        component.minimumIndex[axis], index[axis]);
                    component.maximumIndex[axis] = (std::max)(
                        component.maximumIndex[axis], index[axis]);
                }
            }
        }
    }

    std::vector<std::size_t> order;
    order.reserve(componentCount);
    for (std::size_t label = 1; label <= componentCount; ++label) {
        if (components[label].voxels != 0) {
            order.push_back(label);
        }
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t left, std::size_t right) {
                  if (components[left].voxels != components[right].voxels) {
                      return components[left].voxels
                          > components[right].voxels;
                  }
                  return left < right;
              });
    std::printf("%s.component_count=%zu,addition_voxels=%zu\n",
                prefix, componentCount, additionVoxelCount);
    const std::size_t reportCount = (std::min)(maximumReportedComponents,
                                                order.size());
    for (std::size_t rank = 0; rank < reportCount; ++rank) {
        const ComponentAudit& component = components[order[rank]];
        double extents[3] = {};
        for (int axis = 0; axis < 3; ++axis) {
            extents[axis] = static_cast<double>(
                component.maximumIndex[axis] - component.minimumIndex[axis] + 1)
                * geometry.spacing[axis];
        }
        double sortedExtents[3] = {extents[0], extents[1], extents[2]};
        std::sort(sortedExtents, sortedExtents + 3, std::greater<double>());
        const double inverseVoxels = 1.0
            / static_cast<double>(component.voxels);
        std::printf(
            "%s.c%zu.label=%u,voxels=%zu,reference_voxels=%zu,reference_precision=%.9g,primary_prior_fraction=%.9g,secondary_prior_fraction=%.9g,baseline_contact_voxels=%zu,minimum_endpoint_distance_mm=%.9g,mean_vesselness=%.9g,max_vesselness=%.9g,extent_mm=%.9gx%.9gx%.9g,elongation=%.9g\n",
            prefix, rank + 1, component.label, component.voxels,
            component.referenceVoxels,
            static_cast<double>(component.referenceVoxels) * inverseVoxels,
            static_cast<double>(component.primaryPriorVoxels) * inverseVoxels,
            static_cast<double>(component.secondaryPriorVoxels)
                * inverseVoxels,
            component.baselineContactVoxels,
            component.minimumEndpointDistanceMm,
            component.vesselnessSum * inverseVoxels,
            component.vesselnessMaximum, extents[0], extents[1], extents[2],
            sortedExtents[0] / (std::max)(sortedExtents[1], 1e-12));
    }

    struct Policy {
        std::size_t minimumVoxels = 0;
        std::size_t maximumVoxels = 0;
        double maximumContactFraction = 0.0;
        double maximumEndpointDistanceMm = 0.0;
        double maximumSecondaryPriorFraction = 0.0;
    };
    const Policy fixedTopologyPolicy{30, 700, 0.40, 12.0, 0.25};
    const std::size_t minimumVoxelsValues[] = {10, 30};
    const std::size_t maximumVoxelsValues[] = {400, 700, 1500};
    const double maximumContactFractions[] = {0.25, 0.40};
    const double maximumEndpointDistancesMm[] = {2.0, 4.0, 8.0, 12.0};
    const double maximumSecondaryPriorFractions[] = {0.25, 0.60};
    Policy bestDicePolicy;
    Policy bestRecallPolicy;
    double bestDice = -1.0;
    double bestRecall = -1.0;

    const auto retainedByPolicy = [](const ComponentAudit& component,
                                     const Policy& policy) {
        if (component.voxels < policy.minimumVoxels
            || component.voxels > policy.maximumVoxels
            || component.baselineContactVoxels == 0) {
            return false;
        }
        const double inverseVoxels = 1.0
            / static_cast<double>(component.voxels);
        const double contactFraction =
            static_cast<double>(component.baselineContactVoxels)
            * inverseVoxels;
        const double secondaryFraction =
            static_cast<double>(component.secondaryPriorVoxels)
            * inverseVoxels;
        return contactFraction <= policy.maximumContactFraction
            && component.minimumEndpointDistanceMm
                <= policy.maximumEndpointDistanceMm
            && secondaryFraction
                <= policy.maximumSecondaryPriorFraction;
    };

    for (std::size_t minimumVoxels : minimumVoxelsValues) {
        for (std::size_t maximumVoxels : maximumVoxelsValues) {
            for (double maximumContactFraction : maximumContactFractions) {
                for (double maximumEndpointDistanceMm
                     : maximumEndpointDistancesMm) {
                    for (double maximumSecondaryPriorFraction
                         : maximumSecondaryPriorFractions) {
                        Policy policy;
                        policy.minimumVoxels = minimumVoxels;
                        policy.maximumVoxels = maximumVoxels;
                        policy.maximumContactFraction =
                            maximumContactFraction;
                        policy.maximumEndpointDistanceMm =
                            maximumEndpointDistanceMm;
                        policy.maximumSecondaryPriorFraction =
                            maximumSecondaryPriorFraction;
                        QuickMetrics metrics = baselineMetrics;
                        for (std::size_t label = 1;
                             label <= componentCount; ++label) {
                            const ComponentAudit& component =
                                components[label];
                            if (!retainedByPolicy(component, policy)) {
                                continue;
                            }
                            metrics.predicted += component.voxels;
                            metrics.intersection +=
                                component.referenceVoxels;
                        }
                        metrics.precision = metrics.predicted > 0
                            ? static_cast<double>(metrics.intersection)
                                / static_cast<double>(metrics.predicted)
                            : 0.0;
                        metrics.recall = referenceForeground > 0
                            ? static_cast<double>(metrics.intersection)
                                / static_cast<double>(referenceForeground)
                            : 0.0;
                        metrics.dice = metrics.predicted + referenceForeground
                                > 0
                            ? 2.0 * static_cast<double>(metrics.intersection)
                                / static_cast<double>(metrics.predicted
                                                      + referenceForeground)
                            : 0.0;
                        char name[192] = {};
                        std::snprintf(
                            name, sizeof(name),
                            "%s.policy.min%zu.max%zu.contact%.9g.endpoint%.9g.secondary%.9g",
                            prefix, minimumVoxels, maximumVoxels,
                            maximumContactFraction,
                            maximumEndpointDistanceMm,
                            maximumSecondaryPriorFraction);
                        if (metrics.dice > bestDice) {
                            bestDice = metrics.dice;
                            bestDicePolicy = policy;
                            result.bestDiceName = name;
                            result.bestDiceMetrics = metrics;
                        }
                        if (metrics.dice >= 0.76
                            && metrics.recall > bestRecall) {
                            bestRecall = metrics.recall;
                            bestRecallPolicy = policy;
                            result.bestRecallName = name;
                            result.bestRecallMetrics = metrics;
                        }
                    }
                }
            }
        }
    }

    const auto materializePolicy = [&](const Policy& policy) {
        std::vector<bool> retain(componentCount + 1, false);
        for (std::size_t label = 1; label <= componentCount; ++label) {
            retain[label] = retainedByPolicy(components[label], policy);
        }
        std::vector<unsigned char> values(
            baselineValues, baselineValues + voxelCount);
        for (std::size_t index = 0; index < voxelCount; ++index) {
            const std::uint32_t label = labelValues[index];
            if (label < retain.size() && retain[label]) {
                values[index] = 1;
            }
        }
        return values;
    };
    result.bestDiceValues = materializePolicy(bestDicePolicy);
    result.bestRecallValues = materializePolicy(bestRecallPolicy);
    result.fixedTopologyValues = materializePolicy(fixedTopologyPolicy);
    result.fixedTopologyMetrics = quickMetrics(
        result.fixedTopologyValues.data(), voxelCount, reference,
        referenceForeground);
    char fixedTopologyName[192] = {};
    std::snprintf(
        fixedTopologyName, sizeof(fixedTopologyName),
        "%s.policy.min%zu.max%zu.contact%.9g.endpoint%.9g.secondary%.9g",
        prefix, fixedTopologyPolicy.minimumVoxels,
        fixedTopologyPolicy.maximumVoxels,
        fixedTopologyPolicy.maximumContactFraction,
        fixedTopologyPolicy.maximumEndpointDistanceMm,
        fixedTopologyPolicy.maximumSecondaryPriorFraction);
    result.fixedTopologyName = fixedTopologyName;
    std::printf(
        "%s.best_dice=%s,predicted=%zu,intersection=%zu,dice=%.9g,precision=%.9g,recall=%.9g\n",
        prefix, result.bestDiceName.c_str(),
        result.bestDiceMetrics.predicted,
        result.bestDiceMetrics.intersection,
        result.bestDiceMetrics.dice,
        result.bestDiceMetrics.precision,
        result.bestDiceMetrics.recall);
    std::printf(
        "%s.best_recall=%s,predicted=%zu,intersection=%zu,dice=%.9g,precision=%.9g,recall=%.9g\n",
        prefix, result.bestRecallName.c_str(),
        result.bestRecallMetrics.predicted,
        result.bestRecallMetrics.intersection,
        result.bestRecallMetrics.dice,
        result.bestRecallMetrics.precision,
        result.bestRecallMetrics.recall);
    std::printf(
        "%s.fixed_topology=%s,predicted=%zu,intersection=%zu,dice=%.9g,precision=%.9g,recall=%.9g\n",
        prefix, result.fixedTopologyName.c_str(),
        result.fixedTopologyMetrics.predicted,
        result.fixedTopologyMetrics.intersection,
        result.fixedTopologyMetrics.dice,
        result.fixedTopologyMetrics.precision,
        result.fixedTopologyMetrics.recall);
    return result;
}

struct MinimalPathBridgeResult {
    std::vector<unsigned char> bridgeOnlyValues;
    std::vector<unsigned char> corridorChainValues;
    std::vector<unsigned char> values;
    std::size_t remoteComponentCount = 0;
    std::size_t eligibleComponentCount = 0;
    std::size_t selectedComponentCount = 0;
    std::size_t successfulPathCount = 0;
    std::size_t pathVoxelCount = 0;
    std::size_t localRecoveryVoxelCount = 0;
};

MinimalPathBridgeResult bridgeRemoteComponents(
    BinaryImage* root,
    BinaryImage* candidate,
    BinaryImage* roiDomain,
    BinaryImage* primaryPrior,
    BinaryImage* secondaryPrior,
    FloatImage* ct,
    FloatImage* vesselness,
    const SkeletonEndpointSet& rootTopology,
    const std::vector<xq::XQSegmentationMask::LabelType>& reference,
    const xq::ImageGeometry& geometry,
    std::size_t voxelCount)
{
    MinimalPathBridgeResult result;
    const unsigned char* rootValues = root->GetBufferPointer();
    result.values.assign(rootValues, rootValues + voxelCount);
    result.corridorChainValues.assign(rootValues, rootValues + voxelCount);
    if (rootTopology.endpointIndices.empty()) {
        return result;
    }

    Connected::Pointer connected = Connected::New();
    connected->SetInput(candidate);
    connected->SetFullyConnected(true);
    Relabel::Pointer relabel = Relabel::New();
    relabel->SetInput(connected->GetOutput());
    relabel->Update();
    LabelImage::Pointer labels = relabel->GetOutput();
    labels->DisconnectPipeline();

    struct RemoteComponent {
        std::uint32_t label = 0;
        std::size_t voxels = 0;
        std::size_t rootOverlapVoxels = 0;
        std::size_t referenceVoxels = 0;
        std::size_t primaryPriorVoxels = 0;
        std::size_t secondaryPriorVoxels = 0;
        double vesselnessSum = 0.0;
        double vesselnessMaximum = 0.0;
        double ctSum = 0.0;
        double ctMinimum = (std::numeric_limits<double>::infinity)();
        double ctMaximum = -(std::numeric_limits<double>::infinity)();
        double minimumEndpointDistanceMm =
            (std::numeric_limits<double>::infinity)();
        double endpointAlignment = -1.0;
        double arrivalTime =
            (std::numeric_limits<double>::infinity)();
        double geodesicCostRatio =
            (std::numeric_limits<double>::infinity)();
        std::size_t nearestEndpointOrdinal = 0;
        std::size_t nearestCandidateFlat = 0;
        std::vector<std::size_t> voxelFlats;
        double indexSum[3] = {0.0, 0.0, 0.0};
        int minimumIndex[3] = {
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)()
        };
        int maximumIndex[3] = {
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)()
        };
    };

    const std::size_t componentCount = relabel->GetNumberOfObjects();
    std::vector<RemoteComponent> components(componentCount + 1);
    for (std::size_t label = 1; label <= componentCount; ++label) {
        components[label].label = static_cast<std::uint32_t>(label);
    }
    const std::uint32_t* labelValues = labels->GetBufferPointer();
    const unsigned char* primaryValues = primaryPrior->GetBufferPointer();
    const unsigned char* secondaryValues = secondaryPrior->GetBufferPointer();
    const float* vesselnessValues = vesselness->GetBufferPointer();
    const float* ctValues = ct->GetBufferPointer();
    std::size_t flat = 0;
    for (int z = 0; z < geometry.dimensions[2]; ++z) {
        for (int y = 0; y < geometry.dimensions[1]; ++y) {
            for (int x = 0; x < geometry.dimensions[0]; ++x, ++flat) {
                const std::uint32_t label = labelValues[flat];
                if (label == 0 || label > componentCount) {
                    continue;
                }
                RemoteComponent& component = components[label];
                ++component.voxels;
                component.voxelFlats.push_back(flat);
                if (rootValues[flat] != 0) {
                    ++component.rootOverlapVoxels;
                }
                if (reference[flat] != 0) {
                    ++component.referenceVoxels;
                }
                if (primaryValues[flat] != 0) {
                    ++component.primaryPriorVoxels;
                }
                if (secondaryValues[flat] != 0) {
                    ++component.secondaryPriorVoxels;
                }
                const double response = vesselnessValues[flat];
                component.vesselnessSum += response;
                component.vesselnessMaximum = (std::max)(
                    component.vesselnessMaximum, response);
                const double intensity = ctValues[flat];
                component.ctSum += intensity;
                component.ctMinimum = (std::min)(component.ctMinimum,
                                                 intensity);
                component.ctMaximum = (std::max)(component.ctMaximum,
                                                 intensity);
                const int index[3] = {x, y, z};
                for (int axis = 0; axis < 3; ++axis) {
                    component.indexSum[axis] += index[axis];
                    component.minimumIndex[axis] = (std::min)(
                        component.minimumIndex[axis], index[axis]);
                    component.maximumIndex[axis] = (std::max)(
                        component.maximumIndex[axis], index[axis]);
                }
                for (std::size_t endpointOrdinal = 0;
                     endpointOrdinal < rootTopology.endpointIndices.size();
                     ++endpointOrdinal) {
                    double squaredDistance = 0.0;
                    for (int axis = 0; axis < 3; ++axis) {
                        const double delta = static_cast<double>(
                            index[axis]
                            - rootTopology.endpointIndices[endpointOrdinal][axis])
                            * geometry.spacing[axis];
                        squaredDistance += delta * delta;
                    }
                    if (squaredDistance
                        < component.minimumEndpointDistanceMm
                            * component.minimumEndpointDistanceMm) {
                        component.minimumEndpointDistanceMm =
                            std::sqrt(squaredDistance);
                        component.nearestEndpointOrdinal = endpointOrdinal;
                        component.nearestCandidateFlat = flat;
                    }
                }
            }
        }
    }

    for (std::size_t label = 1; label <= componentCount; ++label) {
        RemoteComponent& component = components[label];
        if (component.voxels == 0
            || component.nearestEndpointOrdinal
                >= rootTopology.endpointOutwardDirections.size()) {
            continue;
        }
        itk::ContinuousIndex<double, kDimension> centroidIndex;
        for (int axis = 0; axis < 3; ++axis) {
            centroidIndex[axis] = component.indexSum[axis]
                / static_cast<double>(component.voxels);
        }
        BinaryImage::PointType centroidPoint;
        BinaryImage::PointType endpointPoint;
        root->TransformContinuousIndexToPhysicalPoint(centroidIndex,
                                                      centroidPoint);
        root->TransformIndexToPhysicalPoint(
            rootTopology.endpointIndices[component.nearestEndpointOrdinal],
            endpointPoint);
        std::array<double, 3> direction = {0.0, 0.0, 0.0};
        double norm = 0.0;
        for (int axis = 0; axis < 3; ++axis) {
            direction[axis] = centroidPoint[axis] - endpointPoint[axis];
            norm += direction[axis] * direction[axis];
        }
        norm = std::sqrt(norm);
        if (norm > 0.0) {
            component.endpointAlignment = 0.0;
            for (int axis = 0; axis < 3; ++axis) {
                component.endpointAlignment +=
                    direction[axis] / norm
                    * rootTopology.endpointOutwardDirections[
                        component.nearestEndpointOrdinal][axis];
            }
        }
    }

    const double minimumSpacing = (std::min)(
        geometry.spacing[0],
        (std::min)(geometry.spacing[1], geometry.spacing[2]));
    const auto flatToIndex = [&](std::size_t index) {
        BinaryImage::IndexType output;
        const std::size_t dimX = static_cast<std::size_t>(
            geometry.dimensions[0]);
        const std::size_t dimY = static_cast<std::size_t>(
            geometry.dimensions[1]);
        output[0] = static_cast<BinaryImage::IndexType::IndexValueType>(
            index % dimX);
        index /= dimX;
        output[1] = static_cast<BinaryImage::IndexType::IndexValueType>(
            index % dimY);
        output[2] = static_cast<BinaryImage::IndexType::IndexValueType>(
            index / dimY);
        return output;
    };
    const auto indexToFlat = [&](const BinaryImage::IndexType& index) {
        return static_cast<std::size_t>(index[0])
            + static_cast<std::size_t>(geometry.dimensions[0])
                * (static_cast<std::size_t>(index[1])
                   + static_cast<std::size_t>(geometry.dimensions[1])
                       * static_cast<std::size_t>(index[2]));
    };

    std::vector<float> speedValues(voxelCount, 0.0001f);
    const unsigned char* roiValues = roiDomain->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (roiValues[index] == 0) {
            continue;
        }
        const double response = (std::max)(
            0.0, static_cast<double>(vesselnessValues[index]));
        const double responseScore = response / (response + 5.0);
        const double intensityScore = std::clamp(
            (static_cast<double>(ctValues[index]) - 60.0) / 100.0,
            0.0, 1.0);
        double speed = 0.05 + 0.75 * responseScore
            + 0.20 * intensityScore;
        if (ctValues[index] < 40.0f || ctValues[index] > 350.0f) {
            speed *= 0.1;
        }
        speedValues[index] = static_cast<float>(
            std::clamp(speed, 0.0001, 1.0));
        if (rootValues[index] != 0) {
            speedValues[index] = 1.0f;
        }
    }
    FloatImage::Pointer speed = importImage<FloatImage, FloatImporter>(
        geometry, speedValues.data(), voxelCount);
    BinaryImage::Pointer looseIntensity = threshold(ct, 80.0, 300.0);
    BinaryImage::Pointer looseResponse = threshold(
        vesselness, 2.0, (std::numeric_limits<float>::max)());
    BinaryImage::Pointer looseSupport = binaryAnd(
        binaryAnd(looseIntensity, looseResponse), roiDomain);

    ArrivalTimeMarcher::NodeContainerPointer trialPoints =
        ArrivalTimeMarcher::NodeContainer::New();
    for (std::size_t endpointOrdinal = 0;
         endpointOrdinal < rootTopology.endpointIndices.size();
         ++endpointOrdinal) {
        ArrivalTimeMarcher::NodeType node;
        node.SetIndex(rootTopology.endpointIndices[endpointOrdinal]);
        node.SetValue(0.0f);
        trialPoints->InsertElement(endpointOrdinal, node);
    }
    ArrivalTimeMarcher::NodeContainerPointer targetPoints =
        ArrivalTimeMarcher::NodeContainer::New();
    std::size_t targetCount = 0;
    for (std::size_t label = 1; label <= componentCount; ++label) {
        const RemoteComponent& component = components[label];
        if (component.voxels == 0 || component.rootOverlapVoxels != 0) {
            continue;
        }
        ArrivalTimeMarcher::NodeType node;
        node.SetIndex(flatToIndex(component.nearestCandidateFlat));
        node.SetValue(0.0f);
        targetPoints->InsertElement(targetCount, node);
        ++targetCount;
    }
    FloatImage::Pointer rootArrival;
    if (targetCount != 0) {
        try {
            ArrivalTimeMarcher::Pointer marcher = ArrivalTimeMarcher::New();
            marcher->SetInput(speed);
            marcher->SetTrialPoints(trialPoints);
            marcher->SetTargetPoints(targetPoints);
            marcher->SetOutputSize(speed->GetLargestPossibleRegion().GetSize());
            marcher->SetOutputOrigin(speed->GetOrigin());
            marcher->SetOutputSpacing(speed->GetSpacing());
            marcher->SetOutputDirection(speed->GetDirection());
            marcher->SetTargetOffset(0.0);
            marcher->SetTargetReachedModeToAllTargets();
            marcher->Update();
            FloatImage::Pointer arrival = marcher->GetOutput();
            arrival->DisconnectPipeline();
            rootArrival = arrival;
            for (std::size_t label = 1; label <= componentCount; ++label) {
                RemoteComponent& component = components[label];
                if (component.voxels == 0
                    || component.rootOverlapVoxels != 0) {
                    continue;
                }
                const double value = arrival->GetPixel(
                    flatToIndex(component.nearestCandidateFlat));
                if (!std::isfinite(value)
                    || value >= static_cast<double>(
                        (std::numeric_limits<float>::max)())) {
                    continue;
                }
                component.arrivalTime = value;
                component.geodesicCostRatio = value / (std::max)(
                    component.minimumEndpointDistanceMm, minimumSpacing);
            }
            std::printf(
                "roi.fast_marching_arrival.status=ok,trial_points=%zu,target_points=%zu,reached_targets=%zu,last_target_arrival=%.9g\n",
                rootTopology.endpointIndices.size(), targetCount,
                static_cast<std::size_t>(
                    marcher->GetReachedTargetPoints()->Size()),
                marcher->GetTargetValue());
        } catch (const itk::ExceptionObject& exception) {
            std::printf(
                "roi.fast_marching_arrival.status=itk_failure,description=%s\n",
                exception.GetDescription());
        }
    }

    constexpr std::size_t minimumVoxels = 3;
    constexpr std::size_t maximumVoxels = 120;
    constexpr double maximumEndpointDistanceMm = 22.5;
    constexpr double maximumSecondaryPriorFraction = 0.25;
    constexpr double minimumEndpointAlignment = 0.70;
    constexpr double maximumElongation = 2.0;
    constexpr double maximumMeanVesselness = 7.8;
    constexpr double maximumPeakVesselness = 12.5;
    constexpr double minimumMeanCt = 140.0;
    constexpr double maximumMeanCt = 175.0;
    constexpr std::size_t maximumPaths = 12;
    struct RankedComponent {
        std::size_t label = 0;
        double score = 0.0;
        double elongation = 0.0;
    };
    std::vector<RankedComponent> eligible;
    std::vector<std::size_t> nonDirectionalEligible;
    for (std::size_t label = 1; label <= componentCount; ++label) {
        const RemoteComponent& component = components[label];
        if (component.voxels == 0 || component.rootOverlapVoxels != 0) {
            continue;
        }
        ++result.remoteComponentCount;
        double extents[3] = {};
        for (int axis = 0; axis < 3; ++axis) {
            extents[axis] = static_cast<double>(
                component.maximumIndex[axis] - component.minimumIndex[axis] + 1)
                * geometry.spacing[axis];
        }
        std::sort(extents, extents + 3, std::greater<double>());
        const double elongation = extents[0]
            / (std::max)(extents[1], 1e-12);
        const double inverseVoxels = 1.0
            / static_cast<double>(component.voxels);
        const double secondaryFraction =
            static_cast<double>(component.secondaryPriorVoxels)
            * inverseVoxels;
        const double meanVesselness = component.vesselnessSum * inverseVoxels;
        const double meanCt = component.ctSum * inverseVoxels;
        if (component.voxels < minimumVoxels
            || component.voxels > maximumVoxels
            || component.minimumEndpointDistanceMm > maximumEndpointDistanceMm
            || secondaryFraction > maximumSecondaryPriorFraction
            || elongation > maximumElongation
            || meanVesselness > maximumMeanVesselness
            || component.vesselnessMaximum > maximumPeakVesselness
            || meanCt < minimumMeanCt
            || meanCt > maximumMeanCt) {
            continue;
        }
        nonDirectionalEligible.push_back(label);
        if (component.endpointAlignment < minimumEndpointAlignment) {
            continue;
        }
        const double score =
            component.minimumEndpointDistanceMm / maximumEndpointDistanceMm
            + 4.0 * (1.0 - component.endpointAlignment)
            + 0.5 * (std::max)(0.0, elongation - 1.5)
            + std::abs(meanVesselness - 6.2) / 5.0
            + std::abs(meanCt - 155.0) / 100.0;
        eligible.push_back({label, score, elongation});
    }
    result.eligibleComponentCount = eligible.size();
    std::sort(eligible.begin(), eligible.end(),
              [](const RankedComponent& left,
                 const RankedComponent& right) {
                  if (left.score != right.score) {
                      return left.score < right.score;
                  }
                  return left.label < right.label;
              });
    std::sort(nonDirectionalEligible.begin(), nonDirectionalEligible.end(),
              [&](std::size_t left, std::size_t right) {
                  if (components[left].geodesicCostRatio
                      != components[right].geodesicCostRatio) {
                      return components[left].geodesicCostRatio
                          < components[right].geodesicCostRatio;
                  }
                  return left < right;
              });

    std::vector<std::size_t> referenceOrder;
    std::vector<std::size_t> primaryOrder;
    std::vector<std::size_t> sizeOrder;
    for (std::size_t label = 1; label <= componentCount; ++label) {
        const RemoteComponent& component = components[label];
        if (component.voxels == 0 || component.rootOverlapVoxels != 0) {
            continue;
        }
        sizeOrder.push_back(label);
        if (component.referenceVoxels != 0) {
            referenceOrder.push_back(label);
        }
        if (component.primaryPriorVoxels != 0) {
            primaryOrder.push_back(label);
        }
    }
    std::sort(referenceOrder.begin(), referenceOrder.end(),
              [&](std::size_t left, std::size_t right) {
                  if (components[left].referenceVoxels
                      != components[right].referenceVoxels) {
                      return components[left].referenceVoxels
                          > components[right].referenceVoxels;
                  }
                  return left < right;
              });
    std::sort(primaryOrder.begin(), primaryOrder.end(),
              [&](std::size_t left, std::size_t right) {
                  if (components[left].primaryPriorVoxels
                      != components[right].primaryPriorVoxels) {
                      return components[left].primaryPriorVoxels
                          > components[right].primaryPriorVoxels;
                  }
                  return left < right;
              });
    std::sort(sizeOrder.begin(), sizeOrder.end(),
              [&](std::size_t left, std::size_t right) {
                  if (components[left].voxels != components[right].voxels) {
                      return components[left].voxels
                          > components[right].voxels;
                  }
                  return left < right;
              });
    const auto printAudit = [&](const char* auditPrefix,
                                const std::vector<std::size_t>& order,
                                std::size_t maximumReportCount = 30) {
        const std::size_t reportCount = (std::min)(
            maximumReportCount, order.size());
        for (std::size_t rank = 0; rank < reportCount; ++rank) {
            const RemoteComponent& component = components[order[rank]];
            double extents[3] = {};
            for (int axis = 0; axis < 3; ++axis) {
                extents[axis] = static_cast<double>(
                    component.maximumIndex[axis]
                    - component.minimumIndex[axis] + 1)
                    * geometry.spacing[axis];
            }
            std::sort(extents, extents + 3, std::greater<double>());
            const double inverseVoxels = 1.0
                / static_cast<double>(component.voxels);
            itk::ContinuousIndex<double, kDimension> centroidIndex;
            for (int axis = 0; axis < 3; ++axis) {
                centroidIndex[axis] = component.indexSum[axis]
                    * inverseVoxels;
            }
            BinaryImage::PointType centroidPoint;
            root->TransformContinuousIndexToPhysicalPoint(centroidIndex,
                                                          centroidPoint);
            const BinaryImage::IndexType targetIndex = flatToIndex(
                component.nearestCandidateFlat);
            std::printf(
                "%s.c%zu.label=%u,voxels=%zu,reference_voxels=%zu,reference_precision=%.9g,primary_fraction=%.9g,secondary_fraction=%.9g,nearest_endpoint=%zu,target_index=%lld:%lld:%lld,centroid_lps_mm=%.9g:%.9g:%.9g,endpoint_distance_mm=%.9g,endpoint_alignment=%.9g,arrival_time=%.9g,geodesic_cost_ratio=%.9g,mean_vesselness=%.9g,max_vesselness=%.9g,ct_mean=%.9g,ct_min=%.9g,ct_max=%.9g,elongation=%.9g\n",
                auditPrefix, rank + 1, component.label, component.voxels,
                component.referenceVoxels,
                static_cast<double>(component.referenceVoxels)
                    * inverseVoxels,
                static_cast<double>(component.primaryPriorVoxels)
                    * inverseVoxels,
                static_cast<double>(component.secondaryPriorVoxels)
                    * inverseVoxels,
                component.nearestEndpointOrdinal,
                static_cast<long long>(targetIndex[0]),
                static_cast<long long>(targetIndex[1]),
                static_cast<long long>(targetIndex[2]),
                centroidPoint[0], centroidPoint[1], centroidPoint[2],
                component.minimumEndpointDistanceMm,
                component.endpointAlignment,
                component.arrivalTime,
                component.geodesicCostRatio,
                component.vesselnessSum * inverseVoxels,
                component.vesselnessMaximum,
                component.ctSum * inverseVoxels,
                component.ctMinimum, component.ctMaximum,
                extents[0] / (std::max)(extents[1], 1e-12));
        }
    };
    printAudit("roi.remote_size_audit", sizeOrder);
    printAudit("roi.remote_nondirectional_audit", nonDirectionalEligible, 80);
    printAudit("roi.remote_truth_audit", referenceOrder);
    printAudit("roi.remote_primary_audit", primaryOrder);

    struct CorridorPath {
        std::size_t label = 0;
        std::size_t rootEndpointOrdinal =
            (std::numeric_limits<std::size_t>::max)();
        std::vector<std::size_t> pathFlats;
        std::vector<std::size_t> memberLabels;
        std::size_t vertexCount = 0;
        std::size_t centerlineSupportedVoxels = 0;
        std::size_t maximumUnsupportedRun = 0;
        bool selected = false;
    };
    std::vector<CorridorPath> corridorPaths;
    if (rootArrival.IsNotNull() && !nonDirectionalEligible.empty()) {
        try {
            using Interpolator = itk::LinearInterpolateImageFunction<
                FloatImage,
                ArrivalPathFilter::CostFunctionType::CoordRepType>;
            Interpolator::Pointer interpolator = Interpolator::New();
            ArrivalPathFilter::CostFunctionType::Pointer cost =
                ArrivalPathFilter::CostFunctionType::New();
            cost->SetInterpolator(interpolator);
            MinimalPathOptimizer::Pointer optimizer =
                MinimalPathOptimizer::New();
            MinimalPathOptimizer::NeighborhoodSizeType neighborhood(3);
            for (int axis = 0; axis < 3; ++axis) {
                neighborhood[axis] = geometry.spacing[axis];
            }
            optimizer->SetNeighborhoodSize(neighborhood);
            optimizer->FullyConnectedOn();
            optimizer->MinimizeOn();

            ArrivalPathFilter::Pointer filter = ArrivalPathFilter::New();
            filter->SetInput(rootArrival);
            filter->SetCostFunction(cost);
            filter->SetOptimizer(optimizer);
            filter->SetTerminationValue(minimumSpacing);
            corridorPaths.resize(nonDirectionalEligible.size());
            for (std::size_t ordinal = 0;
                 ordinal < nonDirectionalEligible.size(); ++ordinal) {
                const std::size_t label = nonDirectionalEligible[ordinal];
                corridorPaths[ordinal].label = label;
                ArrivalPathFilter::PointType targetPoint;
                rootArrival->TransformIndexToPhysicalPoint(
                    flatToIndex(components[label].nearestCandidateFlat),
                    targetPoint);
                filter->AddPathEndPoint(targetPoint);
            }
            filter->Update();

            const unsigned char* looseSupportValues =
                looseSupport->GetBufferPointer();
            const auto physicalSquaredDistance =
                [&](std::size_t leftFlat, std::size_t rightFlat) {
                    const BinaryImage::IndexType left = flatToIndex(leftFlat);
                    const BinaryImage::IndexType right = flatToIndex(rightFlat);
                    double squaredDistance = 0.0;
                    for (int axis = 0; axis < 3; ++axis) {
                        const double delta = static_cast<double>(
                            left[axis] - right[axis]) * geometry.spacing[axis];
                        squaredDistance += delta * delta;
                    }
                    return squaredDistance;
                };
            for (std::size_t ordinal = 0;
                 ordinal < corridorPaths.size(); ++ordinal) {
                CorridorPath& record = corridorPaths[ordinal];
                MinimalPath::Pointer path = filter->GetOutput(ordinal);
                record.vertexCount = path->GetVertexList()->Size();
                if (record.vertexCount < 2) {
                    continue;
                }
                MinimalPathIterator iterator(root, path);
                std::size_t unsupportedRun = 0;
                for (iterator.GoToBegin(); !iterator.IsAtEnd(); ++iterator) {
                    const std::size_t pathFlat = indexToFlat(
                        iterator.GetIndex());
                    if (record.pathFlats.empty()
                        || record.pathFlats.back() != pathFlat) {
                        record.pathFlats.push_back(pathFlat);
                        if (looseSupportValues[pathFlat] != 0) {
                            ++record.centerlineSupportedVoxels;
                            unsupportedRun = 0;
                        } else {
                            ++unsupportedRun;
                            record.maximumUnsupportedRun = (std::max)(
                                record.maximumUnsupportedRun, unsupportedRun);
                        }
                    }
                }
                if (record.pathFlats.empty()) {
                    continue;
                }

                std::size_t rootFlat = record.pathFlats.front();
                double minimumArrival =
                    rootArrival->GetPixel(flatToIndex(rootFlat));
                for (std::size_t pathFlat : record.pathFlats) {
                    const double arrival = rootArrival->GetPixel(
                        flatToIndex(pathFlat));
                    if (arrival < minimumArrival) {
                        minimumArrival = arrival;
                        rootFlat = pathFlat;
                    }
                }
                double minimumRootDistance =
                    (std::numeric_limits<double>::infinity)();
                for (std::size_t endpointOrdinal = 0;
                     endpointOrdinal < rootTopology.endpointIndices.size();
                     ++endpointOrdinal) {
                    const double squaredDistance = physicalSquaredDistance(
                        rootFlat,
                        indexToFlat(rootTopology.endpointIndices[
                            endpointOrdinal]));
                    if (squaredDistance < minimumRootDistance) {
                        minimumRootDistance = squaredDistance;
                        record.rootEndpointOrdinal = endpointOrdinal;
                    }
                }
            }

            constexpr double pathTubeRadiusMm = 1.0;
            const double pathTubeRadiusSquared =
                pathTubeRadiusMm * pathTubeRadiusMm;
            for (CorridorPath& targetPath : corridorPaths) {
                if (targetPath.pathFlats.empty()
                    || targetPath.rootEndpointOrdinal
                        == (std::numeric_limits<std::size_t>::max)()) {
                    continue;
                }
                targetPath.memberLabels.push_back(targetPath.label);
                const double targetArrival =
                    components[targetPath.label].arrivalTime;
                for (const CorridorPath& upstreamPath : corridorPaths) {
                    if (upstreamPath.label == targetPath.label
                        || upstreamPath.pathFlats.empty()
                        || upstreamPath.rootEndpointOrdinal
                            != targetPath.rootEndpointOrdinal
                        || !(components[upstreamPath.label].arrivalTime
                             < targetArrival)) {
                        continue;
                    }
                    bool corridorContact = false;
                    for (std::size_t pathFlat : targetPath.pathFlats) {
                        for (std::size_t componentFlat :
                             components[upstreamPath.label].voxelFlats) {
                            if (physicalSquaredDistance(pathFlat, componentFlat)
                                <= pathTubeRadiusSquared) {
                                corridorContact = true;
                                break;
                            }
                        }
                        if (corridorContact) {
                            break;
                        }
                    }
                    if (corridorContact) {
                        targetPath.memberLabels.push_back(upstreamPath.label);
                    }
                }
                std::sort(targetPath.memberLabels.begin(),
                          targetPath.memberLabels.end());
                targetPath.memberLabels.erase(
                    std::unique(targetPath.memberLabels.begin(),
                                targetPath.memberLabels.end()),
                    targetPath.memberLabels.end());
            }

            std::vector<std::size_t> chainOrder;
            for (std::size_t ordinal = 0;
                 ordinal < corridorPaths.size(); ++ordinal) {
                if (corridorPaths[ordinal].memberLabels.size() >= 2) {
                    chainOrder.push_back(ordinal);
                }
            }
            std::sort(chainOrder.begin(), chainOrder.end(),
                      [&](std::size_t left, std::size_t right) {
                          const CorridorPath& leftPath = corridorPaths[left];
                          const CorridorPath& rightPath = corridorPaths[right];
                          if (leftPath.memberLabels.size()
                              != rightPath.memberLabels.size()) {
                              return leftPath.memberLabels.size()
                                  > rightPath.memberLabels.size();
                          }
                          const double leftArrival =
                              components[leftPath.label].arrivalTime;
                          const double rightArrival =
                              components[rightPath.label].arrivalTime;
                          if (leftArrival != rightArrival) {
                              return leftArrival > rightArrival;
                          }
                          if (leftPath.rootEndpointOrdinal
                              != rightPath.rootEndpointOrdinal) {
                              return leftPath.rootEndpointOrdinal
                                  < rightPath.rootEndpointOrdinal;
                          }
                          return leftPath.label < rightPath.label;
                      });
            std::vector<std::size_t> maximalChains;
            for (std::size_t ordinal : chainOrder) {
                const CorridorPath& candidatePath = corridorPaths[ordinal];
                bool covered = false;
                for (std::size_t selectedOrdinal : maximalChains) {
                    const CorridorPath& selectedPath =
                        corridorPaths[selectedOrdinal];
                    if (selectedPath.rootEndpointOrdinal
                            == candidatePath.rootEndpointOrdinal
                        && std::includes(
                            selectedPath.memberLabels.begin(),
                            selectedPath.memberLabels.end(),
                            candidatePath.memberLabels.begin(),
                            candidatePath.memberLabels.end())) {
                        covered = true;
                        break;
                    }
                }
                if (!covered) {
                    maximalChains.push_back(ordinal);
                }
            }
            if (maximalChains.size() > maximumPaths) {
                maximalChains.resize(maximumPaths);
            }
            for (std::size_t ordinal : maximalChains) {
                corridorPaths[ordinal].selected = true;
            }

            for (std::size_t ordinal = 0;
                 ordinal < corridorPaths.size(); ++ordinal) {
                const CorridorPath& record = corridorPaths[ordinal];
                const RemoteComponent& component = components[record.label];
                std::size_t chainReferenceVoxels = 0;
                std::string members;
                for (std::size_t memberOrdinal = 0;
                     memberOrdinal < record.memberLabels.size();
                     ++memberOrdinal) {
                    const std::size_t memberLabel =
                        record.memberLabels[memberOrdinal];
                    chainReferenceVoxels +=
                        components[memberLabel].referenceVoxels;
                    if (!members.empty()) {
                        members += ':';
                    }
                    members += std::to_string(memberLabel);
                }
                std::printf(
                    "roi.corridor_path.c%zu.label=%u,route_endpoint=%zu,path_vertices=%zu,path_voxels=%zu,centerline_supported_voxels=%zu,max_unsupported_run=%zu,arrival_time=%.9g,members=%zu,member_labels=%s,reference_voxels=%zu,selected=%d\n",
                    ordinal + 1, component.label,
                    record.rootEndpointOrdinal, record.vertexCount,
                    record.pathFlats.size(),
                    record.centerlineSupportedVoxels,
                    record.maximumUnsupportedRun,
                    component.arrivalTime, record.memberLabels.size(),
                    members.c_str(), chainReferenceVoxels,
                    record.selected ? 1 : 0);
            }

            if (!maximalChains.empty()) {
                BinaryImage::Pointer corridorPathMask = BinaryImage::New();
                corridorPathMask->CopyInformation(root);
                corridorPathMask->SetRegions(
                    root->GetLargestPossibleRegion());
                corridorPathMask->Allocate();
                corridorPathMask->FillBuffer(0);
                unsigned char* corridorPathValues =
                    corridorPathMask->GetBufferPointer();
                std::vector<bool> selectedLabels(componentCount + 1, false);
                for (std::size_t ordinal : maximalChains) {
                    const CorridorPath& record = corridorPaths[ordinal];
                    for (std::size_t pathFlat : record.pathFlats) {
                        corridorPathValues[pathFlat] = 1;
                    }
                    for (std::size_t memberLabel : record.memberLabels) {
                        selectedLabels[memberLabel] = true;
                    }
                }
                std::vector<unsigned char> selectedRemoteValues(voxelCount, 0);
                for (std::size_t index = 0; index < voxelCount; ++index) {
                    const std::uint32_t label = labelValues[index];
                    if (label < selectedLabels.size()
                        && selectedLabels[label]) {
                        selectedRemoteValues[index] = 1;
                    }
                }
                BinaryImage::Pointer selectedRemote =
                    importImage<BinaryImage, BinaryImporter>(
                        geometry, selectedRemoteValues.data(), voxelCount);
                BinaryImage::Pointer corridorPathTube = dilatePhysical(
                    corridorPathMask, geometry, pathTubeRadiusMm);
                BinaryImage::Pointer supportedCorridor = binaryAnd(
                    corridorPathTube, looseSupport);
                BinaryImage::Pointer corridorBridge = binaryOr(
                    corridorPathMask, supportedCorridor);
                BinaryImage::Pointer rootAndRemote = binaryOr(
                    root, selectedRemote);
                BinaryImage::Pointer connectedDomain = binaryOr(
                    rootAndRemote, corridorBridge);
                BinaryImage::Pointer corridorResult = reconstruct(
                    root, connectedDomain);
                const unsigned char* corridorValues =
                    corridorResult->GetBufferPointer();
                result.corridorChainValues.assign(
                    corridorValues, corridorValues + voxelCount);

                Connected::Pointer corridorConnected = Connected::New();
                corridorConnected->SetInput(corridorResult);
                corridorConnected->SetFullyConnected(true);
                Relabel::Pointer corridorRelabel = Relabel::New();
                corridorRelabel->SetInput(corridorConnected->GetOutput());
                corridorRelabel->Update();
                std::printf(
                    "roi.corridor_chain.selected_paths=%zu,selected_components=%zu,final_components=%zu\n",
                    maximalChains.size(),
                    static_cast<std::size_t>(std::count(
                        selectedLabels.begin(), selectedLabels.end(), true)),
                    static_cast<std::size_t>(
                        corridorRelabel->GetNumberOfObjects()));
            } else {
                std::printf(
                    "roi.corridor_chain.selected_paths=0,selected_components=0,final_components=1\n");
            }
        } catch (const itk::ExceptionObject& exception) {
            std::printf(
                "roi.corridor_chain.status=itk_failure,description=%s\n",
                exception.GetDescription());
        }
    }

    struct EndpointGroup {
        std::size_t endpointOrdinal = 0;
        std::vector<RankedComponent> members;
        std::size_t totalVoxels = 0;
        double score = 0.0;
        bool hasStrongComponent = false;
    };
    std::vector<EndpointGroup> endpointGroups(
        rootTopology.endpointIndices.size());
    for (std::size_t endpointOrdinal = 0;
         endpointOrdinal < endpointGroups.size(); ++endpointOrdinal) {
        endpointGroups[endpointOrdinal].endpointOrdinal = endpointOrdinal;
    }
    for (const RankedComponent& ranked : eligible) {
        const RemoteComponent& component = components[ranked.label];
        EndpointGroup& group =
            endpointGroups[component.nearestEndpointOrdinal];
        group.members.push_back(ranked);
        group.totalVoxels += component.voxels;
        group.hasStrongComponent = group.hasStrongComponent
            || component.voxels >= 20;
    }
    std::vector<EndpointGroup> supportedGroups;
    for (EndpointGroup& group : endpointGroups) {
        if (group.members.empty()
            || (!group.hasStrongComponent
                && (group.members.size() < 2 || group.totalVoxels < 12))) {
            continue;
        }
        std::sort(group.members.begin(), group.members.end(),
                  [&](const RankedComponent& left,
                      const RankedComponent& right) {
                      const RemoteComponent& leftComponent =
                          components[left.label];
                      const RemoteComponent& rightComponent =
                          components[right.label];
                      if (leftComponent.minimumEndpointDistanceMm
                          != rightComponent.minimumEndpointDistanceMm) {
                          return leftComponent.minimumEndpointDistanceMm
                              > rightComponent.minimumEndpointDistanceMm;
                      }
                      return left.label < right.label;
                  });
        const RemoteComponent& target =
            components[group.members.front().label];
        const double targetInverseVoxels = 1.0
            / static_cast<double>(target.voxels);
        const double targetMeanVesselness =
            target.vesselnessSum * targetInverseVoxels;
        const double targetMeanCt = target.ctSum * targetInverseVoxels;
        const bool weakContinuationChain = group.members.size() >= 2
            && group.totalVoxels >= 14
            && target.endpointAlignment >= 0.79
            && targetMeanVesselness <= 6.8
            && targetMeanCt <= 170.0;
        const bool shortStrongContinuation = group.hasStrongComponent
            && target.minimumEndpointDistanceMm <= 8.0
            && target.endpointAlignment >= 0.79
            && targetMeanVesselness <= maximumMeanVesselness;
        if (!weakContinuationChain && !shortStrongContinuation) {
            continue;
        }
        group.score =
            2.0 * (1.0 - target.endpointAlignment)
            - (std::min)(1.0,
                         static_cast<double>(group.totalVoxels) / 120.0)
            - 0.20 * static_cast<double>((std::min)(
                static_cast<std::size_t>(4), group.members.size()))
            - 0.50 * target.minimumEndpointDistanceMm
                / maximumEndpointDistanceMm;
        supportedGroups.push_back(group);
    }
    std::sort(supportedGroups.begin(), supportedGroups.end(),
              [](const EndpointGroup& left, const EndpointGroup& right) {
                  if (left.score != right.score) {
                      return left.score < right.score;
                  }
                  return left.endpointOrdinal < right.endpointOrdinal;
              });
    if (supportedGroups.size() > maximumPaths) {
        supportedGroups.resize(maximumPaths);
    }
    result.selectedComponentCount = 0;
    for (const EndpointGroup& group : supportedGroups) {
        result.selectedComponentCount += group.members.size();
    }
    std::printf(
        "roi.minimal_path.supported_endpoint_groups=%zu,selected_paths=%zu\n",
        supportedGroups.size(), supportedGroups.size());
    for (std::size_t ordinal = 0; ordinal < supportedGroups.size(); ++ordinal) {
        const EndpointGroup& group = supportedGroups[ordinal];
        const RemoteComponent& target =
            components[group.members.front().label];
        std::size_t groupReferenceVoxels = 0;
        for (const RankedComponent& member : group.members) {
            groupReferenceVoxels += components[member.label].referenceVoxels;
        }
        std::printf(
            "roi.minimal_path.group%zu.endpoint=%zu,members=%zu,total_voxels=%zu,reference_voxels=%zu,target_label=%u,target_distance_mm=%.9g,target_alignment=%.9g,score=%.9g\n",
            ordinal + 1, group.endpointOrdinal, group.members.size(),
            group.totalVoxels, groupReferenceVoxels, target.label,
            target.minimumEndpointDistanceMm, target.endpointAlignment,
            group.score);
    }
    std::printf(
        "roi.minimal_path.policy.min_voxels=%zu,max_voxels=%zu,max_endpoint_mm=%.9g,max_secondary_fraction=%.9g,min_endpoint_alignment=%.9g,max_elongation=%.9g,max_mean_vesselness=%.9g,max_peak_vesselness=%.9g,mean_ct_range=%.9g..%.9g,max_paths=%zu\n",
        minimumVoxels, maximumVoxels, maximumEndpointDistanceMm,
        maximumSecondaryPriorFraction, minimumEndpointAlignment,
        maximumElongation, maximumMeanVesselness,
        maximumPeakVesselness, minimumMeanCt, maximumMeanCt, maximumPaths);
    std::printf(
        "roi.minimal_path.remote_components=%zu,eligible_components=%zu,selected_components=%zu,root_endpoints=%zu\n",
        result.remoteComponentCount, result.eligibleComponentCount,
        result.selectedComponentCount, rootTopology.endpointIndices.size());

    BinaryImage::Pointer pathMask = BinaryImage::New();
    pathMask->CopyInformation(root);
    pathMask->SetRegions(root->GetLargestPossibleRegion());
    pathMask->Allocate();
    pathMask->FillBuffer(0);
    unsigned char* pathValues = pathMask->GetBufferPointer();
    std::vector<bool> successfulLabel(componentCount + 1, false);
    for (std::size_t ordinal = 0; ordinal < supportedGroups.size(); ++ordinal) {
        const EndpointGroup& group = supportedGroups[ordinal];
        const RankedComponent& ranked = group.members.front();
        const RemoteComponent& component = components[ranked.label];
        const BinaryImage::IndexType startIndex =
            rootTopology.endpointIndices[group.endpointOrdinal];
        const BinaryImage::IndexType endIndex = flatToIndex(
            component.nearestCandidateFlat);
        MinimalPathFilter::PointType startPoint;
        MinimalPathFilter::PointType endPoint;
        speed->TransformIndexToPhysicalPoint(startIndex, startPoint);
        speed->TransformIndexToPhysicalPoint(endIndex, endPoint);

        try {
            using Interpolator = itk::LinearInterpolateImageFunction<
                FloatImage,
                MinimalPathFilter::CostFunctionType::CoordRepType>;
            Interpolator::Pointer interpolator = Interpolator::New();
            MinimalPathFilter::CostFunctionType::Pointer cost =
                MinimalPathFilter::CostFunctionType::New();
            cost->SetInterpolator(interpolator);
            MinimalPathOptimizer::Pointer optimizer =
                MinimalPathOptimizer::New();
            MinimalPathOptimizer::NeighborhoodSizeType neighborhood(3);
            for (int axis = 0; axis < 3; ++axis) {
                neighborhood[axis] = geometry.spacing[axis];
            }
            optimizer->SetNeighborhoodSize(neighborhood);
            optimizer->FullyConnectedOn();
            optimizer->MinimizeOn();

            MinimalPathFilter::Pointer filter = MinimalPathFilter::New();
            filter->SetInput(speed);
            filter->SetCostFunction(cost);
            filter->SetOptimizer(optimizer);
            filter->SetTerminationValue(minimumSpacing);
            MinimalPathFilter::PathInformationType::Pointer pathInfo =
                MinimalPathFilter::PathInformationType::New();
            pathInfo->SetStartPoint(startPoint);
            pathInfo->SetEndPoint(endPoint);
            filter->AddPathInformation(pathInfo);
            filter->Update();
            MinimalPath::Pointer path = filter->GetOutput(0);
            const std::size_t vertexCount = path->GetVertexList()->Size();
            if (vertexCount < 2) {
                std::printf(
                    "roi.minimal_path.c%zu.label=%u,status=empty_path\n",
                    ordinal + 1, component.label);
                continue;
            }
            MinimalPathIterator iterator(pathMask, path);
            for (iterator.GoToBegin(); !iterator.IsAtEnd(); ++iterator) {
                iterator.Set(1);
            }
            pathValues[indexToFlat(startIndex)] = 1;
            pathValues[component.nearestCandidateFlat] = 1;
            for (const RankedComponent& member : group.members) {
                successfulLabel[components[member.label].label] = true;
            }
            ++result.successfulPathCount;
            const double inverseVoxels = 1.0
                / static_cast<double>(component.voxels);
            std::printf(
                "roi.minimal_path.c%zu.label=%u,status=ok,group_members=%zu,group_voxels=%zu,target_voxels=%zu,reference_precision=%.9g,primary_fraction=%.9g,secondary_fraction=%.9g,endpoint_distance_mm=%.9g,endpoint_alignment=%.9g,mean_vesselness=%.9g,max_vesselness=%.9g,ct_mean=%.9g,elongation=%.9g,score=%.9g,path_vertices=%zu\n",
                ordinal + 1, component.label, group.members.size(),
                group.totalVoxels, component.voxels,
                static_cast<double>(component.referenceVoxels)
                    * inverseVoxels,
                static_cast<double>(component.primaryPriorVoxels)
                    * inverseVoxels,
                static_cast<double>(component.secondaryPriorVoxels)
                    * inverseVoxels,
                component.minimumEndpointDistanceMm,
                component.endpointAlignment,
                component.vesselnessSum * inverseVoxels,
                component.vesselnessMaximum,
                component.ctSum * inverseVoxels,
                ranked.elongation, ranked.score, vertexCount);
        } catch (const itk::ExceptionObject&) {
            std::printf(
                "roi.minimal_path.c%zu.label=%u,status=itk_failure\n",
                ordinal + 1, component.label);
        }
    }

    std::vector<unsigned char> selectedRemoteValues(voxelCount, 0);
    for (std::size_t index = 0; index < voxelCount; ++index) {
        const std::uint32_t label = labelValues[index];
        if (label < successfulLabel.size() && successfulLabel[label]) {
            selectedRemoteValues[index] = 1;
        }
        if (pathValues[index] != 0) {
            ++result.pathVoxelCount;
        }
    }
    BinaryImage::Pointer selectedRemote =
        importImage<BinaryImage, BinaryImporter>(
            geometry, selectedRemoteValues.data(), voxelCount);
    BinaryImage::Pointer pathTube = dilatePhysical(pathMask, geometry, 1.0);
    BinaryImage::Pointer supportedTube = binaryAnd(pathTube, looseSupport);
    BinaryImage::Pointer bridge = binaryOr(pathMask, supportedTube);
    BinaryImage::Pointer rootAndRemote = binaryOr(root, selectedRemote);
    BinaryImage::Pointer connectedDomain = binaryOr(rootAndRemote, bridge);
    BinaryImage::Pointer bridged = reconstruct(root, connectedDomain);
    const unsigned char* bridgeOnlyValues = bridged->GetBufferPointer();
    result.bridgeOnlyValues.assign(bridgeOnlyValues,
                                   bridgeOnlyValues + voxelCount);

    constexpr double localRecoveryRadiusMm = 3.2;
    BinaryImage::Pointer localRecoveryFocus = binaryOr(selectedRemote, bridge);
    BinaryImage::Pointer localRecoveryNeighborhood = dilatePhysical(
        localRecoveryFocus, geometry, localRecoveryRadiusMm);
    BinaryImage::Pointer localRecoverySupport = binaryAnd(
        looseSupport, localRecoveryNeighborhood);
    BinaryImage::Pointer localRecoveryDomain = binaryOr(
        bridged, localRecoverySupport);
    BinaryImage::Pointer locallyRecovered = reconstruct(
        bridged, localRecoveryDomain);
    const unsigned char* locallyRecoveredValues =
        locallyRecovered->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (bridgeOnlyValues[index] == 0 && locallyRecoveredValues[index] != 0) {
            ++result.localRecoveryVoxelCount;
        }
    }
    std::printf(
        "roi.local_hysteresis.radius_mm=%.9g,vesselness_minimum=2,intensity_hu=80..300,added_voxels=%zu\n",
        localRecoveryRadiusMm, result.localRecoveryVoxelCount);

    Connected::Pointer finalConnected = Connected::New();
    finalConnected->SetInput(locallyRecovered);
    finalConnected->SetFullyConnected(true);
    Relabel::Pointer finalRelabel = Relabel::New();
    finalRelabel->SetInput(finalConnected->GetOutput());
    finalRelabel->Update();
    std::printf(
        "roi.minimal_path.successful_paths=%zu,path_voxels=%zu,final_components=%zu\n",
        result.successfulPathCount, result.pathVoxelCount,
        static_cast<std::size_t>(finalRelabel->GetNumberOfObjects()));
    result.values.assign(locallyRecoveredValues,
                         locallyRecoveredValues + voxelCount);
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 6 || argv == nullptr) {
        std::fprintf(
            stderr,
            "Usage: xq_vascular_roi_development_analyze <ct-dir> <gold-dir> <gold-foreground> <organ-roi.nii.gz> <coarse-vessel-roi.nii.gz> --baseline-artifact <legacy-v1.xqvmask> [--secondary-coarse <roi.nii.gz>] [--ct-series-uid <uid>] [--gold-series-uid <uid>]\n");
        return 1;
    }
    const double foregroundValue = std::strtod(argv[3], nullptr);
    std::string ctUid;
    std::string goldUid;
    std::string secondaryCoarsePath;
    std::string baselineArtifactPath;
    for (int index = 6; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--secondary-coarse" && index + 1 < argc) {
            secondaryCoarsePath = argv[++index];
        } else if (argument == "--baseline-artifact" && index + 1 < argc) {
            baselineArtifactPath = argv[++index];
        } else if (argument == "--ct-series-uid" && index + 1 < argc) {
            ctUid = argv[++index];
        } else if (argument == "--gold-series-uid" && index + 1 < argc) {
            goldUid = argv[++index];
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n",
                         argument.c_str());
            return 1;
        }
    }
    if (baselineArtifactPath.empty()) {
        std::fprintf(stderr, "Missing required --baseline-artifact\n");
        return 1;
    }

    xq::GdcmItkDicomSeriesReader imageReader;
    const xq::DicomSeriesDiscoveryResult imageDiscovery =
        imageReader.discover(argv[1]);
    if (!imageDiscovery.ok()) {
        return 2;
    }
    if (ctUid.empty()) {
        if (imageDiscovery.series.size() != 1) {
            return 3;
        }
        ctUid = imageDiscovery.series.front().identity.seriesInstanceUid;
    }
    const xq::DicomSeriesReadResult image = imageReader.read(argv[1], ctUid);
    if (!image.ok()) {
        return 4;
    }

    const xq::DicomSeriesDiscoveryResult goldDiscovery =
        imageReader.discover(argv[2]);
    if (!goldDiscovery.ok()) {
        return 5;
    }
    if (goldUid.empty()) {
        if (goldDiscovery.series.size() != 1) {
            return 6;
        }
        goldUid = goldDiscovery.series.front().identity.seriesInstanceUid;
    }
    xq::DicomBinaryLabelProfile labelProfile;
    labelProfile.backgroundValue = 0.0;
    labelProfile.foregroundValue = foregroundValue;
    labelProfile.outputLabel = 1;
    labelProfile.labelName = "development_gold";
    xq::GdcmItkDicomLabelReader labelReader;
    const xq::DicomLabelReadResult gold =
        labelReader.read(argv[2], goldUid, labelProfile);
    if (!gold.ok()
        || !sameGeometry(image.volume.geometry(), gold.mask->geometry())) {
        return 7;
    }

    const std::vector<xq::VascularRoiFileInput> roiInputs = {
        {xq::VascularRoiRole::Organ, argv[4], "TotalSegmentator", "2.15.0"},
        {xq::VascularRoiRole::CoarseVessel, argv[5], "TotalSegmentator", "2.15.0"}
    };
    xq::ItkVascularRoiPriorReader roiReader;
    const xq::VascularRoiPriorReadResult roi = roiReader.read(
        image.volume, image.descriptor.contentFingerprint, roiInputs);
    std::printf("roi.status=%s\n",
                xq::vascularRoiPriorReadStatusToken(roi.status));
    if (!roi.ok()) {
        return 8;
    }

    xq::VascularRoiPriorReadResult secondaryRoi;
    const xq::XQVascularRoiLayer* secondaryCoarse = nullptr;
    if (!secondaryCoarsePath.empty()) {
        const std::vector<xq::VascularRoiFileInput> secondaryInputs = {
            {xq::VascularRoiRole::Organ, argv[4], "TotalSegmentator", "2.15.0"},
            {xq::VascularRoiRole::CoarseVessel, secondaryCoarsePath,
             "TotalSegmentator.liver_vessels", "2.15.0"}
        };
        secondaryRoi = roiReader.read(image.volume,
                                      image.descriptor.contentFingerprint,
                                      secondaryInputs);
        std::printf("roi.secondary.status=%s\n",
                    xq::vascularRoiPriorReadStatusToken(secondaryRoi.status));
        if (!secondaryRoi.ok()) {
            return 9;
        }
        secondaryCoarse =
            secondaryRoi.prior->layer(xq::VascularRoiRole::CoarseVessel);
    }

    xq::ResidentVoxelSource source(image.buffer);
    xq::ItkVascularPreprocessor preprocessor;
    const xq::VascularPreprocessResult preprocessed = preprocessor.run(
        image.volume, source, xq::portalVenousCtPreprocessProfileV1());
    if (!preprocessed.ok()) {
        return 10;
    }
    const xq::AutomaticVesselSegmentationArtifactReadResult baselineArtifact =
        xq::readAutomaticVesselSegmentationArtifact(baselineArtifactPath);
    if (!baselineArtifact.ok() || !baselineArtifact.isLegacy()
        || !baselineArtifact.legacyProductionValid
        || !baselineArtifact.legacySegmentation.has_value()
        || !baselineArtifact.legacySegmentation->mask
        || !sameGeometry(image.volume.geometry(),
                         baselineArtifact.legacySegmentation->mask->geometry())
        || baselineArtifact.legacySegmentation->inputFingerprint
            != preprocessed.output->inputFingerprint
        || baselineArtifact.legacySegmentation->vesselnessFingerprint
            != preprocessed.output->outputFingerprint) {
        return 11;
    }
    const xq::XQSegmentationMask& baselineMask =
        *baselineArtifact.legacySegmentation->mask;

    xq::ItkVascularSegmentationEvaluator evaluator;
    printMetrics("baseline", evaluator.evaluate(baselineMask, *gold.mask));
    const xq::XQVascularRoiLayer* organ =
        roi.prior->layer(xq::VascularRoiRole::Organ);
    const xq::XQVascularRoiLayer* coarse =
        roi.prior->layer(xq::VascularRoiRole::CoarseVessel);
    printMetrics("roi.coarse_raw", evaluator.evaluate(*coarse->mask, *gold.mask));
    if (secondaryCoarse != nullptr) {
        printMetrics("roi.secondary_raw",
                     evaluator.evaluate(*secondaryCoarse->mask, *gold.mask));
    }

    const std::size_t voxelCount = image.buffer->voxelCount();
    std::vector<unsigned char> organValues(organ->mask->voxels().begin(),
                                           organ->mask->voxels().end());
    std::vector<unsigned char> coarseValues(coarse->mask->voxels().begin(),
                                            coarse->mask->voxels().end());
    std::vector<unsigned char> secondaryValues(voxelCount, 0);
    if (secondaryCoarse != nullptr) {
        secondaryValues.assign(secondaryCoarse->mask->voxels().begin(),
                               secondaryCoarse->mask->voxels().end());
    }
    std::vector<unsigned char> baselineValues(
        baselineMask.voxels().begin(), baselineMask.voxels().end());
    const std::vector<std::uint8_t>& imageBytes = image.buffer->bytes();
    const std::vector<std::uint8_t>& vesselnessBytes =
        preprocessed.output->buffer->bytes();
    FloatImage::Pointer itkCt = importImage<FloatImage, FloatImporter>(
        image.volume.geometry(),
        reinterpret_cast<float*>(
            const_cast<std::uint8_t*>(imageBytes.data())),
        voxelCount);
    FloatImage::Pointer itkVesselness = importImage<FloatImage, FloatImporter>(
        image.volume.geometry(),
        reinterpret_cast<float*>(
            const_cast<std::uint8_t*>(vesselnessBytes.data())),
        voxelCount);
    BinaryImage::Pointer itkOrgan = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(), organValues.data(), voxelCount);
    BinaryImage::Pointer itkCoarse = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(), coarseValues.data(), voxelCount);
    BinaryImage::Pointer itkSecondary = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(), secondaryValues.data(), voxelCount);
    BinaryImage::Pointer itkBaseline = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(), baselineValues.data(), voxelCount);

    const SkeletonEndpointSet baselineTopology = extractSkeletonEndpoints(
        itkBaseline, image.volume.geometry(), voxelCount);
    if (baselineTopology.endpointIndices.empty()) {
        return 12;
    }
    std::printf("baseline.skeleton_voxels=%zu,endpoints=%zu\n",
                baselineTopology.skeletonVoxelCount,
                baselineTopology.endpointIndices.size());

    BinaryImage::Pointer itkCombinedPrior = secondaryCoarse != nullptr
        ? binaryOr(itkCoarse, itkSecondary)
        : itkCoarse;
    if (secondaryCoarse != nullptr) {
        const xq::XQSegmentationMask combinedPriorMask = materialize(
            image.volume.geometry(),
            itkCombinedPrior->GetBufferPointer(), voxelCount);
        printMetrics("roi.combined_raw",
                     evaluator.evaluate(combinedPriorMask, *gold.mask));
    }

    std::size_t organCoarseOverlap = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (organValues[index] != 0 && coarseValues[index] != 0) {
            ++organCoarseOverlap;
        }
    }
    std::printf("roi.organ_coarse_overlap_voxels=%zu\n", organCoarseOverlap);

    constexpr double coarseDomainDilationMm = 2.0;
    BinaryImage::Pointer dilatedCoarseDomain = dilatePhysical(
        itkCombinedPrior, image.volume.geometry(), coarseDomainDilationMm);
    BinaryImage::Pointer itkRoiDomain = binaryOr(itkOrgan,
                                                 dilatedCoarseDomain);
    std::printf("roi.coarse_domain_dilation_mm=%.9g\n",
                coarseDomainDilationMm);

    // The completed threshold audit rejected every other combination. Keep the
    // topology/path work on the frozen case-1 gate instead of silently tuning
    // a second grid while evaluating the path kernel.
    const double gates[] = {5.0};
    const double intensityLowers[] = {120.0};
    const std::vector<xq::XQSegmentationMask::LabelType>& goldValues =
        gold.mask->voxels();
    std::vector<unsigned char> bestDiceValues;
    std::vector<unsigned char> bestRecallValues;
    double bestDice = -1.0;
    double bestDiceGate = 0.0;
    double bestDiceIntensity = 0.0;
    double bestRecall = -1.0;
    double bestRecallGate = 0.0;
    double bestRecallIntensity = 0.0;
    std::vector<unsigned char> policyBestDiceValues;
    std::vector<unsigned char> policyBestRecallValues;
    std::string policyBestDiceName;
    std::string policyBestRecallName;
    QuickMetrics policyBestDiceMetrics;
    QuickMetrics policyBestRecallMetrics;
    double policyBestDice = -1.0;
    double policyBestRecall = -1.0;
    std::vector<unsigned char> fixedTopologyValues;
    std::string fixedTopologyName;

    for (double gate : gates) {
        for (double intensityLower : intensityLowers) {
            BinaryImage::Pointer vesselnessGate = threshold(
                itkVesselness, gate, preprocessed.output->scalarMaximum);
            BinaryImage::Pointer intensityGate = threshold(
                itkCt, intensityLower, 300.0);
            BinaryImage::Pointer responseAndIntensity = binaryAnd(
                vesselnessGate, intensityGate);
            BinaryImage::Pointer allowed = binaryAnd(responseAndIntensity,
                                                      itkRoiDomain);
            BinaryImage::Pointer domain = binaryOr(allowed, itkBaseline);
            BinaryImage::Pointer reconstructed = reconstruct(itkBaseline,
                                                              domain);

            const unsigned char* values = reconstructed->GetBufferPointer();
            const QuickMetrics metrics = quickMetrics(
                values, voxelCount, goldValues, gold.foregroundVoxelCount);
            std::printf(
                "roi.grid.g%.9g.i%.9g.predicted=%zu,intersection=%zu,dice=%.9g,precision=%.9g,recall=%.9g\n",
                gate, intensityLower, metrics.predicted,
                metrics.intersection, metrics.dice, metrics.precision,
                metrics.recall);

            if (intensityLower == 120.0
                || ((gate == 3.0 || gate == 4.0 || gate == 5.0)
                    && (intensityLower == 80.0
                        || intensityLower == 100.0))) {
                char auditPrefix[96] = {};
                std::snprintf(auditPrefix, sizeof(auditPrefix),
                              "roi.addition_audit.g%.9g.i%.9g",
                              gate, intensityLower);
                const AdditionPolicyResult policy = analyzeAdditionComponents(
                    auditPrefix, reconstructed, itkBaseline, itkCoarse,
                    itkSecondary, itkVesselness,
                    baselineTopology.endpointDistance, goldValues,
                    gold.foregroundVoxelCount,
                    image.volume.geometry(), voxelCount, 0);
                if (gate == 5.0 && intensityLower == 120.0) {
                    fixedTopologyValues = policy.fixedTopologyValues;
                    fixedTopologyName = policy.fixedTopologyName;
                }
                if (policy.bestDiceMetrics.dice > policyBestDice) {
                    policyBestDice = policy.bestDiceMetrics.dice;
                    policyBestDiceValues = policy.bestDiceValues;
                    policyBestDiceName = policy.bestDiceName;
                    policyBestDiceMetrics = policy.bestDiceMetrics;
                }
                if (policy.bestRecallMetrics.dice >= 0.76
                    && policy.bestRecallMetrics.recall > policyBestRecall) {
                    policyBestRecall = policy.bestRecallMetrics.recall;
                    policyBestRecallValues = policy.bestRecallValues;
                    policyBestRecallName = policy.bestRecallName;
                    policyBestRecallMetrics = policy.bestRecallMetrics;
                }
            }

            if (metrics.dice > bestDice) {
                bestDice = metrics.dice;
                bestDiceGate = gate;
                bestDiceIntensity = intensityLower;
                bestDiceValues.assign(values, values + voxelCount);
            }
            if (metrics.dice >= 0.70 && metrics.recall > bestRecall) {
                bestRecall = metrics.recall;
                bestRecallGate = gate;
                bestRecallIntensity = intensityLower;
                bestRecallValues.assign(values, values + voxelCount);
            }
        }
    }

    if (bestDiceValues.size() != voxelCount
        || bestRecallValues.size() != voxelCount) {
        return 13;
    }
    const xq::XQSegmentationMask bestDiceMask = materialize(
        image.volume.geometry(), bestDiceValues.data(), voxelCount);
    char prefix[128] = {};
    std::snprintf(prefix, sizeof(prefix),
                  "roi.grid_best_dice.g%.9g.i%.9g",
                  bestDiceGate, bestDiceIntensity);
    printMetrics(prefix, evaluator.evaluate(bestDiceMask, *gold.mask));

    if (bestRecallGate != bestDiceGate
        || bestRecallIntensity != bestDiceIntensity) {
        const xq::XQSegmentationMask bestRecallMask = materialize(
            image.volume.geometry(), bestRecallValues.data(), voxelCount);
        std::snprintf(prefix, sizeof(prefix),
                      "roi.grid_best_recall.g%.9g.i%.9g",
                      bestRecallGate, bestRecallIntensity);
        printMetrics(prefix, evaluator.evaluate(bestRecallMask, *gold.mask));
    }

    if (policyBestDiceValues.size() != voxelCount
        || policyBestRecallValues.size() != voxelCount
        || fixedTopologyValues.size() != voxelCount) {
        return 14;
    }
    const xq::XQSegmentationMask policyBestDiceMask = materialize(
        image.volume.geometry(), policyBestDiceValues.data(), voxelCount);
    printMetrics((policyBestDiceName + ".full").c_str(),
                 evaluator.evaluate(policyBestDiceMask, *gold.mask));
    if (policyBestRecallName != policyBestDiceName) {
        const xq::XQSegmentationMask policyBestRecallMask = materialize(
            image.volume.geometry(), policyBestRecallValues.data(),
            voxelCount);
        printMetrics((policyBestRecallName + ".full").c_str(),
                     evaluator.evaluate(policyBestRecallMask, *gold.mask));
    }

    const xq::XQSegmentationMask fixedTopologyMask = materialize(
        image.volume.geometry(), fixedTopologyValues.data(), voxelCount);
    printMetrics((fixedTopologyName + ".frozen_full").c_str(),
                 evaluator.evaluate(fixedTopologyMask, *gold.mask));
    BinaryImage::Pointer fixedTopology =
        importImage<BinaryImage, BinaryImporter>(
            image.volume.geometry(), fixedTopologyValues.data(), voxelCount);
    const SkeletonEndpointSet fixedTopologyEndpoints =
        extractSkeletonEndpoints(fixedTopology, image.volume.geometry(),
                                 voxelCount);
    if (fixedTopologyEndpoints.endpointIndices.empty()) {
        return 15;
    }
    std::printf("roi.fixed_topology.skeleton_voxels=%zu,endpoints=%zu\n",
                fixedTopologyEndpoints.skeletonVoxelCount,
                fixedTopologyEndpoints.endpointIndices.size());

    constexpr double recoveryCandidateVesselnessMinimum = 5.0;
    constexpr double recoveryCandidateIntensityMinimumHu = 120.0;
    constexpr double recoveryCandidateIntensityMaximumHu = 300.0;
    std::printf(
        "roi.schema4_experiment2.root_gate=5,root_intensity_lower_hu=120,candidate_gate=%.9g,candidate_intensity_hu=%.9g..%.9g\n",
        recoveryCandidateVesselnessMinimum,
        recoveryCandidateIntensityMinimumHu,
        recoveryCandidateIntensityMaximumHu);
    BinaryImage::Pointer recoveryVesselnessGate = threshold(
        itkVesselness, recoveryCandidateVesselnessMinimum,
        preprocessed.output->scalarMaximum);
    BinaryImage::Pointer recoveryIntensityGate = threshold(
        itkCt, recoveryCandidateIntensityMinimumHu,
        recoveryCandidateIntensityMaximumHu);
    BinaryImage::Pointer recoveryResponseAndIntensity = binaryAnd(
        recoveryVesselnessGate, recoveryIntensityGate);
    BinaryImage::Pointer recoveryCandidate = binaryAnd(
        recoveryResponseAndIntensity, itkRoiDomain);
    const MinimalPathBridgeResult bridged = bridgeRemoteComponents(
        fixedTopology, recoveryCandidate, itkRoiDomain, itkCoarse, itkSecondary,
        itkCt, itkVesselness, fixedTopologyEndpoints, goldValues,
        image.volume.geometry(), voxelCount);
    if (bridged.bridgeOnlyValues.size() != voxelCount
        || bridged.corridorChainValues.size() != voxelCount
        || bridged.values.size() != voxelCount) {
        return 16;
    }

    std::vector<unsigned char> goldBinaryValues(voxelCount, 0);
    for (std::size_t index = 0; index < voxelCount; ++index) {
        goldBinaryValues[index] = goldValues[index] != 0 ? 1 : 0;
    }
    BinaryImage::Pointer itkGold = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(), goldBinaryValues.data(), voxelCount);
    BinaryImage::Pointer itkBridgeOnly = importImage<BinaryImage, BinaryImporter>(
        image.volume.geometry(),
        const_cast<unsigned char*>(bridged.bridgeOnlyValues.data()),
        voxelCount);

#if defined(XQ_DEVELOPMENT_TUBETK_ENDPOINT_RECOVERY)
    const xq::XQSegmentationMask tubeTkBridgeOnlyMask = materialize(
        image.volume.geometry(), bridged.bridgeOnlyValues.data(), voxelCount);
    const std::string tubeTkBridgePrefix =
        std::string(kTubeTkExperimentPrefix) + ".bridge_only.full";
    printMetrics(tubeTkBridgePrefix.c_str(),
                 evaluator.evaluate(tubeTkBridgeOnlyMask, *gold.mask));

    const SkeletonEndpointSet bridgeOnlyEndpoints = extractSkeletonEndpoints(
        itkBridgeOnly, image.volume.geometry(), voxelCount);
    std::printf(
        "%s.bridge_endpoints.skeleton_voxels=%zu,endpoints=%zu\n",
        kTubeTkExperimentPrefix, bridgeOnlyEndpoints.skeletonVoxelCount,
        bridgeOnlyEndpoints.endpointIndices.size());
    if (bridgeOnlyEndpoints.endpointIndices.empty()) {
        return 18;
    }

    const TubeTkEndpointRecoveryResult tubeTkRecovered =
        runTubeTkEndpointRecovery(
            itkCt, itkVesselness, itkOrgan, itkCoarse, itkRoiDomain,
            itkBridgeOnly, bridgeOnlyEndpoints, voxelCount);
    std::printf(
        "%s.tubetk_endpoint.status=%s,seeds=%zu,tubes=%zu,tube_points=%zu,working_raster_voxels=%zu,output_tube_voxels=%zu,added_voxels=%zu,final_components=%zu\n",
        kTubeTkExperimentPrefix, tubeTkRecovered.status.c_str(),
        tubeTkRecovered.clippedSeedCount,
        tubeTkRecovered.extractedTubeCount,
        tubeTkRecovered.extractedTubePointCount,
        tubeTkRecovered.rasterizedVoxelCount,
        tubeTkRecovered.clippedTubeVoxelCount,
        tubeTkRecovered.addedVoxelCount,
        tubeTkRecovered.finalComponentCount);
    if (!tubeTkRecovered.ok || tubeTkRecovered.values.size() != voxelCount) {
        return 19;
    }

    const xq::XQSegmentationMask tubeTkRecoveredMask = materialize(
        image.volume.geometry(), tubeTkRecovered.values.data(), voxelCount);
    const xq::VascularSegmentationEvaluationResult tubeTkEvaluation =
        evaluator.evaluate(tubeTkRecoveredMask, *gold.mask);
    const std::string tubeTkResultPrefix =
        std::string(kTubeTkExperimentPrefix) + ".tubetk_endpoint.full";
    printMetrics(tubeTkResultPrefix.c_str(), tubeTkEvaluation);
    if (!tubeTkEvaluation.ok()) {
        return 20;
    }
    const xq::VascularSegmentationMetrics& tubeTkMetrics =
        *tubeTkEvaluation.metrics;
    const bool allGatesPass = tubeTkMetrics.dice >= 0.70
        && tubeTkMetrics.hd95Mm <= 3.2
        && tubeTkMetrics.assdMm <= 1.2
        && tubeTkMetrics.clDice >= 0.75
        && tubeTkMetrics.largestPredictionComponentFraction >= 0.90;
    std::printf(
        "%s.acceptance=%s,dice_gate=%d,hd95_gate=%d,assd_gate=%d,cldice_gate=%d,largest_component_gate=%d\n",
        kTubeTkExperimentPrefix,
        allGatesPass ? "accepted" : "rejected",
        tubeTkMetrics.dice >= 0.70 ? 1 : 0,
        tubeTkMetrics.hd95Mm <= 3.2 ? 1 : 0,
        tubeTkMetrics.assdMm <= 1.2 ? 1 : 0,
        tubeTkMetrics.clDice >= 0.75 ? 1 : 0,
        tubeTkMetrics.largestPredictionComponentFraction >= 0.90 ? 1 : 0);
    return 0;
#endif

    BinaryImage::Pointer unclippedTrustedDomain = binaryOr(
        itkBridgeOnly, recoveryResponseAndIntensity);
    BinaryImage::Pointer unclippedTrusted = reconstruct(
        itkBridgeOnly, unclippedTrustedDomain);
    std::size_t unclippedTrustedAddedVoxels = 0;
    const unsigned char* unclippedTrustedValues =
        unclippedTrusted->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (bridged.bridgeOnlyValues[index] == 0
            && unclippedTrustedValues[index] != 0) {
            ++unclippedTrustedAddedVoxels;
        }
    }
    std::printf(
        "roi.unclipped_trusted.vesselness_minimum=5,ct_hu=120..300,added_voxels=%zu\n",
        unclippedTrustedAddedVoxels);

    const xq::VascularPreprocessProfileV1 preprocessProfile =
        xq::portalVenousCtPreprocessProfileV1();
    FloatImage::Pointer satoResponse = computeSatoResponse(
        itkCt, preprocessProfile);
    const float* satoResponseValues = satoResponse->GetBufferPointer();
    float satoMaximum = 0.0f;
    std::size_t satoPositiveVoxels = 0;
    bool satoResponseFinite = true;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        const float value = satoResponseValues[index];
        if (!std::isfinite(value)) {
            satoResponseFinite = false;
            break;
        }
        if (value > 0.0f) {
            ++satoPositiveVoxels;
            satoMaximum = (std::max)(satoMaximum, value);
        }
    }
    std::printf("roi.sato.positive_voxels=%zu,maximum=%.9g\n",
                satoPositiveVoxels, static_cast<double>(satoMaximum));
    if (!satoResponseFinite || satoPositiveVoxels == 0
        || !(satoMaximum > 0.0f)) {
        return 17;
    }
    BinaryImage::Pointer satoPositive = threshold(
        satoResponse, std::numeric_limits<float>::denorm_min(),
        satoMaximum);
    BinaryImage::Pointer satoTrustedSupport = binaryAnd(
        recoveryResponseAndIntensity, satoPositive);
    BinaryImage::Pointer satoConsensusDomain = binaryOr(
        itkBridgeOnly, satoTrustedSupport);
    BinaryImage::Pointer satoConsensus = reconstruct(
        itkBridgeOnly, satoConsensusDomain);
    const unsigned char* satoConsensusBuffer =
        satoConsensus->GetBufferPointer();
    std::vector<unsigned char> satoConsensusValues(
        satoConsensusBuffer, satoConsensusBuffer + voxelCount);
    std::size_t satoConsensusAddedVoxels = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (bridged.bridgeOnlyValues[index] == 0
            && satoConsensusValues[index] != 0) {
            ++satoConsensusAddedVoxels;
        }
    }
    std::printf(
        "roi.sato_consensus.frangi_vesselness_minimum=5,ct_hu=120..300,sato_minimum_exclusive=0,added_voxels=%zu\n",
        satoConsensusAddedVoxels);
    satoConsensus = nullptr;
    satoConsensusDomain = nullptr;
    satoTrustedSupport = nullptr;
    satoPositive = nullptr;
    satoResponse = nullptr;

    BinaryImage::Pointer levelSetCtDomain = threshold(itkCt, 80.0, 300.0);
    FloatMask::Pointer levelSetFeature = FloatMask::New();
    levelSetFeature->SetInput(itkVesselness);
    levelSetFeature->SetMaskImage(levelSetCtDomain);
    levelSetFeature->SetOutsideValue(0.0f);

    constexpr float levelSetLowerThreshold = 2.0f;
    constexpr unsigned int levelSetIterations = 80;
    constexpr double levelSetCurvatureScaling = 0.25;
    const LevelSetProbeResult bridgeSeedLevelSet = runThresholdLevelSet(
        "roi.threshold_level_set.bridge_seed", itkBridgeOnly,
        levelSetFeature->GetOutput(), itkBridgeOnly, levelSetLowerThreshold,
        static_cast<float>(preprocessed.output->scalarMaximum),
        levelSetIterations, levelSetCurvatureScaling, voxelCount);
    BinaryImage::Pointer levelSetResponseDomain = threshold(
        itkVesselness, levelSetLowerThreshold,
        preprocessed.output->scalarMaximum);
    BinaryImage::Pointer unclippedLooseSupport = binaryAnd(
        levelSetCtDomain, levelSetResponseDomain);
    BinaryImage::Pointer unclippedHysteresisDomain = binaryOr(
        itkBridgeOnly, unclippedLooseSupport);
    BinaryImage::Pointer unclippedHysteresis = reconstruct(
        itkBridgeOnly, unclippedHysteresisDomain);
    std::size_t unclippedHysteresisAddedVoxels = 0;
    const unsigned char* unclippedHysteresisValues =
        unclippedHysteresis->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (bridged.bridgeOnlyValues[index] == 0
            && unclippedHysteresisValues[index] != 0) {
            ++unclippedHysteresisAddedVoxels;
        }
    }
    std::printf(
        "roi.unclipped_hysteresis.vesselness_minimum=2,ct_hu=80..300,added_voxels=%zu\n",
        unclippedHysteresisAddedVoxels);
    BinaryImage::Pointer gatedPrimaryPrior = binaryAnd(
        itkCoarse, binaryAnd(levelSetCtDomain, levelSetResponseDomain));
    BinaryImage::Pointer bridgeAndPrimarySeed = binaryOr(
        itkBridgeOnly, gatedPrimaryPrior);
    std::size_t gatedPrimarySeedVoxels = 0;
    const unsigned char* gatedPrimaryValues =
        gatedPrimaryPrior->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (gatedPrimaryValues[index] != 0) {
            ++gatedPrimarySeedVoxels;
        }
    }
    std::printf("roi.threshold_level_set.gated_primary_seed_voxels=%zu\n",
                gatedPrimarySeedVoxels);
    const LevelSetProbeResult coarseSeedLevelSet = runThresholdLevelSet(
        "roi.threshold_level_set.coarse_seed", bridgeAndPrimarySeed,
        levelSetFeature->GetOutput(), itkBridgeOnly, levelSetLowerThreshold,
        static_cast<float>(preprocessed.output->scalarMaximum),
        levelSetIterations, levelSetCurvatureScaling, voxelCount);

    const auto makeContour = [](BinaryImage* mask) {
        Contour::Pointer contour = Contour::New();
        contour->SetInput(mask);
        contour->SetForegroundValue(1);
        contour->SetBackgroundValue(0);
        contour->SetFullyConnected(false);
        contour->Update();
        BinaryImage::Pointer output = contour->GetOutput();
        output->DisconnectPipeline();
        return output;
    };
    BinaryImage::Pointer goldSurface = makeContour(itkGold);
    BinaryImage::Pointer predictionSurface = makeContour(itkBridgeOnly);
    Distance::Pointer distanceToPrediction = Distance::New();
    distanceToPrediction->SetInput(predictionSurface);
    distanceToPrediction->SetUseImageSpacing(true);
    distanceToPrediction->SetSquaredDistance(false);
    distanceToPrediction->SetInsideIsPositive(false);
    distanceToPrediction->Update();
    FloatImage::Pointer predictionDistance =
        distanceToPrediction->GetOutput();
    predictionDistance->DisconnectPipeline();
    const auto makeSignedDistance = [](BinaryImage* mask) {
        Distance::Pointer distance = Distance::New();
        distance->SetInput(mask);
        distance->SetUseImageSpacing(true);
        distance->SetSquaredDistance(false);
        distance->SetInsideIsPositive(false);
        distance->Update();
        FloatImage::Pointer output = distance->GetOutput();
        output->DisconnectPipeline();
        return output;
    };
    FloatImage::Pointer organDistance = makeSignedDistance(itkOrgan);
    FloatImage::Pointer combinedPriorDistance = makeSignedDistance(
        itkCombinedPrior);
    constexpr double distantReferenceThresholdMm = 3.2;
    BinaryImage::Pointer distantGate = threshold(
        predictionDistance, distantReferenceThresholdMm,
        (std::numeric_limits<float>::max)());
    BinaryImage::Pointer distantReferenceSurface = binaryAnd(
        goldSurface, distantGate);
    Connected::Pointer distantConnected = Connected::New();
    distantConnected->SetInput(distantReferenceSurface);
    distantConnected->SetFullyConnected(true);
    Relabel::Pointer distantRelabel = Relabel::New();
    distantRelabel->SetInput(distantConnected->GetOutput());
    distantRelabel->Update();
    LabelImage::Pointer distantLabels = distantRelabel->GetOutput();
    distantLabels->DisconnectPipeline();

    struct DistantReferenceComponent {
        std::size_t voxels = 0;
        std::size_t organVoxels = 0;
        std::size_t coarseVoxels = 0;
        std::size_t secondaryVoxels = 0;
        std::size_t roiVoxels = 0;
        std::size_t looseSupportVoxels = 0;
        double ctSum = 0.0;
        double vesselnessSum = 0.0;
        double vesselnessMaximum = 0.0;
        double distanceSum = 0.0;
        double distanceMaximum = 0.0;
        double organDistanceSum = 0.0;
        double organDistanceMinimum =
            (std::numeric_limits<double>::infinity)();
        double organDistanceMaximum = 0.0;
        double priorDistanceSum = 0.0;
        double priorDistanceMinimum =
            (std::numeric_limits<double>::infinity)();
        double priorDistanceMaximum = 0.0;
        double indexSum[3] = {0.0, 0.0, 0.0};
        int minimumIndex[3] = {
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)()
        };
        int maximumIndex[3] = {
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)(),
            (std::numeric_limits<int>::min)()
        };
    };
    const std::size_t distantComponentCount =
        distantRelabel->GetNumberOfObjects();
    std::vector<DistantReferenceComponent> distantComponents(
        distantComponentCount + 1);
    const unsigned char* goldSurfaceValues =
        goldSurface->GetBufferPointer();
    const unsigned char* distantValues =
        distantReferenceSurface->GetBufferPointer();
    const std::uint32_t* distantLabelValues =
        distantLabels->GetBufferPointer();
    const unsigned char* roiDomainValues =
        itkRoiDomain->GetBufferPointer();
    const float* ctScalarValues = itkCt->GetBufferPointer();
    const float* vesselnessScalarValues = itkVesselness->GetBufferPointer();
    const float* predictionDistanceValues =
        predictionDistance->GetBufferPointer();
    const float* organDistanceValues = organDistance->GetBufferPointer();
    const float* combinedPriorDistanceValues =
        combinedPriorDistance->GetBufferPointer();
    std::vector<double> distantOrganDistances;
    std::vector<double> distantPriorDistances;
    distantOrganDistances.reserve(6000);
    distantPriorDistances.reserve(6000);
    std::size_t referenceSurfaceVoxelCount = 0;
    std::size_t distantReferenceVoxelCount = 0;
    std::size_t distantReferenceInRoiCount = 0;
    std::size_t distantReferenceLooseSupportCount = 0;
    std::size_t auditFlat = 0;
    for (int z = 0; z < image.volume.geometry().dimensions[2]; ++z) {
        for (int y = 0; y < image.volume.geometry().dimensions[1]; ++y) {
            for (int x = 0; x < image.volume.geometry().dimensions[0];
                 ++x, ++auditFlat) {
                if (goldSurfaceValues[auditFlat] != 0) {
                    ++referenceSurfaceVoxelCount;
                }
                if (distantValues[auditFlat] == 0) {
                    continue;
                }
                ++distantReferenceVoxelCount;
                const bool inRoi = roiDomainValues[auditFlat] != 0;
                const bool inLooseSupport = inRoi
                    && ctScalarValues[auditFlat] >= 80.0f
                    && ctScalarValues[auditFlat] <= 300.0f
                    && vesselnessScalarValues[auditFlat] >= 2.0f;
                if (inRoi) {
                    ++distantReferenceInRoiCount;
                }
                if (inLooseSupport) {
                    ++distantReferenceLooseSupportCount;
                }
                const double distanceFromOrgan = (std::max)(
                    0.0, static_cast<double>(organDistanceValues[auditFlat]));
                const double distanceFromPrior = (std::max)(
                    0.0,
                    static_cast<double>(
                        combinedPriorDistanceValues[auditFlat]));
                distantOrganDistances.push_back(distanceFromOrgan);
                distantPriorDistances.push_back(distanceFromPrior);
                const std::uint32_t label = distantLabelValues[auditFlat];
                if (label == 0 || label > distantComponentCount) {
                    continue;
                }
                DistantReferenceComponent& component =
                    distantComponents[label];
                ++component.voxels;
                if (organValues[auditFlat] != 0) {
                    ++component.organVoxels;
                }
                if (coarseValues[auditFlat] != 0) {
                    ++component.coarseVoxels;
                }
                if (secondaryValues[auditFlat] != 0) {
                    ++component.secondaryVoxels;
                }
                if (inRoi) {
                    ++component.roiVoxels;
                }
                if (inLooseSupport) {
                    ++component.looseSupportVoxels;
                }
                component.ctSum += ctScalarValues[auditFlat];
                component.vesselnessSum +=
                    vesselnessScalarValues[auditFlat];
                component.vesselnessMaximum = (std::max)(
                    component.vesselnessMaximum,
                    static_cast<double>(
                        vesselnessScalarValues[auditFlat]));
                component.distanceSum +=
                    predictionDistanceValues[auditFlat];
                component.distanceMaximum = (std::max)(
                    component.distanceMaximum,
                    static_cast<double>(
                        predictionDistanceValues[auditFlat]));
                component.organDistanceSum += distanceFromOrgan;
                component.organDistanceMinimum = (std::min)(
                    component.organDistanceMinimum, distanceFromOrgan);
                component.organDistanceMaximum = (std::max)(
                    component.organDistanceMaximum, distanceFromOrgan);
                component.priorDistanceSum += distanceFromPrior;
                component.priorDistanceMinimum = (std::min)(
                    component.priorDistanceMinimum, distanceFromPrior);
                component.priorDistanceMaximum = (std::max)(
                    component.priorDistanceMaximum, distanceFromPrior);
                const int coordinates[3] = {x, y, z};
                for (int axis = 0; axis < 3; ++axis) {
                    component.indexSum[axis] += coordinates[axis];
                    component.minimumIndex[axis] = (std::min)(
                        component.minimumIndex[axis], coordinates[axis]);
                    component.maximumIndex[axis] = (std::max)(
                        component.maximumIndex[axis], coordinates[axis]);
                }
            }
        }
    }
    std::printf(
        "roi.distant_reference.threshold_mm=%.9g,reference_surface_voxels=%zu,distant_surface_voxels=%zu,distant_fraction=%.9g,in_allowed_roi=%zu,in_allowed_roi_fraction=%.9g,in_loose_support=%zu,in_loose_support_fraction=%.9g,components=%zu\n",
        distantReferenceThresholdMm, referenceSurfaceVoxelCount,
        distantReferenceVoxelCount,
        referenceSurfaceVoxelCount == 0 ? 0.0
            : static_cast<double>(distantReferenceVoxelCount)
                / static_cast<double>(referenceSurfaceVoxelCount),
        distantReferenceInRoiCount,
        distantReferenceVoxelCount == 0 ? 0.0
            : static_cast<double>(distantReferenceInRoiCount)
                / static_cast<double>(distantReferenceVoxelCount),
        distantReferenceLooseSupportCount,
        distantReferenceVoxelCount == 0 ? 0.0
            : static_cast<double>(distantReferenceLooseSupportCount)
                / static_cast<double>(distantReferenceVoxelCount),
        distantComponentCount);
    const auto reportDistanceQuantiles = [](const char* prefix,
                                            std::vector<double> values) {
        if (values.empty()) {
            return;
        }
        std::sort(values.begin(), values.end());
        const auto percentile = [&](double probability) {
            const std::size_t index = (std::min)(
                values.size() - 1,
                static_cast<std::size_t>(std::ceil(
                    probability * static_cast<double>(values.size()))) - 1);
            return values[index];
        };
        std::printf(
            "%s.p50_mm=%.9g,p75_mm=%.9g,p90_mm=%.9g,p95_mm=%.9g,max_mm=%.9g\n",
            prefix, percentile(0.50), percentile(0.75), percentile(0.90),
            percentile(0.95), values.back());
    };
    reportDistanceQuantiles("roi.distant_reference.organ_distance",
                            distantOrganDistances);
    reportDistanceQuantiles("roi.distant_reference.prior_distance",
                            distantPriorDistances);
    const std::size_t distantReportCount = (std::min)(
        static_cast<std::size_t>(30), distantComponentCount);
    for (std::size_t label = 1; label <= distantReportCount; ++label) {
        const DistantReferenceComponent& component =
            distantComponents[label];
        if (component.voxels == 0) {
            continue;
        }
        const double inverseVoxels = 1.0
            / static_cast<double>(component.voxels);
        itk::ContinuousIndex<double, kDimension> centroidIndex;
        for (int axis = 0; axis < 3; ++axis) {
            centroidIndex[axis] = component.indexSum[axis] * inverseVoxels;
        }
        BinaryImage::PointType centroidPoint;
        itkGold->TransformContinuousIndexToPhysicalPoint(
            centroidIndex, centroidPoint);
        std::printf(
            "roi.distant_reference.c%zu.surface_voxels=%zu,centroid_lps_mm=%.9g:%.9g:%.9g,index_bounds=%d:%d:%d..%d:%d:%d,mean_distance_mm=%.9g,max_distance_mm=%.9g,organ_distance_mm=%.9g..%.9g..%.9g,prior_distance_mm=%.9g..%.9g..%.9g,mean_ct=%.9g,mean_vesselness=%.9g,max_vesselness=%.9g,organ_fraction=%.9g,coarse_fraction=%.9g,secondary_fraction=%.9g,allowed_roi_fraction=%.9g,loose_support_fraction=%.9g\n",
            label, component.voxels,
            centroidPoint[0], centroidPoint[1], centroidPoint[2],
            component.minimumIndex[0], component.minimumIndex[1],
            component.minimumIndex[2], component.maximumIndex[0],
            component.maximumIndex[1], component.maximumIndex[2],
            component.distanceSum * inverseVoxels,
            component.distanceMaximum,
            component.organDistanceMinimum,
            component.organDistanceSum * inverseVoxels,
            component.organDistanceMaximum,
            component.priorDistanceMinimum,
            component.priorDistanceSum * inverseVoxels,
            component.priorDistanceMaximum,
            component.ctSum * inverseVoxels,
            component.vesselnessSum * inverseVoxels,
            component.vesselnessMaximum,
            static_cast<double>(component.organVoxels) * inverseVoxels,
            static_cast<double>(component.coarseVoxels) * inverseVoxels,
            static_cast<double>(component.secondaryVoxels) * inverseVoxels,
            static_cast<double>(component.roiVoxels) * inverseVoxels,
            static_cast<double>(component.looseSupportVoxels) * inverseVoxels);
    }

    const xq::XQSegmentationMask bridgeOnlyMask = materialize(
        image.volume.geometry(), bridged.bridgeOnlyValues.data(), voxelCount);
    printMetrics("roi.schema4_experiment2.bridge_only.full",
                 evaluator.evaluate(bridgeOnlyMask, *gold.mask));
    const xq::XQSegmentationMask thresholdLevelSetMask = materialize(
        image.volume.geometry(), bridgeSeedLevelSet.values.data(), voxelCount);
    printMetrics("roi.schema4_experiment4.threshold_level_set.full",
                 evaluator.evaluate(thresholdLevelSetMask, *gold.mask));
    const xq::XQSegmentationMask coarseSeedLevelSetMask = materialize(
        image.volume.geometry(), coarseSeedLevelSet.values.data(), voxelCount);
    printMetrics("roi.schema4_experiment5.coarse_seed_level_set.full",
                 evaluator.evaluate(coarseSeedLevelSetMask, *gold.mask));
    const xq::XQSegmentationMask unclippedHysteresisMask = materialize(
        image.volume.geometry(), unclippedHysteresisValues, voxelCount);
    printMetrics("roi.schema4_experiment6.unclipped_hysteresis.full",
                 evaluator.evaluate(unclippedHysteresisMask, *gold.mask));
    const xq::XQSegmentationMask unclippedTrustedMask = materialize(
        image.volume.geometry(), unclippedTrustedValues, voxelCount);
    printMetrics("roi.schema4_experiment7.unclipped_trusted.full",
                 evaluator.evaluate(unclippedTrustedMask, *gold.mask));
    const xq::XQSegmentationMask satoConsensusMask = materialize(
        image.volume.geometry(), satoConsensusValues.data(), voxelCount);
    printMetrics("roi.schema4_experiment8.sato_consensus.full",
                 evaluator.evaluate(satoConsensusMask, *gold.mask));
    const xq::XQSegmentationMask corridorChainMask = materialize(
        image.volume.geometry(), bridged.corridorChainValues.data(),
        voxelCount);
    printMetrics("roi.schema4_experiment3.corridor_chain.full",
                 evaluator.evaluate(corridorChainMask, *gold.mask));
    const xq::XQSegmentationMask locallyRecoveredMask = materialize(
        image.volume.geometry(), bridged.values.data(), voxelCount);
    printMetrics("roi.schema4_experiment2.local_hysteresis.full",
                 evaluator.evaluate(locallyRecoveredMask, *gold.mask));

    return 0;
}
