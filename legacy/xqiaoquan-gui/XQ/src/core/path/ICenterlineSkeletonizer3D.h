#ifndef XQ_CORE_PATH_I_CENTERLINE_SKELETONIZER_3D_H
#define XQ_CORE_PATH_I_CENTERLINE_SKELETONIZER_3D_H

#include "core/XQImageVolume.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class IVoxelSource;

enum class CenterlineSkeletonizationStatus {
    Ok,
    InvalidGeometry,
    InvalidSource,
    NonBinaryMask,
    EmptyMask,
    EmptySkeleton,
    NonPositiveRadius,
    ProcessingFailed
};

enum class CenterlineSkeletonizationStage {
    ValidateInput,
    ImportMask,
    DistanceMap,
    Thin,
    Materialize,
    Complete
};

const char* centerlineSkeletonizationStatusToken(
    CenterlineSkeletonizationStatus status);
const char* centerlineSkeletonizationStageToken(
    CenterlineSkeletonizationStage stage);

// XQ-owned output of a real 3D thinning + physical distance-map kernel. Arrays
// use the same x-fastest layout as IVoxelSource and XQSegmentationMask. The
// radius array is the complete physical distance map so a later tree extractor
// can reuse this adapter without running either external filter again.
struct CenterlineSkeletonV1 {
    static constexpr unsigned int ContractVersion = 1;

    unsigned int contractVersion = ContractVersion;
    ImageGeometry geometry{};
    std::vector<std::uint8_t> skeleton;
    std::vector<float> radiusMm;
    std::size_t inputForegroundVoxelCount = 0;
    std::size_t skeletonVoxelCount = 0;
    std::string thinningBackendId;
    std::string thinningBackendVersion;
    std::string distanceBackendId;
    std::string distanceBackendVersion;

    bool isValid() const;
};

struct CenterlineSkeletonizationResult {
    CenterlineSkeletonizationStatus status =
        CenterlineSkeletonizationStatus::InvalidSource;
    CenterlineSkeletonizationStage stage =
        CenterlineSkeletonizationStage::ValidateInput;
    std::optional<CenterlineSkeletonV1> output;

    bool ok() const
    {
        return status == CenterlineSkeletonizationStatus::Ok
            && output.has_value() && output->isValid();
    }
};

// External kernels implement this interface in adapters. Public callers see
// only XQ geometry, IVoxelSource and XQ-owned arrays.
class ICenterlineSkeletonizer3D {
public:
    virtual ~ICenterlineSkeletonizer3D() = default;

    virtual CenterlineSkeletonizationResult run(
        const ImageGeometry& geometry,
        const IVoxelSource& source) const = 0;
};

} // namespace xq

#endif // XQ_CORE_PATH_I_CENTERLINE_SKELETONIZER_3D_H
