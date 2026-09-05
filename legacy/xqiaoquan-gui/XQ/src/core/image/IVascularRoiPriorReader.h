#ifndef XQ_CORE_IMAGE_I_VASCULAR_ROI_PRIOR_READER_H
#define XQ_CORE_IMAGE_I_VASCULAR_ROI_PRIOR_READER_H

#include "core/Diagnostics.h"
#include "core/XQImageVolume.h"
#include "core/image/XQVascularRoiPrior.h"

#include <optional>
#include <string>
#include <vector>

namespace xq {

enum class VascularRoiPriorReadStatus {
    Ok,
    InvalidArgument,
    InvalidReferenceGeometry,
    DuplicateRole,
    SourceReadFailed,
    InvalidSourceGeometry,
    NoPhysicalOverlap,
    EmptyForeground,
    AllForeground,
    AllocationFailed,
    ProcessingFailed
};

const char* vascularRoiPriorReadStatusToken(
    VascularRoiPriorReadStatus status);

struct VascularRoiFileInput {
    VascularRoiRole role = VascularRoiRole::Organ;
    std::string path;
    std::string generatorId;
    std::string generatorVersion;
};

struct VascularRoiPriorReadResult {
    VascularRoiPriorReadStatus status =
        VascularRoiPriorReadStatus::InvalidArgument;
    std::optional<XQVascularRoiPriorV1> prior;
    std::vector<Diagnostic> diagnostics;

    bool ok() const;
};

// Public ROI import boundary. Implementations may use NIfTI/ITK privately, but
// callers only exchange XQ/std values and receive fully materialized XQ masks.
class IVascularRoiPriorReader {
public:
    virtual ~IVascularRoiPriorReader() = default;

    virtual VascularRoiPriorReadResult read(
        const XQImageVolume& reference,
        const std::string& ctInputFingerprint,
        const std::vector<VascularRoiFileInput>& inputs) const = 0;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_I_VASCULAR_ROI_PRIOR_READER_H
