#ifndef XQ_IO_BLOB_MERKLE_SIDECAR_H
#define XQ_IO_BLOB_MERKLE_SIDECAR_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xq {

// Segmented-hash sidecar for a content-addressed blob (M8b-2 decision A).
//
// A blob's bytes are split into fixed-size segments (the trailing segment may be
// short). Each segment gets its own SHA-256; the Merkle root is the SHA-256 of
// the concatenation of all per-segment hex digests (a single flat level is
// enough for this milestone). The sidecar is a small text file living next to
// the blob (e.g. "<blob>.merkle"):
//
//   XQMERKLE
//   version 1
//   segmentBytes <N>
//   segCount <M>
//   blobBytes <B>
//   root <64-hex>
//   seg <64-hex>        (M lines, segment 0 .. M-1)
//
// The read path maps the blob once, recomputes the root from the listed segment
// digests and compares (cheap, O(segments)); then per acquire it hashes only the
// touched segment(s) and compares against the listed digest (lazy integrity).
struct MerkleSidecar {
    std::uint64_t segmentBytes = 0;
    std::uint64_t segCount = 0;
    std::uint64_t blobBytes = 0;
    std::string root;                 // 64-char lower hex
    std::vector<std::string> segShas; // segCount entries, each 64-char lower hex

    // Recompute the root from segShas and compare to the stored root. Returns
    // false if segShas is empty or any digest length is wrong.
    bool verify_root() const;

    // Hash the segment at segIndex inside the mapped blob [base, base+blobBytes)
    // and compare to segShas[segIndex]. Out-of-range index or a base/size that
    // disagrees with the sidecar returns false.
    bool verify_segment(const void* base, std::size_t mappedSize,
                        std::uint64_t segIndex) const;
};

// Compute the flat-Merkle root for the given per-segment hex digests: the
// SHA-256 of their in-order concatenation. Exposed for write + verify reuse.
std::string merkle_root_of(const std::vector<std::string>& segShas);

// Build (in memory) the sidecar for a blob: split into segmentBytes-sized
// segments, hash each, compute the root. segmentBytes must be > 0.
MerkleSidecar build_sidecar(const void* blobBytes, std::size_t blobSize,
                            std::uint64_t segmentBytes);

// Build the sidecar for `blobBytes` and write it to outMerklePath. Returns false
// on a bad argument or any file-write error.
bool write_sidecar(const std::vector<std::uint8_t>& blobBytes,
                   std::uint64_t segmentBytes, const std::string& outMerklePath);

// Build the sidecar for the blob at blobPath and write it to outMerklePath.
bool write_sidecar_for_file(const std::string& blobPath,
                            std::uint64_t segmentBytes,
                            const std::string& outMerklePath);

// Parse a sidecar file. Returns false (and leaves *out untouched) on a missing
// file, bad magic/version, or a malformed body (e.g. segCount != listed digests).
bool load_sidecar(const std::string& merklePath, MerkleSidecar* out);

} // namespace xq

#endif // XQ_IO_BLOB_MERKLE_SIDECAR_H
