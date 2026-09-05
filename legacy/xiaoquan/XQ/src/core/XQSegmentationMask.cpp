#include "core/XQSegmentationMask.h"

namespace xq {

XQSegmentationMask::XQSegmentationMask(const int dimensions[3])
    : valid_(false)
    , hasGeometry_(false)
    , geometry_()
    , hasSourceImage_(false)
{
    dimensions_[0] = dimensions[0];
    dimensions_[1] = dimensions[1];
    dimensions_[2] = dimensions[2];

    if (dimensions_[0] < 1 || dimensions_[1] < 1 || dimensions_[2] < 1) {
        return;
    }

    const std::size_t voxels = static_cast<std::size_t>(dimensions_[0])
        * static_cast<std::size_t>(dimensions_[1])
        * static_cast<std::size_t>(dimensions_[2]);
    voxels_.assign(voxels, static_cast<LabelType>(0));
    valid_ = true;
}

bool XQSegmentationMask::is_valid() const
{
    return valid_;
}

int XQSegmentationMask::dimensionX() const
{
    return dimensions_[0];
}

int XQSegmentationMask::dimensionY() const
{
    return dimensions_[1];
}

int XQSegmentationMask::dimensionZ() const
{
    return dimensions_[2];
}

const int* XQSegmentationMask::dimensions() const
{
    return dimensions_;
}

std::size_t XQSegmentationMask::voxelCount() const
{
    return voxels_.size();
}

void XQSegmentationMask::setGeometry(const ImageGeometry& g)
{
    geometry_ = g;
    hasGeometry_ = true;
}

bool XQSegmentationMask::hasGeometry() const
{
    return hasGeometry_;
}

const ImageGeometry& XQSegmentationMask::geometry() const
{
    return geometry_;
}

void XQSegmentationMask::setSourceImageNode(const NodeId& node)
{
    sourceImageNode_ = node;
    hasSourceImage_ = true;
}

bool XQSegmentationMask::hasSourceImageNode() const
{
    return hasSourceImage_;
}

NodeId XQSegmentationMask::sourceImageNode() const
{
    return sourceImageNode_;
}

void XQSegmentationMask::setLabels(const std::vector<SegmentationLabel>& labels)
{
    labels_ = labels;
}

const std::vector<SegmentationLabel>& XQSegmentationMask::labels() const
{
    return labels_;
}

std::size_t XQSegmentationMask::voxelIndex(int x, int y, int z) const
{
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(dimensions_[0])
        * (static_cast<std::size_t>(y)
           + static_cast<std::size_t>(dimensions_[1]) * static_cast<std::size_t>(z));
}

XQSegmentationMask::LabelType XQSegmentationMask::labelAt(std::size_t voxelIndex) const
{
    if (voxelIndex >= voxels_.size()) {
        return 0;
    }
    return voxels_[voxelIndex];
}

void XQSegmentationMask::setLabelAt(std::size_t voxelIndex, LabelType label)
{
    if (voxelIndex >= voxels_.size()) {
        return;
    }
    voxels_[voxelIndex] = label;
}

const std::vector<XQSegmentationMask::LabelType>& XQSegmentationMask::voxels() const
{
    return voxels_;
}

std::size_t XQSegmentationMask::foregroundVoxelCount() const
{
    std::size_t count = 0;
    for (LabelType label : voxels_) {
        if (label != 0) {
            ++count;
        }
    }
    return count;
}

} // namespace xq
