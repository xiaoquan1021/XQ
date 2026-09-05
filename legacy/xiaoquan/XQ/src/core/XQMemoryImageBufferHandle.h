#ifndef XQ_CORE_XQ_MEMORY_IMAGE_BUFFER_HANDLE_H
#define XQ_CORE_XQ_MEMORY_IMAGE_BUFFER_HANDLE_H

#include "core/XQImageVolume.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xq {

// Real in-memory scalar buffer for an image volume. core's ImageBufferHandle is
// a counts-only handle (no pixels), but segmentation algorithms must read scalar
// values per voxel, so M2 adds this XQ-owned buffer.
//
// Holds raw bytes plus the scalar type, voxel dimensions and component count.
// Synthetic test data and adapter-decoded real images (io/adapter decodes into
// this) both use it. Zero external dependencies.
//
// Layout follows VTK/ITK point-scalar order: x fastest, then y, then z, with
// components interleaved per voxel (c0 c1 ... for voxel 0, then voxel 1, ...).
class XQMemoryImageBufferHandle {
public:
    // Number of bytes one scalar of the given type occupies. Returns 0 for
    // ScalarType::Unknown.
    static std::size_t scalarSize(ScalarType type);

    // Constructs a buffer that owns the given bytes. The buffer is valid only
    // when type != Unknown, every dimension is >= 1, componentCount >= 1, and
    // bytes.size() == voxelCount * componentCount * scalarSize(type). A size
    // mismatch (or any invalid parameter) yields an invalid buffer (is_valid()
    // == false) rather than throwing, matching core's "explicit result, no
    // exceptions through UI" contract.
    XQMemoryImageBufferHandle(ScalarType type,
                              const int dimensions[3],
                              int componentCount,
                              std::vector<std::uint8_t> bytes);

    bool is_valid() const;

    ScalarType scalarType() const;
    int dimensionX() const;
    int dimensionY() const;
    int dimensionZ() const;
    int componentCount() const;

    // Number of voxels = dimX * dimY * dimZ (0 if invalid).
    std::size_t voxelCount() const;

    const std::vector<std::uint8_t>& bytes() const;

    // Linear index of voxel (x, y, z): x + dimX * (y + dimY * z). Does not
    // bounds-check; callers validate against the dimensions.
    std::size_t voxelIndex(int x, int y, int z) const;

    // Reads component `component` of the voxel at linear index `voxelIndex`,
    // returned as double (the raw stored value; rescale slope/intercept are an
    // image-volume concern, not applied here). Out-of-range index/component or an
    // invalid buffer return 0.0.
    double scalarAt(std::size_t voxelIndex, int component = 0) const;

private:
    ScalarType type_;
    int dimensions_[3];
    int componentCount_;
    std::vector<std::uint8_t> bytes_;
    bool valid_;
};

} // namespace xq

#endif // XQ_CORE_XQ_MEMORY_IMAGE_BUFFER_HANDLE_H
