#ifndef XQ_CORE_SOURCE_RESIDENT_VOXEL_SOURCE_H
#define XQ_CORE_SOURCE_RESIDENT_VOXEL_SOURCE_H

#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/IVoxelSource.h"

#include <memory>

namespace xq {

// Adapts an in-memory XQMemoryImageBufferHandle as an IVoxelSource. The whole
// volume is borrowed (zero copy); region/slab sub-blocks are materialized once
// per acquire. Holds a shared_ptr<const handle> and the lease copies it as a
// keepalive, so borrowed views stay valid even if this adapter is destroyed.
class ResidentVoxelSource : public IVoxelSource {
public:
    explicit ResidentVoxelSource(std::shared_ptr<const XQMemoryImageBufferHandle> handle);

    VoxelMeta meta() const override;
    VoxelLease acquire_whole() const override;
    VoxelLease acquire_region(const int extent[6]) const override;
    VoxelLease acquire_slab(int z) const override;

private:
    std::shared_ptr<const XQMemoryImageBufferHandle> handle_;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_RESIDENT_VOXEL_SOURCE_H
