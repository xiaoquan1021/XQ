#include "io/source/MappedVoxelSource.h"

#include "core/XQMemoryImageBufferHandle.h" // scalarSize
#include "io/blob/Sha256.h"

#include <cstring>
#include <utility>

namespace xq {

MappedVoxelSource::MappedVoxelSource(const std::string& blobPath,
                                     const int dims[3],
                                     ScalarType type,
                                     int components,
                                     IntegrityMode mode,
                                     const std::string& merklePath,
                                     const std::string& expectedSha256)
    : type_(type)
    , components_(components)
    , mode_(mode)
    , expectedSha256_(expectedSha256)
{
    dims_[0] = dims[0];
    dims_[1] = dims[1];
    dims_[2] = dims[2];

    if (type_ == ScalarType::Unknown || components_ < 1 || dims_[0] < 1
        || dims_[1] < 1 || dims_[2] < 1) {
        return;
    }

    mapped_ = map_file_readonly(blobPath);
    if (!mapped_.ok) {
        return;
    }

    // The mapped blob must be exactly the voxel payload size.
    if (mapped_.size != totalBytes()) {
        return;
    }

    if (mode_ == IntegrityMode::SegmentedMerkle) {
        if (!load_sidecar(merklePath, &sidecar_)) {
            return;
        }
        // Trust anchor: the listed per-segment digests must hash to the stored
        // root, and the sidecar must describe exactly this blob.
        if (!sidecar_.verify_root()) {
            return;
        }
        if (sidecar_.blobBytes != static_cast<std::uint64_t>(mapped_.size)) {
            return;
        }
        if (sidecar_.segmentBytes == 0) {
            return;
        }
        segChecked_.assign(static_cast<std::size_t>(sidecar_.segCount), false);
    }

    valid_ = true;
}

std::size_t MappedVoxelSource::voxelStride() const
{
    return static_cast<std::size_t>(components_)
        * XQMemoryImageBufferHandle::scalarSize(type_);
}

std::size_t MappedVoxelSource::totalBytes() const
{
    const std::size_t voxels = static_cast<std::size_t>(dims_[0])
        * static_cast<std::size_t>(dims_[1]) * static_cast<std::size_t>(dims_[2]);
    return voxels * voxelStride();
}

VoxelMeta MappedVoxelSource::meta() const
{
    VoxelMeta m;
    if (!valid_) {
        return m;
    }
    m.valid = true;
    m.type = type_;
    m.dims[0] = dims_[0];
    m.dims[1] = dims_[1];
    m.dims[2] = dims_[2];
    m.components = components_;
    m.voxelCount = static_cast<std::size_t>(dims_[0])
        * static_cast<std::size_t>(dims_[1]) * static_cast<std::size_t>(dims_[2]);
    return m;
}

bool MappedVoxelSource::verify_range(std::size_t byteBegin,
                                     std::size_t byteEnd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.acquireCount;

    if (byteEnd > mapped_.size) {
        byteEnd = mapped_.size;
    }
    if (byteBegin >= byteEnd) {
        return true; // nothing to verify
    }

    if (mode_ == IntegrityMode::FullVerify) {
        if (fullVerified_) {
            return true;
        }
        const std::string digest = Sha256::hashHex(mapped_.base, mapped_.size);
        stats_.bytesHashed += static_cast<std::uint64_t>(mapped_.size);
        // Compare against the content-address anchor (BufferRef.sha256) when the
        // caller supplied one: a mismatch means the blob on disk was corrupted or
        // tampered, so the read must fail. When no anchor was supplied (empty),
        // we still hash once + memoize (uniform read-path cost) but cannot
        // compare -- the M8a anchor then lives only in the main document.
        if (!expectedSha256_.empty() && digest != expectedSha256_) {
            return false; // corrupt / tampered blob
        }
        fullVerified_ = true;
        return true;
    }

    // SegmentedMerkle: verify each touched segment lazily, memoized.
    const std::size_t segBytes = static_cast<std::size_t>(sidecar_.segmentBytes);
    const std::uint64_t firstSeg = static_cast<std::uint64_t>(byteBegin / segBytes);
    const std::uint64_t lastSeg =
        static_cast<std::uint64_t>((byteEnd - 1) / segBytes);

    for (std::uint64_t seg = firstSeg; seg <= lastSeg; ++seg) {
        if (seg >= sidecar_.segCount) {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(seg);
        if (segChecked_[idx]) {
            continue;
        }
        if (!sidecar_.verify_segment(mapped_.base, mapped_.size, seg)) {
            return false; // tampered or corrupt segment
        }
        segChecked_[idx] = true;
        ++stats_.segmentsChecked;

        // Account the bytes actually hashed for this segment (trailing segment
        // may be short).
        std::size_t n = segBytes;
        const std::size_t off = idx * segBytes;
        if (off + n > mapped_.size) {
            n = mapped_.size - off;
        }
        stats_.bytesHashed += static_cast<std::uint64_t>(n);
    }
    return true;
}

VoxelLease MappedVoxelSource::acquire_whole() const
{
    if (!valid_) {
        return VoxelLease::invalid();
    }
    const std::size_t total = totalBytes();
    if (!verify_range(0, total)) {
        return VoxelLease::invalid();
    }

    VoxelView view;
    view.valid = true;
    view.type = type_;
    view.dims[0] = dims_[0];
    view.dims[1] = dims_[1];
    view.dims[2] = dims_[2];
    view.components = components_;
    view.bytes = ReadSpan<std::uint8_t>(
        static_cast<const std::uint8_t*>(mapped_.base), total);
    // Zero copy: keepalive is the mmap control block; the view points straight
    // into the mapping.
    return VoxelLease::borrow(mapped_.keepalive, view);
}

VoxelLease MappedVoxelSource::acquire_region(const int extent[6]) const
{
    if (!valid_) {
        return VoxelLease::invalid();
    }

    const int x0 = extent[0];
    const int x1 = extent[1];
    const int y0 = extent[2];
    const int y1 = extent[3];
    const int z0 = extent[4];
    const int z1 = extent[5];

    const int dimX = dims_[0];
    const int dimY = dims_[1];
    const int dimZ = dims_[2];

    if (x0 < 0 || y0 < 0 || z0 < 0 || x1 < x0 || y1 < y0 || z1 < z0
        || x1 >= dimX || y1 >= dimY || z1 >= dimZ) {
        return VoxelLease::invalid();
    }

    const std::size_t stride = voxelStride();

    // The region's touched voxels (x-fastest) span the linear byte range from
    // the first to the last voxel inclusive; verify that whole span lazily
    // (conservative: may cover in-between voxels, which is safe).
    auto linearIndex = [dimX, dimY](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x)
            + static_cast<std::size_t>(dimX)
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(dimY) * static_cast<std::size_t>(z));
    };
    const std::size_t firstVoxel = linearIndex(x0, y0, z0);
    const std::size_t lastVoxel = linearIndex(x1, y1, z1);
    const std::size_t byteBegin = firstVoxel * stride;
    const std::size_t byteEnd = (lastVoxel + 1) * stride;

    if (!verify_range(byteBegin, byteEnd)) {
        return VoxelLease::invalid();
    }

    const int subX = x1 - x0 + 1;
    const int subY = y1 - y0 + 1;
    const int subZ = z1 - z0 + 1;
    const std::size_t subVoxels = static_cast<std::size_t>(subX)
        * static_cast<std::size_t>(subY) * static_cast<std::size_t>(subZ);

    std::vector<std::uint8_t> out(subVoxels * stride);
    const std::uint8_t* src = static_cast<const std::uint8_t*>(mapped_.base);

    std::size_t dst = 0;
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            // A whole x-run is contiguous in source; copy it in one memcpy.
            const std::size_t srcVoxel = linearIndex(x0, y, z);
            const std::size_t srcOffset = srcVoxel * stride;
            const std::size_t runBytes = static_cast<std::size_t>(subX) * stride;
            std::memcpy(out.data() + dst, src + srcOffset, runBytes);
            dst += runBytes;
        }
    }

    const int subDims[3] = {subX, subY, subZ};
    return VoxelLease::own(std::move(out), type_, subDims, components_);
}

VoxelLease MappedVoxelSource::acquire_slab(int z) const
{
    if (!valid_) {
        return VoxelLease::invalid();
    }
    const int extent[6] = {0, dims_[0] - 1, 0, dims_[1] - 1, z, z};
    return acquire_region(extent);
}

MappedVoxelSource::IntegrityStats MappedVoxelSource::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace xq
