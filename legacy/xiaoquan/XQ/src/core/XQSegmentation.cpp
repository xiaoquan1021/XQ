#include "core/XQSegmentation.h"

namespace xq {
namespace {

std::size_t voxel_count(const ImageGeometry& geometry)
{
    if (geometry.dimensions[0] <= 0
        || geometry.dimensions[1] <= 0
        || geometry.dimensions[2] <= 0) {
        return 0;
    }

    return static_cast<std::size_t>(geometry.dimensions[0])
        * static_cast<std::size_t>(geometry.dimensions[1])
        * static_cast<std::size_t>(geometry.dimensions[2]);
}

} // namespace

SegmentationMaskHandle::SegmentationMaskHandle()
    : voxelCount_(0)
    , labelCount_(0)
{
}

std::size_t SegmentationMaskHandle::voxelCount() const
{
    return voxelCount_;
}

std::size_t SegmentationMaskHandle::labelCount() const
{
    return labelCount_;
}

void SegmentationMaskHandle::setVoxelCount(std::size_t voxelCount)
{
    voxelCount_ = voxelCount;
}

void SegmentationMaskHandle::setLabelCount(std::size_t labelCount)
{
    labelCount_ = labelCount;
}

XQSegmentation::XQSegmentation()
    : id_(SegmentationId::invalid())
    , hasSourceImage_(false)
    , sourceImageNode_(NodeId::invalid())
    , geometry_()
    , geometrySet_(false)
    , mask_()
    , labels_()
{
}

void XQSegmentation::setId(SegmentationId id)
{
    id_ = id;
}

SegmentationId XQSegmentation::id() const
{
    return id_;
}

void XQSegmentation::setSourceImageNode(const NodeId& node)
{
    sourceImageNode_ = node;
    hasSourceImage_ = true;
}

bool XQSegmentation::hasSourceImageNode() const
{
    return hasSourceImage_;
}

NodeId XQSegmentation::sourceImageNode() const
{
    if (!hasSourceImage_) {
        return NodeId::invalid();
    }
    return sourceImageNode_;
}

void XQSegmentation::setGeometry(const ImageGeometry& g)
{
    geometry_ = g;
    geometrySet_ = true;
    updateMaskCounts();
}

const ImageGeometry& XQSegmentation::geometry() const
{
    return geometry_;
}

void XQSegmentation::setMask(std::shared_ptr<SegmentationMaskHandle> mask)
{
    mask_ = mask;
    updateMaskCounts();
}

std::shared_ptr<SegmentationMaskHandle> XQSegmentation::maskHandle() const
{
    return mask_;
}

void XQSegmentation::setLabels(const std::vector<SegmentationLabel>& labels)
{
    labels_ = labels;
    updateMaskCounts();
}

const std::vector<SegmentationLabel>& XQSegmentation::labels() const
{
    return labels_;
}

void XQSegmentation::updateMaskCounts()
{
    if (!mask_) {
        return;
    }

    mask_->setVoxelCount(geometrySet_ ? voxel_count(geometry_) : 0);
    mask_->setLabelCount(labels_.size());
}

} // namespace xq
