#ifndef XQ_CORE_XQ_SEGMENTATION_H
#define XQ_CORE_XQ_SEGMENTATION_H

#include "core/NodeId.h"
#include "core/XQImageVolume.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace xq {

using SegmentationId = NodeId;

struct SegmentationLabel {
    int value;
    std::string name;
};

class XQSegmentation;

class SegmentationMaskHandle {
public:
    SegmentationMaskHandle();

    std::size_t voxelCount() const;
    std::size_t labelCount() const;

private:
    friend class XQSegmentation;

    void setVoxelCount(std::size_t voxelCount);
    void setLabelCount(std::size_t labelCount);

    std::size_t voxelCount_;
    std::size_t labelCount_;
};

class XQSegmentation {
public:
    XQSegmentation();

    void setId(SegmentationId id);
    SegmentationId id() const;

    void setSourceImageNode(const NodeId& node);
    bool hasSourceImageNode() const;
    NodeId sourceImageNode() const;

    void setGeometry(const ImageGeometry& g);
    const ImageGeometry& geometry() const;

    void setMask(std::shared_ptr<SegmentationMaskHandle> mask);
    std::shared_ptr<SegmentationMaskHandle> maskHandle() const;

    void setLabels(const std::vector<SegmentationLabel>& labels);
    const std::vector<SegmentationLabel>& labels() const;

private:
    void updateMaskCounts();

    SegmentationId id_;
    bool hasSourceImage_;
    NodeId sourceImageNode_;
    ImageGeometry geometry_;
    bool geometrySet_;
    std::shared_ptr<SegmentationMaskHandle> mask_;
    std::vector<SegmentationLabel> labels_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SEGMENTATION_H
