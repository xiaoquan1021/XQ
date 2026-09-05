#ifndef XQ_CORE_SOURCE_I_VOXEL_SOURCE_H
#define XQ_CORE_SOURCE_I_VOXEL_SOURCE_H

#include "core/source/ReadLease.h"
#include "core/source/SourceViews.h"

namespace xq {

// Read-only voxel access contract. The only virtual boundary is "acquire one
// block": each acquire is a single virtual dispatch returning a contiguous
// read-only view; all in-view access is non-virtual inline.
class IVoxelSource {
public:
    virtual ~IVoxelSource() = default;

    virtual VoxelMeta meta() const = 0;

    // Whole volume (borrowed, zero copy).
    virtual VoxelLease acquire_whole() const = 0;

    // Sub-block by inclusive extent {x0, x1, y0, y1, z0, z1}. Out-of-range
    // extent yields an invalid lease (view().valid == false), not an exception.
    virtual VoxelLease acquire_region(const int extent[6]) const = 0;

    // The z-th xy plane. Out-of-range z yields an invalid lease.
    virtual VoxelLease acquire_slab(int z) const = 0;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_I_VOXEL_SOURCE_H
