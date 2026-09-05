#ifndef XQ_IO_SOURCE_MAPPED_VOXEL_SOURCE_H
#define XQ_IO_SOURCE_MAPPED_VOXEL_SOURCE_H

#include "core/XQImageVolume.h" // ScalarType
#include "core/source/IVoxelSource.h"
#include "io/blob/MerkleSidecar.h"
#include "io/source/MmapBlob.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace xq {

// Production IVoxelSource backed by a Win32 read-only memory mapping of a
// content-addressed voxel blob (M8b-2 §3.2). The whole-volume acquire is zero
// copy (borrow the mapped bytes, keepalive = the mmap control block); region and
// slab acquires materialize the requested sub-block (own). Integrity is checked
// lazily: only the segments a given acquire touches are hashed and compared
// against the .merkle sidecar (SegmentedMerkle), or the whole blob is hashed
// once on first acquire (FullVerify, the M8a fallback for blobs without a
// sidecar). A failed integrity check yields an invalid lease.
//
// This class never touches BlobStore::get (which re-reads + re-hashes the whole
// blob and decodes element-by-element); it maps once and reinterprets.
class MappedVoxelSource : public IVoxelSource {
public:
    enum class IntegrityMode {
        FullVerify,     // hash whole blob once on first touch (no sidecar)
        SegmentedMerkle // per-segment lazy hashing against a .merkle sidecar
    };

    // Counters exposed for tests to prove laziness (AC4) and caching.
    struct IntegrityStats {
        std::uint64_t bytesHashed = 0;     // total bytes fed to SHA so far
        std::uint64_t segmentsChecked = 0; // distinct segments verified so far
        std::uint64_t acquireCount = 0;    // acquire_* calls served
    };

    // SegmentedMerkle requires a readable, root-consistent sidecar whose
    // blobBytes matches the mapped file size; otherwise the source is invalid.
    // FullVerify ignores merklePath. expectedSha256 (optional) is the blob's
    // content-address anchor (BufferRef.sha256, lower-case hex): under
    // FullVerify the whole-blob digest is compared against it on first touch and
    // a mismatch yields invalid leases (tamper detection). Empty = no anchor
    // (digest still computed + memoized, but not compared -- back-compat).
    MappedVoxelSource(const std::string& blobPath,
                      const int dims[3],
                      ScalarType type,
                      int components,
                      IntegrityMode mode,
                      const std::string& merklePath = std::string(),
                      const std::string& expectedSha256 = std::string());

    // True when the mapping (and, for SegmentedMerkle, the sidecar root) is
    // sound and the mapped size equals dims*components*scalarSize.
    bool valid() const { return valid_; }

    VoxelMeta meta() const override;
    VoxelLease acquire_whole() const override;
    VoxelLease acquire_region(const int extent[6]) const override;
    VoxelLease acquire_slab(int z) const override;

    IntegrityStats stats() const;

private:
    // Verify integrity over the byte range [byteBegin, byteEnd) (lazy, memoized).
    // Returns false if any covered segment fails its digest. Must be called with
    // no external lock; it takes the internal mutex.
    bool verify_range(std::size_t byteBegin, std::size_t byteEnd) const;

    std::size_t voxelStride() const; // bytes per voxel = components * scalarSize
    std::size_t totalBytes() const;  // voxelCount * voxelStride

    int dims_[3] = {0, 0, 0};
    ScalarType type_ = ScalarType::Unknown;
    int components_ = 1;
    IntegrityMode mode_ = IntegrityMode::FullVerify;
    std::string expectedSha256_; // content-address anchor; empty = no comparison

    MappedFile mapped_;
    bool valid_ = false;

    MerkleSidecar sidecar_;

    mutable std::mutex mutex_;
    mutable std::vector<bool> segChecked_; // SegmentedMerkle memo
    mutable bool fullVerified_ = false;    // FullVerify memo
    mutable IntegrityStats stats_;
};

} // namespace xq

#endif // XQ_IO_SOURCE_MAPPED_VOXEL_SOURCE_H
