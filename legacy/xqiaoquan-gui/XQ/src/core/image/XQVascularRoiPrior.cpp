#include "core/image/XQVascularRoiPrior.h"

namespace xq {

namespace {

bool sameGeometry(const ImageGeometry& left, const ImageGeometry& right)
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

} // namespace

const char* vascularRoiRoleToken(VascularRoiRole role)
{
    switch (role) {
    case VascularRoiRole::Organ: return "organ";
    case VascularRoiRole::CoarseVessel: return "coarse_vessel";
    }
    return "unknown";
}

bool XQVascularRoiLayer::isValid() const
{
    return mask != nullptr && mask->is_valid() && mask->hasGeometry()
        && hasSourceGeometry && sourceForegroundVoxelCount > 0
        && alignedForegroundVoxelCount > 0
        && alignedForegroundVoxelCount == mask->foregroundVoxelCount()
        && !sourceFingerprint.empty() && !alignedFingerprint.empty()
        && !generatorId.empty() && !generatorVersion.empty();
}

const XQVascularRoiLayer* XQVascularRoiPriorV1::layer(
    VascularRoiRole role) const
{
    for (const XQVascularRoiLayer& candidate : layers) {
        if (candidate.role == role) {
            return &candidate;
        }
    }
    return nullptr;
}

bool XQVascularRoiPriorV1::isValid() const
{
    if (layers.size() != 2 || ctInputFingerprint.empty()
        || priorFingerprint.empty()) {
        return false;
    }
    const XQVascularRoiLayer* organ = layer(VascularRoiRole::Organ);
    const XQVascularRoiLayer* vessel = layer(VascularRoiRole::CoarseVessel);
    return organ != nullptr && vessel != nullptr && organ != vessel
        && organ->isValid() && vessel->isValid()
        && sameGeometry(organ->mask->geometry(), vessel->mask->geometry());
}

} // namespace xq
