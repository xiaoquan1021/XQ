#ifndef XQ_ADAPTERS_ITK_ITK_CENTERLINE_SKELETONIZER_3D_H
#define XQ_ADAPTERS_ITK_ITK_CENTERLINE_SKELETONIZER_3D_H

#include "core/path/ICenterlineSkeletonizer3D.h"

namespace xq {

class ItkCenterlineSkeletonizer3D final
    : public ICenterlineSkeletonizer3D {
public:
    static const char* thinningBackendId();
    static const char* thinningBackendVersion();
    static const char* distanceBackendId();
    static const char* distanceBackendVersion();

    CenterlineSkeletonizationResult run(
        const ImageGeometry& geometry,
        const IVoxelSource& source) const override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_CENTERLINE_SKELETONIZER_3D_H
