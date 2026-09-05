#include "core/path/ICenterlineSkeletonizer3D.h"

#include <cmath>
#include <limits>

namespace xq {
namespace {

bool checkedVoxelCount(const ImageGeometry& geometry, std::size_t* count)
{
    if (count == nullptr) {
        return false;
    }
    std::size_t value = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0) {
            return false;
        }
        const std::size_t extent =
            static_cast<std::size_t>(geometry.dimensions[axis]);
        if (value > (std::numeric_limits<std::size_t>::max)() / extent) {
            return false;
        }
        value *= extent;
    }
    *count = value;
    return true;
}

} // namespace

const char* centerlineSkeletonizationStatusToken(
    CenterlineSkeletonizationStatus status)
{
    switch (status) {
    case CenterlineSkeletonizationStatus::Ok: return "ok";
    case CenterlineSkeletonizationStatus::InvalidGeometry:
        return "invalid_geometry";
    case CenterlineSkeletonizationStatus::InvalidSource: return "invalid_source";
    case CenterlineSkeletonizationStatus::NonBinaryMask: return "non_binary_mask";
    case CenterlineSkeletonizationStatus::EmptyMask: return "empty_mask";
    case CenterlineSkeletonizationStatus::EmptySkeleton: return "empty_skeleton";
    case CenterlineSkeletonizationStatus::NonPositiveRadius:
        return "non_positive_radius";
    case CenterlineSkeletonizationStatus::ProcessingFailed:
        return "processing_failed";
    }
    return "unknown";
}

const char* centerlineSkeletonizationStageToken(
    CenterlineSkeletonizationStage stage)
{
    switch (stage) {
    case CenterlineSkeletonizationStage::ValidateInput: return "validate_input";
    case CenterlineSkeletonizationStage::ImportMask: return "import_mask";
    case CenterlineSkeletonizationStage::DistanceMap: return "distance_map";
    case CenterlineSkeletonizationStage::Thin: return "thin";
    case CenterlineSkeletonizationStage::Materialize: return "materialize";
    case CenterlineSkeletonizationStage::Complete: return "complete";
    }
    return "unknown";
}

bool CenterlineSkeletonV1::isValid() const
{
    std::size_t voxelCount = 0;
    if (contractVersion != ContractVersion
        || !checkedVoxelCount(geometry, &voxelCount)
        || skeleton.size() != voxelCount || radiusMm.size() != voxelCount
        || inputForegroundVoxelCount == 0 || skeletonVoxelCount == 0
        || skeletonVoxelCount > inputForegroundVoxelCount
        || thinningBackendId.empty() || thinningBackendVersion.empty()
        || distanceBackendId.empty() || distanceBackendVersion.empty()) {
        return false;
    }

    std::size_t observedSkeleton = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (skeleton[index] > 1) {
            return false;
        }
        if (skeleton[index] == 0) {
            continue;
        }
        ++observedSkeleton;
        const double radius = static_cast<double>(radiusMm[index]);
        if (!std::isfinite(radius) || !(radius > 0.0)) {
            return false;
        }
    }
    return observedSkeleton == skeletonVoxelCount;
}

} // namespace xq
