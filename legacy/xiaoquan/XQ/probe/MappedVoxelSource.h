#ifndef XQ_PROBE_MAPPED_VOXEL_SOURCE_H
#define XQ_PROBE_MAPPED_VOXEL_SOURCE_H

// scale-probe — real Win32 mmap-backed IVoxelSource (task 06-30-scale-probe, R2).
// Disposable spike code; only compiles under -DXQ_ENABLE_SCALE_PROBE=ON.
//
// Maps an M8a-format voxel blob (U8, x-fastest) read-only and exposes it through
// the M8b-1 IVoxelSource contract. acquire_whole() is a true zero-copy borrow:
// the VoxelView.bytes ReadSpan points straight at the mapped pages, and the
// VoxelLease keepalive holds the mmap alive (drop the last lease => UnmapViewOfFile).
//
// Integrity is the crux this probe exists to break (design.md §4): the mainline
// BlobStore verifies by reading the WHOLE file into the heap and hashing every
// byte (BlobStore.cpp:222-265), which defeats mmap laziness. We implement two
// strategies and let the probe measure them:
//   - FullVerify (A): hash the entire mapped region once before handing out any
//     view. Touches every byte -> no laziness, but a single trusted blob.
//   - SegmentedMerkle (B): pre-split the blob into fixed segments with per-segment
//     SHA-256; on each acquire, verify ONLY the segments overlapping the touched
//     byte range. Laziness preserved: working set scales with touched bytes.

#include "MmapBlob.h"

#include "core/XQImageVolume.h"          // ScalarType
#include "core/source/IVoxelSource.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xq {
namespace probe {

enum class IntegrityMode {
    FullVerify,      // strategy A: whole-file SHA before first access
    SegmentedMerkle  // strategy B: per-segment SHA, verify only touched segments
};

// Counters the probe reads back to prove which strategy actually stayed lazy.
struct IntegrityStats {
    std::uint64_t bytesHashed = 0;     // total bytes fed to SHA across all acquires
    std::uint64_t segmentsChecked = 0; // segments verified (B only)
    std::uint64_t acquireCount = 0;
};

class MappedVoxelSource : public IVoxelSource {
public:
    // Maps `blobPath` read-only. `dims` is x,y,z; voxels are U8, 1 component,
    // x-fastest (matches the synthetic generator + VoxelView layout). `expectedSha`
    // is the blob's whole-file SHA-256 (from its BufferRef) for FullVerify and for
    // building the segment table in SegmentedMerkle. `segmentBytes` is the merkle
    // segment size (B only). On a mapping failure, valid() is false and every
    // acquire returns an invalid lease.
    MappedVoxelSource(const std::string& blobPath,
                      const int dims[3],
                      const std::string& expectedSha,
                      IntegrityMode mode,
                      std::size_t segmentBytes = (std::size_t(1) << 20));

    bool valid() const { return blob_.ok; }
    unsigned long lastError() const { return blob_.lastError; }
    const IntegrityStats& stats() const { return stats_; }

    // IVoxelSource
    VoxelMeta meta() const override;
    VoxelLease acquire_whole() const override;
    VoxelLease acquire_region(const int extent[6]) const override;
    VoxelLease acquire_slab(int z) const override;

private:
    // Verify the byte range [begin, end) per the active integrity mode. Returns
    // false if any covered segment / the whole file fails its hash. Updates stats_.
    bool verifyRange(std::size_t begin, std::size_t end) const;

    // Build the per-segment SHA table (B) by hashing the mapped bytes once at
    // construction (simulating a .merkle sidecar that a real M8b-2 would persist).
    void buildSegmentTable();

    MmapBlob blob_;
    int dims_[3];
    std::size_t voxelCount_;
    std::string expectedSha_;
    IntegrityMode mode_;
    std::size_t segmentBytes_;
    std::vector<std::string> segmentSha_; // B: one SHA-256 hex per segment
    mutable std::vector<bool> segmentVerified_; // B: memoize verified segments
    mutable bool wholeVerified_ = false;        // A: memoize whole-file verify
    mutable IntegrityStats stats_;
};

} // namespace probe
} // namespace xq

#endif // XQ_PROBE_MAPPED_VOXEL_SOURCE_H
