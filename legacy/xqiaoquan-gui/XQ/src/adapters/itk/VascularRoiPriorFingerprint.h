#ifndef XQ_ADAPTERS_ITK_VASCULAR_ROI_PRIOR_FINGERPRINT_H
#define XQ_ADAPTERS_ITK_VASCULAR_ROI_PRIOR_FINGERPRINT_H

#include "core/image/XQVascularRoiPrior.h"

#include "picosha2.h"

#include <cstdint>
#include <string>
#include <vector>

namespace xq {
namespace itk_detail {

inline void vascularRoiHashBytes(picosha2::hash256_one_by_one* hasher,
                                 const void* data,
                                 std::size_t size)
{
    const unsigned char* first = static_cast<const unsigned char*>(data);
    hasher->process(first, first + size);
}

inline void vascularRoiHashText(picosha2::hash256_one_by_one* hasher,
                                const std::string& text)
{
    const std::uint64_t size = static_cast<std::uint64_t>(text.size());
    vascularRoiHashBytes(hasher, &size, sizeof(size));
    if (!text.empty()) {
        vascularRoiHashBytes(hasher, text.data(), text.size());
    }
}

inline std::string vascularRoiAlignedFingerprint(
    VascularRoiRole role,
    const ImageGeometry& geometry,
    const std::vector<XQSegmentationMask::LabelType>& voxels)
{
    picosha2::hash256_one_by_one hasher;
    vascularRoiHashText(&hasher, "xq-vascular-roi-layer-v1");
    vascularRoiHashText(&hasher, vascularRoiRoleToken(role));
    vascularRoiHashBytes(
        &hasher, geometry.dimensions, sizeof(geometry.dimensions));
    vascularRoiHashBytes(&hasher, geometry.spacing, sizeof(geometry.spacing));
    vascularRoiHashBytes(&hasher, geometry.origin, sizeof(geometry.origin));
    vascularRoiHashBytes(
        &hasher, geometry.direction, sizeof(geometry.direction));
    if (!voxels.empty()) {
        vascularRoiHashBytes(&hasher, voxels.data(), voxels.size());
    }
    hasher.finish();
    return "xq-vascular-roi-layer-v1:sha256:"
        + picosha2::get_hash_hex_string(hasher);
}

inline std::string vascularRoiPriorFingerprint(
    const std::string& ctFingerprint,
    const std::vector<XQVascularRoiLayer>& layers)
{
    picosha2::hash256_one_by_one hasher;
    vascularRoiHashText(&hasher, "xq-vascular-roi-prior-v1");
    vascularRoiHashText(&hasher, ctFingerprint);
    for (const XQVascularRoiLayer& layer : layers) {
        vascularRoiHashText(&hasher, vascularRoiRoleToken(layer.role));
        vascularRoiHashText(&hasher, layer.sourceFingerprint);
        vascularRoiHashText(&hasher, layer.alignedFingerprint);
        vascularRoiHashText(&hasher, layer.generatorId);
        vascularRoiHashText(&hasher, layer.generatorVersion);
    }
    hasher.finish();
    return "xq-vascular-roi-prior-v1:sha256:"
        + picosha2::get_hash_hex_string(hasher);
}

inline bool vascularRoiPriorFingerprintsMatch(
    const XQVascularRoiPriorV1& prior)
{
    if (!prior.isValid()) {
        return false;
    }
    for (const XQVascularRoiLayer& layer : prior.layers) {
        if (layer.mask == nullptr
            || layer.alignedFingerprint
                != vascularRoiAlignedFingerprint(
                    layer.role, layer.mask->geometry(), layer.mask->voxels())) {
            return false;
        }
    }
    return prior.priorFingerprint
        == vascularRoiPriorFingerprint(prior.ctInputFingerprint, prior.layers);
}

} // namespace itk_detail
} // namespace xq

#endif // XQ_ADAPTERS_ITK_VASCULAR_ROI_PRIOR_FINGERPRINT_H
