#include "adapters/itk/ItkVascularSegmentationEvaluator.h"

#include "itkBinaryContourImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkImage.h"
#include "itkImportImageFilter.h"
#include "itkMacro.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkVersion.h"

#include "itkBinaryThinningImageFilter3D.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace xq {
namespace {

constexpr unsigned int kDimension = 3;
constexpr double kGeometryTolerance = 1e-12;

using BinaryImage = itk::Image<unsigned char, kDimension>;
using FloatImage = itk::Image<float, kDimension>;
using LabelImage = itk::Image<std::uint32_t, kDimension>;
using BinaryImporter = itk::ImportImageFilter<unsigned char, kDimension>;
using ConnectedFilter = itk::ConnectedComponentImageFilter<BinaryImage, LabelImage>;
using RelabelFilter = itk::RelabelComponentImageFilter<LabelImage, LabelImage>;
using ContourFilter = itk::BinaryContourImageFilter<BinaryImage, BinaryImage>;
using DistanceFilter =
    itk::SignedMaurerDistanceMapImageFilter<BinaryImage, FloatImage>;
using ThinningFilter = itk::BinaryThinningImageFilter3D<BinaryImage, BinaryImage>;
using Clock = std::chrono::steady_clock;

double elapsedMilliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void addDiagnostic(VascularSegmentationEvaluationResult* result,
                   DiagnosticSeverity severity,
                   VascularSegmentationEvaluationDiagnosticCode code,
                   VascularSegmentationEvaluationStage stage,
                   std::initializer_list<double> numericContext = {})
{
    if (result == nullptr) {
        return;
    }
    VascularSegmentationEvaluationDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.stage = stage;
    diagnostic.messageKey = vascularSegmentationEvaluationDiagnosticToken(code);
    diagnostic.numericContext.assign(numericContext.begin(), numericContext.end());
    result->diagnostics.push_back(std::move(diagnostic));
}

VascularSegmentationEvaluationResult failure(
    Clock::time_point start,
    VascularSegmentationEvaluationStatus status,
    VascularSegmentationEvaluationStage stage,
    VascularSegmentationEvaluationDiagnosticCode code,
    std::initializer_list<double> numericContext = {})
{
    VascularSegmentationEvaluationResult result;
    result.status = status;
    result.stage = stage;
    result.elapsedMilliseconds = elapsedMilliseconds(start);
    addDiagnostic(&result, DiagnosticSeverity::Error, code, stage, numericContext);
    return result;
}

double nearlyEqualScale(double left, double right)
{
    return (std::max)({1.0, std::abs(left), std::abs(right)});
}

bool nearlyEqual(double left, double right)
{
    return std::abs(left - right)
        <= kGeometryTolerance * nearlyEqualScale(left, right);
}

bool sameGeometry(const ImageGeometry& left, const ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || !nearlyEqual(left.spacing[axis], right.spacing[axis])
            || !nearlyEqual(left.origin[axis], right.origin[axis])) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (!nearlyEqual(left.direction[axis][column],
                             right.direction[axis][column])) {
                return false;
            }
        }
    }
    return true;
}

BinaryImage::Pointer importMask(const XQSegmentationMask& mask)
{
    const ImageGeometry& geometry = mask.geometry();
    BinaryImage::SizeType size;
    BinaryImage::IndexType start;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        size[axis] = static_cast<BinaryImage::SizeType::SizeValueType>(
            geometry.dimensions[axis]);
        start[axis] = 0;
    }
    BinaryImage::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    BinaryImporter::Pointer importer = BinaryImporter::New();
    importer->SetRegion(region);
    BinaryImage::SpacingType spacing;
    BinaryImage::PointType origin;
    BinaryImage::DirectionType direction;
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
    const std::vector<XQSegmentationMask::LabelType>& voxels = mask.voxels();
    importer->SetImportPointer(
        const_cast<unsigned char*>(voxels.data()), voxels.size(), false);
    importer->Update();
    BinaryImage::Pointer image = importer->GetOutput();
    image->DisconnectPipeline();
    return image;
}

bool validateLabels(const XQSegmentationMask& mask, std::size_t* foreground)
{
    if (foreground == nullptr) {
        return false;
    }
    *foreground = 0;
    for (XQSegmentationMask::LabelType value : mask.voxels()) {
        if (value > 1) {
            return false;
        }
        if (value != 0) {
            ++(*foreground);
        }
    }
    return true;
}

double percentile95(std::vector<double>* values)
{
    if (values == nullptr || values->empty()) {
        return 0.0;
    }
    std::sort(values->begin(), values->end());
    const std::size_t index = (std::min)(
        values->size() - 1,
        static_cast<std::size_t>(std::ceil(
            0.95 * static_cast<double>(values->size()))) - 1);
    return (*values)[index];
}

BinaryImage::Pointer makeSurface(BinaryImage* mask)
{
    ContourFilter::Pointer contour = ContourFilter::New();
    contour->SetInput(mask);
    contour->SetForegroundValue(1);
    contour->SetBackgroundValue(0);
    contour->SetFullyConnected(false);
    contour->Update();
    BinaryImage::Pointer surface = contour->GetOutput();
    surface->DisconnectPipeline();
    return surface;
}

FloatImage::Pointer makeSurfaceDistance(BinaryImage* surface)
{
    DistanceFilter::Pointer distance = DistanceFilter::New();
    distance->SetInput(surface);
    distance->SetUseImageSpacing(true);
    distance->SetSquaredDistance(false);
    distance->SetInsideIsPositive(false);
    distance->Update();
    FloatImage::Pointer output = distance->GetOutput();
    output->DisconnectPipeline();
    return output;
}

std::size_t countForeground(const BinaryImage* image)
{
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        return 0;
    }
    const std::size_t count =
        image->GetLargestPossibleRegion().GetNumberOfPixels();
    const unsigned char* values = image->GetBufferPointer();
    std::size_t foreground = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (values[index] != 0) {
            ++foreground;
        }
    }
    return foreground;
}

} // namespace

const char* ItkVascularSegmentationEvaluator::evaluatorId()
{
    return "xq.itk.vascular-segmentation-evaluator";
}

const char* ItkVascularSegmentationEvaluator::evaluatorVersion()
{
    return "1.0.0";
}

const char* ItkVascularSegmentationEvaluator::thinningBackendId()
{
    return "ITKThickness3D.BinaryThinningImageFilter3D";
}

const char* ItkVascularSegmentationEvaluator::thinningBackendVersion()
{
    return "v5.3.0@36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb";
}

VascularSegmentationEvaluationResult ItkVascularSegmentationEvaluator::evaluate(
    const XQSegmentationMask& prediction,
    const XQSegmentationMask& reference) const
{
    const Clock::time_point start = Clock::now();
    VascularSegmentationEvaluationStage currentStage =
        VascularSegmentationEvaluationStage::ValidateInput;
    try {
        if (!prediction.is_valid() || !prediction.hasGeometry()) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::InvalidPrediction,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::InvalidPrediction);
        }
        if (!reference.is_valid() || !reference.hasGeometry()) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::InvalidReference,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::InvalidReference);
        }
        if (!sameGeometry(prediction.geometry(), reference.geometry())
            || prediction.voxelCount() != reference.voxelCount()) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::GeometryMismatch,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::GeometryMismatch);
        }
        std::size_t predictionForeground = 0;
        std::size_t referenceForeground = 0;
        if (!validateLabels(prediction, &predictionForeground)) {
            return failure(
                start, VascularSegmentationEvaluationStatus::InvalidPrediction,
                currentStage,
                VascularSegmentationEvaluationDiagnosticCode::UnsupportedLabelValue);
        }
        if (!validateLabels(reference, &referenceForeground)) {
            return failure(
                start, VascularSegmentationEvaluationStatus::InvalidReference,
                currentStage,
                VascularSegmentationEvaluationDiagnosticCode::UnsupportedLabelValue);
        }
        if (predictionForeground == 0) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::EmptyPrediction,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::EmptyPrediction);
        }
        if (referenceForeground == 0) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::EmptyReference,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::EmptyReference);
        }

        currentStage = VascularSegmentationEvaluationStage::Overlap;
        std::size_t intersection = 0;
        const std::vector<XQSegmentationMask::LabelType>& predictionVoxels =
            prediction.voxels();
        const std::vector<XQSegmentationMask::LabelType>& referenceVoxels =
            reference.voxels();
        for (std::size_t index = 0; index < predictionVoxels.size(); ++index) {
            if (predictionVoxels[index] != 0 && referenceVoxels[index] != 0) {
                ++intersection;
            }
        }
        const double dice = 2.0 * static_cast<double>(intersection)
            / static_cast<double>(predictionForeground + referenceForeground);

        BinaryImage::Pointer predictionImage = importMask(prediction);
        BinaryImage::Pointer referenceImage = importMask(reference);

        currentStage = VascularSegmentationEvaluationStage::ConnectedComponents;
        ConnectedFilter::Pointer connected = ConnectedFilter::New();
        connected->SetInput(predictionImage);
        connected->SetFullyConnected(true);
        RelabelFilter::Pointer relabel = RelabelFilter::New();
        relabel->SetInput(connected->GetOutput());
        relabel->Update();
        const std::size_t componentCount = relabel->GetNumberOfObjects();
        if (componentCount == 0) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::ProcessingFailed,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::EmptyPrediction);
        }
        const LabelImage* relabeled = relabel->GetOutput();
        const std::uint32_t* labels = relabeled->GetBufferPointer();
        std::size_t largestComponent = 0;
        for (std::size_t index = 0; index < prediction.voxelCount(); ++index) {
            if (labels[index] == 1) {
                ++largestComponent;
            }
        }
        const double largestComponentFraction =
            static_cast<double>(largestComponent)
            / static_cast<double>(predictionForeground);

        currentStage = VascularSegmentationEvaluationStage::SurfaceDistance;
        BinaryImage::Pointer predictionSurface = makeSurface(predictionImage);
        BinaryImage::Pointer referenceSurface = makeSurface(referenceImage);
        const std::size_t predictionSurfaceCount =
            countForeground(predictionSurface);
        const std::size_t referenceSurfaceCount = countForeground(referenceSurface);
        if (predictionSurfaceCount == 0 || referenceSurfaceCount == 0) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::ProcessingFailed,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::EmptySurface);
        }
        FloatImage::Pointer distanceToPrediction =
            makeSurfaceDistance(predictionSurface);
        FloatImage::Pointer distanceToReference =
            makeSurfaceDistance(referenceSurface);
        const unsigned char* predictionSurfaceValues =
            predictionSurface->GetBufferPointer();
        const unsigned char* referenceSurfaceValues =
            referenceSurface->GetBufferPointer();
        const float* predictionDistances =
            distanceToPrediction->GetBufferPointer();
        const float* referenceDistances =
            distanceToReference->GetBufferPointer();
        std::vector<double> predictionToReference;
        std::vector<double> referenceToPrediction;
        predictionToReference.reserve(predictionSurfaceCount);
        referenceToPrediction.reserve(referenceSurfaceCount);
        double symmetricDistanceSum = 0.0;
        for (std::size_t index = 0; index < prediction.voxelCount(); ++index) {
            if (predictionSurfaceValues[index] != 0) {
                const double distance =
                    std::abs(static_cast<double>(referenceDistances[index]));
                if (!std::isfinite(distance)) {
                    return failure(
                        start,
                        VascularSegmentationEvaluationStatus::ProcessingFailed,
                        currentStage,
                        VascularSegmentationEvaluationDiagnosticCode::UnexpectedException);
                }
                predictionToReference.push_back(distance);
                symmetricDistanceSum += distance;
            }
            if (referenceSurfaceValues[index] != 0) {
                const double distance =
                    std::abs(static_cast<double>(predictionDistances[index]));
                if (!std::isfinite(distance)) {
                    return failure(
                        start,
                        VascularSegmentationEvaluationStatus::ProcessingFailed,
                        currentStage,
                        VascularSegmentationEvaluationDiagnosticCode::UnexpectedException);
                }
                referenceToPrediction.push_back(distance);
                symmetricDistanceSum += distance;
            }
        }
        const double predictionP95 = percentile95(&predictionToReference);
        const double referenceP95 = percentile95(&referenceToPrediction);
        const double hd95 = (std::max)(predictionP95, referenceP95);
        const double assd = symmetricDistanceSum
            / static_cast<double>(predictionSurfaceCount + referenceSurfaceCount);

        currentStage = VascularSegmentationEvaluationStage::Skeletonize;
        ThinningFilter::Pointer predictionThinning = ThinningFilter::New();
        predictionThinning->SetInput(predictionImage);
        predictionThinning->Update();
        BinaryImage::Pointer predictionSkeleton = predictionThinning->GetOutput();
        predictionSkeleton->DisconnectPipeline();
        ThinningFilter::Pointer referenceThinning = ThinningFilter::New();
        referenceThinning->SetInput(referenceImage);
        referenceThinning->Update();
        BinaryImage::Pointer referenceSkeleton = referenceThinning->GetOutput();
        referenceSkeleton->DisconnectPipeline();
        const std::size_t predictionSkeletonCount =
            countForeground(predictionSkeleton);
        const std::size_t referenceSkeletonCount =
            countForeground(referenceSkeleton);
        if (predictionSkeletonCount == 0 || referenceSkeletonCount == 0) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::ProcessingFailed,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::EmptySkeleton);
        }
        const unsigned char* predictionSkeletonValues =
            predictionSkeleton->GetBufferPointer();
        const unsigned char* referenceSkeletonValues =
            referenceSkeleton->GetBufferPointer();
        std::size_t predictionSkeletonInsideReference = 0;
        std::size_t referenceSkeletonInsidePrediction = 0;
        for (std::size_t index = 0; index < prediction.voxelCount(); ++index) {
            if (predictionSkeletonValues[index] != 0
                && referenceVoxels[index] != 0) {
                ++predictionSkeletonInsideReference;
            }
            if (referenceSkeletonValues[index] != 0
                && predictionVoxels[index] != 0) {
                ++referenceSkeletonInsidePrediction;
            }
        }
        const double topologyPrecision =
            static_cast<double>(predictionSkeletonInsideReference)
            / static_cast<double>(predictionSkeletonCount);
        const double topologySensitivity =
            static_cast<double>(referenceSkeletonInsidePrediction)
            / static_cast<double>(referenceSkeletonCount);
        const double clDice = topologyPrecision + topologySensitivity > 0.0
            ? 2.0 * topologyPrecision * topologySensitivity
                / (topologyPrecision + topologySensitivity)
            : 0.0;

        VascularSegmentationEvaluationResult result;
        result.status = VascularSegmentationEvaluationStatus::Ok;
        result.stage = VascularSegmentationEvaluationStage::Complete;
        result.elapsedMilliseconds = elapsedMilliseconds(start);
        VascularSegmentationMetrics metrics;
        metrics.evaluatorId = evaluatorId();
        metrics.evaluatorVersion = evaluatorVersion();
        metrics.itkVersion = ITK_VERSION;
        metrics.thinningBackendId = thinningBackendId();
        metrics.thinningBackendVersion = thinningBackendVersion();
        metrics.predictionForegroundVoxelCount = predictionForeground;
        metrics.referenceForegroundVoxelCount = referenceForeground;
        metrics.intersectionVoxelCount = intersection;
        metrics.dice = dice;
        metrics.predictionComponentCount = componentCount;
        metrics.largestPredictionComponentVoxelCount = largestComponent;
        metrics.largestPredictionComponentFraction = largestComponentFraction;
        metrics.predictionSurfaceVoxelCount = predictionSurfaceCount;
        metrics.referenceSurfaceVoxelCount = referenceSurfaceCount;
        metrics.predictionToReferenceSurfaceP95Mm = predictionP95;
        metrics.referenceToPredictionSurfaceP95Mm = referenceP95;
        metrics.hd95Mm = hd95;
        metrics.assdMm = assd;
        metrics.predictionSkeletonVoxelCount = predictionSkeletonCount;
        metrics.referenceSkeletonVoxelCount = referenceSkeletonCount;
        metrics.topologyPrecision = topologyPrecision;
        metrics.topologySensitivity = topologySensitivity;
        metrics.clDice = clDice;
        metrics.elapsedMilliseconds = result.elapsedMilliseconds;
        result.metrics.emplace(std::move(metrics));
        if (!result.metrics->isValid()) {
            return failure(start,
                           VascularSegmentationEvaluationStatus::ProcessingFailed,
                           currentStage,
                           VascularSegmentationEvaluationDiagnosticCode::UnexpectedException);
        }
        return result;
    } catch (const itk::ExceptionObject&) {
        return failure(start,
                       VascularSegmentationEvaluationStatus::ProcessingFailed,
                       currentStage,
                       VascularSegmentationEvaluationDiagnosticCode::ItkException);
    } catch (const std::exception&) {
        return failure(start,
                       VascularSegmentationEvaluationStatus::ProcessingFailed,
                       currentStage,
                       VascularSegmentationEvaluationDiagnosticCode::UnexpectedException);
    } catch (...) {
        return failure(start,
                       VascularSegmentationEvaluationStatus::ProcessingFailed,
                       currentStage,
                       VascularSegmentationEvaluationDiagnosticCode::UnexpectedException);
    }
}

} // namespace xq
