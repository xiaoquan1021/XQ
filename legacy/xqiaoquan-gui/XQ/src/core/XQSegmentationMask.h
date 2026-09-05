#ifndef XQ_CORE_XQ_SEGMENTATION_MASK_H
#define XQ_CORE_XQ_SEGMENTATION_MASK_H

#include "core/NodeId.h"
#include "core/XQImageVolume.h"
#include "core/XQSegmentation.h" // SegmentationLabel

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xq {

// Real label volume for a derived segmentation mask. core's XQSegmentation owns
// only a counts-only SegmentationMaskHandle (no voxels); segmentation algorithms
// need to write and read actual per-voxel labels, so M2 adds this XQ-owned mask
// with a real label grid. New type on purpose: the handle-based XQSegmentation
// and its M0 tests stay untouched.
//
// One label value per voxel (0 == background by convention). Voxel layout
// matches XQMemoryImageBufferHandle: x fastest, then y, then z.
class XQSegmentationMask {
public:
    using LabelType = std::uint8_t;

    // Empty (background-only) mask of the given dimensions. All voxels start at
    // 0. dimensions must each be >= 1; otherwise the mask is invalid
    // (is_valid() == false, no allocation).
    explicit XQSegmentationMask(const int dimensions[3]);

    bool is_valid() const;

    int dimensionX() const;
    int dimensionY() const;
    int dimensionZ() const;
    // Pointer to the 3 voxel dimensions, e.g. to size another mask the same way.
    const int* dimensions() const;
    std::size_t voxelCount() const;

    void setGeometry(const ImageGeometry& g);
    bool hasGeometry() const;
    const ImageGeometry& geometry() const;

    void setSourceImageNode(const NodeId& node);
    bool hasSourceImageNode() const;
    NodeId sourceImageNode() const;

    void setLabels(const std::vector<SegmentationLabel>& labels);
    const std::vector<SegmentationLabel>& labels() const;

    // Linear index of voxel (x, y, z): x + dimX * (y + dimY * z).
    std::size_t voxelIndex(int x, int y, int z) const;

    // Reads / writes the label at a linear voxel index. labelAt on an
    // out-of-range index (or invalid mask) returns 0; setLabelAt ignores an
    // out-of-range index.
    LabelType labelAt(std::size_t voxelIndex) const;
    void setLabelAt(std::size_t voxelIndex, LabelType label);

    const std::vector<LabelType>& voxels() const;

    // Number of voxels whose label is non-zero (foreground).
    std::size_t foregroundVoxelCount() const;

private:
    int dimensions_[3];
    bool valid_;
    bool hasGeometry_;
    ImageGeometry geometry_;
    bool hasSourceImage_;
    NodeId sourceImageNode_;
    std::vector<SegmentationLabel> labels_;
    std::vector<LabelType> voxels_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SEGMENTATION_MASK_H
