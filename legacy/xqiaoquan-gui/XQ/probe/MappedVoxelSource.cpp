#include "MappedVoxelSource.h"

#include "io/blob/Sha256.h"

#include <algorithm>
#include <cstring>

namespace xq {
namespace probe {

namespace {

MmapBlob mapOrFail(const std::string& path)
{
    return map_file_readonly(path);
}

} // namespace

MappedVoxelSource::MappedVoxelSource(const std::string& blobPath,
                                     const int dims[3],
                                     const std::string& expectedSha,
                                     IntegrityMode mode,
                                     std::size_t segmentBytes)
    : blob_(mapOrFail(blobPath))
    , dims_{dims[0], dims[1], dims[2]}
    , voxelCount_(static_cast<std::size_t>(dims[0])
                  * static_cast<std::size_t>(dims[1])
                  * static_cast<std::size_t>(dims[2]))
    , expectedSha_(expectedSha)
    , mode_(mode)
    , segmentBytes_(segmentBytes == 0 ? (std::size_t(1) << 20) : segmentBytes)
{
    if (blob_.ok && mode_ == IntegrityMode::SegmentedMerkle) {
        buildSegmentTable();
    }
}

void MappedVoxelSource::buildSegmentTable()
{
    // Hash each fixed-size segment once. In a real M8b-2 this table would be a
    // persisted .merkle sidecar (with a root); here we derive it from the mapped
    // bytes at construction. This one pass is NOT counted in stats_ (it models
    // pre-computed sidecar metadata, not per-acquire work).
    const std::size_t total = blob_.size;
    const std::size_t segCount = (total + segmentBytes_ - 1) / segmentBytes_;
    segmentSha_.reserve(segCount);
    segmentVerified_.assign(segCount, false);
    const unsigned char* base = static_cast<const unsigned char*>(blob_.base);
    for (std::size_t s = 0; s < segCount; ++s) {
        const std::size_t off = s * segmentBytes_;
        const std::size_t len = std::min(segmentBytes_, total - off);
        segmentSha_.push_back(Sha256::hashHex(base + off, len));
    }
}

bool MappedVoxelSource::verifyRange(std::size_t begin, std::size_t end) const
{
    ++stats_.acquireCount;
    if (begin >= end || end > blob_.size) {
        return false;
    }
    const unsigned char* base = static_cast<const unsigned char*>(blob_.base);

    if (mode_ == IntegrityMode::FullVerify) {
        // Strategy A: whole-file hash once, memoized. Touches every byte -> no
        // laziness. Any acquire pays the full O(N) on first call.
        if (!wholeVerified_) {
            const std::string sha = Sha256::hashHex(base, blob_.size);
            stats_.bytesHashed += blob_.size;
            if (sha != expectedSha_) {
                return false;
            }
            wholeVerified_ = true;
        }
        return true;
    }

    // Strategy B: verify only the segments overlapping [begin, end), each once.
    const std::size_t first = begin / segmentBytes_;
    const std::size_t last = (end - 1) / segmentBytes_;
    for (std::size_t s = first; s <= last; ++s) {
        if (segmentVerified_[s]) {
            continue;
        }
        const std::size_t off = s * segmentBytes_;
        const std::size_t len = std::min(segmentBytes_, blob_.size - off);
        const std::string sha = Sha256::hashHex(base + off, len);
        stats_.bytesHashed += len;
        ++stats_.segmentsChecked;
        if (sha != segmentSha_[s]) {
            return false;
        }
        segmentVerified_[s] = true;
    }
    return true;
}

VoxelMeta MappedVoxelSource::meta() const
{
    VoxelMeta m;
    if (!blob_.ok) {
        return m;
    }
    m.valid = true;
    m.type = ScalarType::UInt8;
    m.dims[0] = dims_[0];
    m.dims[1] = dims_[1];
    m.dims[2] = dims_[2];
    m.components = 1;
    m.voxelCount = voxelCount_;
    return m;
}

VoxelLease MappedVoxelSource::acquire_whole() const
{
    if (!blob_.ok || blob_.size < voxelCount_) {
        return VoxelLease::invalid();
    }
    // Whole-volume acquire touches every byte, so both strategies verify the
    // full range here. Zero-copy: the view points at the mapped pages and the
    // lease keepalive holds the mapping alive.
    if (!verifyRange(0, voxelCount_)) {
        return VoxelLease::invalid();
    }
    VoxelView view;
    view.valid = true;
    view.type = ScalarType::UInt8;
    view.dims[0] = dims_[0];
    view.dims[1] = dims_[1];
    view.dims[2] = dims_[2];
    view.components = 1;
    view.bytes = ReadSpan<std::uint8_t>(
        static_cast<const std::uint8_t*>(blob_.base), voxelCount_);
    return VoxelLease::borrow(blob_.keepalive, view);
}

VoxelLease MappedVoxelSource::acquire_region(const int extent[6]) const
{
    if (!blob_.ok) {
        return VoxelLease::invalid();
    }
    const int x0 = extent[0], x1 = extent[1];
    const int y0 = extent[2], y1 = extent[3];
    const int z0 = extent[4], z1 = extent[5];
    if (x0 < 0 || y0 < 0 || z0 < 0 || x1 < x0 || y1 < y0 || z1 < z0
        || x1 >= dims_[0] || y1 >= dims_[1] || z1 >= dims_[2]) {
        return VoxelLease::invalid();
    }

    // The touched byte range spans from the first voxel (x0,y0,z0) to the last
    // (x1,y1,z1) in x-fastest linear order. For SegmentedMerkle this is the key
    // measurement: only segments overlapping this range get hashed, so a small
    // region stays lazy even though the blob is huge.
    auto lin = [this](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x)
            + static_cast<std::size_t>(dims_[0])
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(dims_[1]) * static_cast<std::size_t>(z));
    };
    const std::size_t firstByte = lin(x0, y0, z0);
    const std::size_t lastByte = lin(x1, y1, z1);
    if (!verifyRange(firstByte, lastByte + 1)) {
        return VoxelLease::invalid();
    }

    // Materialize the sub-block x-fastest (region is not contiguous in the
    // source; same as ResidentVoxelSource). NOTE for the report: a strided view
    // would avoid this copy but the M8b-1 ReadSpan has no stride -> recorded as
    // an interface-shape gap.
    const int subX = x1 - x0 + 1;
    const int subY = y1 - y0 + 1;
    const int subZ = z1 - z0 + 1;
    const std::size_t subVoxels = static_cast<std::size_t>(subX)
        * static_cast<std::size_t>(subY) * static_cast<std::size_t>(subZ);
    std::vector<std::uint8_t> out(subVoxels);
    const std::uint8_t* base = static_cast<const std::uint8_t*>(blob_.base);
    std::size_t dst = 0;
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            const std::size_t rowStart = lin(x0, y, z);
            std::memcpy(out.data() + dst, base + rowStart, static_cast<std::size_t>(subX));
            dst += static_cast<std::size_t>(subX);
        }
    }
    const int subDims[3] = {subX, subY, subZ};
    return VoxelLease::own(std::move(out), ScalarType::UInt8, subDims, 1);
}

VoxelLease MappedVoxelSource::acquire_slab(int z) const
{
    if (!blob_.ok || z < 0 || z >= dims_[2]) {
        return VoxelLease::invalid();
    }
    const int extent[6] = {0, dims_[0] - 1, 0, dims_[1] - 1, z, z};
    return acquire_region(extent);
}

} // namespace probe
} // namespace xq
