#include "core/source/ResidentVoxelSource.h"

#include <cstring>
#include <utility>

namespace xq {

ResidentVoxelSource::ResidentVoxelSource(
    std::shared_ptr<const XQMemoryImageBufferHandle> handle)
    : handle_(std::move(handle))
{
}

VoxelMeta ResidentVoxelSource::meta() const
{
    VoxelMeta m;
    if (!handle_ || !handle_->is_valid()) {
        return m;
    }
    m.valid = true;
    m.type = handle_->scalarType();
    m.dims[0] = handle_->dimensionX();
    m.dims[1] = handle_->dimensionY();
    m.dims[2] = handle_->dimensionZ();
    m.components = handle_->componentCount();
    m.voxelCount = handle_->voxelCount();
    return m;
}

VoxelLease ResidentVoxelSource::acquire_whole() const
{
    if (!handle_ || !handle_->is_valid()) {
        return VoxelLease::invalid();
    }
    VoxelView view;
    view.valid = true;
    view.type = handle_->scalarType();
    view.dims[0] = handle_->dimensionX();
    view.dims[1] = handle_->dimensionY();
    view.dims[2] = handle_->dimensionZ();
    view.components = handle_->componentCount();
    const std::vector<std::uint8_t>& bytes = handle_->bytes();
    view.bytes = ReadSpan<std::uint8_t>(bytes.data(), bytes.size());
    return VoxelLease::borrow(handle_, view);
}

VoxelLease ResidentVoxelSource::acquire_region(const int extent[6]) const
{
    if (!handle_ || !handle_->is_valid()) {
        return VoxelLease::invalid();
    }

    const int x0 = extent[0];
    const int x1 = extent[1];
    const int y0 = extent[2];
    const int y1 = extent[3];
    const int z0 = extent[4];
    const int z1 = extent[5];

    const int dimX = handle_->dimensionX();
    const int dimY = handle_->dimensionY();
    const int dimZ = handle_->dimensionZ();

    // Inclusive extent must be ordered and inside [0, dim).
    if (x0 < 0 || y0 < 0 || z0 < 0 || x1 < x0 || y1 < y0 || z1 < z0
        || x1 >= dimX || y1 >= dimY || z1 >= dimZ) {
        return VoxelLease::invalid();
    }

    const ScalarType type = handle_->scalarType();
    const int components = handle_->componentCount();
    const std::size_t scalar_size = XQMemoryImageBufferHandle::scalarSize(type);
    const std::size_t voxel_stride =
        static_cast<std::size_t>(components) * scalar_size; // bytes per voxel

    const int subX = x1 - x0 + 1;
    const int subY = y1 - y0 + 1;
    const int subZ = z1 - z0 + 1;

    const std::size_t subVoxels = static_cast<std::size_t>(subX)
        * static_cast<std::size_t>(subY) * static_cast<std::size_t>(subZ);

    std::vector<std::uint8_t> out(subVoxels * voxel_stride);
    const std::vector<std::uint8_t>& src = handle_->bytes();

    // Copy each target voxel in x-fastest order from its source linear index.
    std::size_t dst = 0;
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const std::size_t srcVoxel = handle_->voxelIndex(x, y, z);
                const std::size_t srcOffset = srcVoxel * voxel_stride;
                std::memcpy(out.data() + dst, src.data() + srcOffset, voxel_stride);
                dst += voxel_stride;
            }
        }
    }

    const int dims[3] = {subX, subY, subZ};
    return VoxelLease::own(std::move(out), type, dims, components);
}

VoxelLease ResidentVoxelSource::acquire_slab(int z) const
{
    if (!handle_ || !handle_->is_valid()) {
        return VoxelLease::invalid();
    }
    const int extent[6] = {
        0, handle_->dimensionX() - 1,
        0, handle_->dimensionY() - 1,
        z, z,
    };
    return acquire_region(extent);
}

} // namespace xq
