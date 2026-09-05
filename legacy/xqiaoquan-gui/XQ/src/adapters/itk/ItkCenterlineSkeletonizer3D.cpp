#include "adapters/itk/ItkCenterlineSkeletonizer3D.h"

#include "core/source/IVoxelSource.h"

#include "itkBinaryThinningImageFilter3D.h"
#include "itkImage.h"
#include "itkImportImageFilter.h"
#include "itkMacro.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkVersion.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <utility>
#include <vector>

namespace xq {
namespace {

constexpr unsigned int kDimension = 3;
constexpr double kDirectionDeterminantEpsilon = 1e-12;

using BinaryImage = itk::Image<unsigned char, kDimension>;
using FloatImage = itk::Image<float, kDimension>;
using BinaryImporter = itk::ImportImageFilter<unsigned char, kDimension>;
using ThinningFilter =
    itk::BinaryThinningImageFilter3D<BinaryImage, BinaryImage>;
using DistanceFilter =
    itk::SignedMaurerDistanceMapImageFilter<BinaryImage, FloatImage>;

CenterlineSkeletonizationResult failure(
    CenterlineSkeletonizationStatus status,
    CenterlineSkeletonizationStage stage)
{
    CenterlineSkeletonizationResult result;
    result.status = status;
    result.stage = stage;
    return result;
}

double directionDeterminant(const double direction[3][3])
{
    return direction[0][0]
            * (direction[1][1] * direction[2][2]
               - direction[1][2] * direction[2][1])
        - direction[0][1]
            * (direction[1][0] * direction[2][2]
               - direction[1][2] * direction[2][0])
        + direction[0][2]
            * (direction[1][0] * direction[2][1]
               - direction[1][1] * direction[2][0]);
}

bool validGeometry(const ImageGeometry& geometry, std::size_t* voxelCount)
{
    if (voxelCount == nullptr
        || geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
        return false;
    }
    std::size_t count = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0
            || !std::isfinite(geometry.spacing[axis])
            || !(geometry.spacing[axis] > 0.0)
            || !std::isfinite(geometry.origin[axis])) {
            return false;
        }
        const std::size_t extent =
            static_cast<std::size_t>(geometry.dimensions[axis]);
        if (count > (std::numeric_limits<std::size_t>::max)() / extent) {
            return false;
        }
        count *= extent;
        for (int column = 0; column < 3; ++column) {
            if (!std::isfinite(geometry.direction[axis][column])) {
                return false;
            }
        }
    }
    const double determinant = directionDeterminant(geometry.direction);
    if (!std::isfinite(determinant)
        || std::abs(determinant) <= kDirectionDeterminantEpsilon) {
        return false;
    }
    *voxelCount = count;
    return true;
}

bool sourceMatches(const VoxelMeta& meta,
                   const ImageGeometry& geometry,
                   std::size_t voxelCount)
{
    return meta.valid && meta.type == ScalarType::UInt8
        && meta.components == 1 && meta.voxelCount == voxelCount
        && meta.dims[0] == geometry.dimensions[0]
        && meta.dims[1] == geometry.dimensions[1]
        && meta.dims[2] == geometry.dimensions[2];
}

BinaryImage::Pointer importMask(const ImageGeometry& geometry,
                                const std::uint8_t* values,
                                std::size_t voxelCount)
{
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
    importer->SetImportPointer(
        const_cast<unsigned char*>(values), voxelCount, false);
    importer->Update();
    BinaryImage::Pointer image = importer->GetOutput();
    image->DisconnectPipeline();
    return image;
}

} // namespace

const char* ItkCenterlineSkeletonizer3D::thinningBackendId()
{
    return "ITKThickness3D.BinaryThinningImageFilter3D";
}

const char* ItkCenterlineSkeletonizer3D::thinningBackendVersion()
{
    return "v5.3.0@36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb";
}

const char* ItkCenterlineSkeletonizer3D::distanceBackendId()
{
    return "itk::SignedMaurerDistanceMapImageFilter";
}

const char* ItkCenterlineSkeletonizer3D::distanceBackendVersion()
{
    return ITK_VERSION;
}

CenterlineSkeletonizationResult ItkCenterlineSkeletonizer3D::run(
    const ImageGeometry& geometry,
    const IVoxelSource& source) const
{
    CenterlineSkeletonizationStage stage =
        CenterlineSkeletonizationStage::ValidateInput;
    try {
        std::size_t voxelCount = 0;
        if (!validGeometry(geometry, &voxelCount)) {
            return failure(
                CenterlineSkeletonizationStatus::InvalidGeometry, stage);
        }
        const VoxelMeta meta = source.meta();
        if (!sourceMatches(meta, geometry, voxelCount)) {
            return failure(CenterlineSkeletonizationStatus::InvalidSource, stage);
        }

        VoxelLease whole = source.acquire_whole();
        const VoxelView& view = whole.view();
        if (!view.valid || view.type != ScalarType::UInt8
            || view.components != 1 || view.voxelCount() != voxelCount
            || view.bytes.size() != voxelCount
            || view.dims[0] != geometry.dimensions[0]
            || view.dims[1] != geometry.dimensions[1]
            || view.dims[2] != geometry.dimensions[2]
            || view.bytes.data() == nullptr) {
            return failure(CenterlineSkeletonizationStatus::InvalidSource, stage);
        }

        std::size_t foreground = 0;
        for (std::size_t index = 0; index < voxelCount; ++index) {
            const std::uint8_t value = view.bytes[index];
            if (value > 1) {
                return failure(
                    CenterlineSkeletonizationStatus::NonBinaryMask, stage);
            }
            foreground += value != 0 ? 1u : 0u;
        }
        if (foreground == 0) {
            return failure(CenterlineSkeletonizationStatus::EmptyMask, stage);
        }

        stage = CenterlineSkeletonizationStage::ImportMask;
        BinaryImage::Pointer mask = importMask(
            geometry, view.bytes.data(), voxelCount);

        stage = CenterlineSkeletonizationStage::DistanceMap;
        DistanceFilter::Pointer distance = DistanceFilter::New();
        distance->SetInput(mask);
        distance->SetUseImageSpacing(true);
        distance->SetSquaredDistance(false);
        distance->SetInsideIsPositive(true);
        distance->Update();
        FloatImage::Pointer distanceImage = distance->GetOutput();
        distanceImage->DisconnectPipeline();

        stage = CenterlineSkeletonizationStage::Thin;
        ThinningFilter::Pointer thinning = ThinningFilter::New();
        thinning->SetInput(mask);
        thinning->Update();
        BinaryImage::Pointer skeletonImage = thinning->GetOutput();
        skeletonImage->DisconnectPipeline();

        stage = CenterlineSkeletonizationStage::Materialize;
        const unsigned char* skeletonValues =
            skeletonImage->GetBufferPointer();
        const float* radiusValues = distanceImage->GetBufferPointer();
        if (skeletonValues == nullptr || radiusValues == nullptr
            || skeletonImage->GetLargestPossibleRegion().GetNumberOfPixels()
                != voxelCount
            || distanceImage->GetLargestPossibleRegion().GetNumberOfPixels()
                != voxelCount) {
            return failure(
                CenterlineSkeletonizationStatus::ProcessingFailed, stage);
        }

        CenterlineSkeletonV1 output;
        output.geometry = geometry;
        output.inputForegroundVoxelCount = foreground;
        output.thinningBackendId = thinningBackendId();
        output.thinningBackendVersion = thinningBackendVersion();
        output.distanceBackendId = distanceBackendId();
        output.distanceBackendVersion = distanceBackendVersion();
        output.skeleton.assign(skeletonValues, skeletonValues + voxelCount);
        output.radiusMm.assign(radiusValues, radiusValues + voxelCount);
        for (std::size_t index = 0; index < voxelCount; ++index) {
            if (output.skeleton[index] == 0) {
                continue;
            }
            ++output.skeletonVoxelCount;
            const double radius = static_cast<double>(output.radiusMm[index]);
            if (!std::isfinite(radius) || !(radius > 0.0)) {
                return failure(
                    CenterlineSkeletonizationStatus::NonPositiveRadius, stage);
            }
        }
        if (output.skeletonVoxelCount == 0) {
            return failure(
                CenterlineSkeletonizationStatus::EmptySkeleton, stage);
        }
        if (!output.isValid()) {
            return failure(
                CenterlineSkeletonizationStatus::ProcessingFailed, stage);
        }

        CenterlineSkeletonizationResult result;
        result.status = CenterlineSkeletonizationStatus::Ok;
        result.stage = CenterlineSkeletonizationStage::Complete;
        result.output.emplace(std::move(output));
        return result;
    } catch (const itk::ExceptionObject&) {
        return failure(CenterlineSkeletonizationStatus::ProcessingFailed, stage);
    } catch (const std::exception&) {
        return failure(CenterlineSkeletonizationStatus::ProcessingFailed, stage);
    } catch (...) {
        return failure(CenterlineSkeletonizationStatus::ProcessingFailed, stage);
    }
}

} // namespace xq
