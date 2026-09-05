#ifndef XQ_CORE_IMAGE_XQ_VASCULAR_ROI_PRIOR_H
#define XQ_CORE_IMAGE_XQ_VASCULAR_ROI_PRIOR_H

#include "core/XQSegmentationMask.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace xq {

enum class VascularRoiRole {
    Organ,
    CoarseVessel
};

const char* vascularRoiRoleToken(VascularRoiRole role);

struct XQVascularRoiLayer {
    VascularRoiRole role = VascularRoiRole::Organ;
    std::shared_ptr<XQSegmentationMask> mask;
    ImageGeometry sourceGeometry{};
    bool hasSourceGeometry = false;
    bool resampledToReference = false;
    std::size_t sourceForegroundVoxelCount = 0;
    std::size_t alignedForegroundVoxelCount = 0;
    std::string sourceFingerprint;
    std::string alignedFingerprint;
    std::string generatorId;
    std::string generatorVersion;

    bool isValid() const;
};

struct XQVascularRoiPriorV1 {
    std::vector<XQVascularRoiLayer> layers;
    std::string ctInputFingerprint;
    std::string priorFingerprint;

    const XQVascularRoiLayer* layer(VascularRoiRole role) const;
    bool isValid() const;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_XQ_VASCULAR_ROI_PRIOR_H
