#ifndef XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H
#define XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H

#include "core/NodeId.h"
#include "services/segmentation/SegmentationService.h"
#include "core/XQAiSegmentationRequest.h"

#include <string>

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the segmentation stage. It collects the user's
// intent (image + buffer + algorithm parameters), asks SegmentationService (or
// AiService for the AI path) for a mask, builds the mask-insertion command, and
// commits it through the command stack. It implements no segmentation algorithm
// and never mutates the scene directly.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class SegmentationController {
public:
    SegmentationController(XQScene* scene, XQCommandStack* stack);

    enum class Status {
        Ok,
        Rejected,    // the service rejected the intent (no scene change)
        NullScene,
    };

    // Threshold segmentation. The image / buffer are the decoded source volume;
    // sourceImageNode binds the mask back to its image node in the tree.
    struct ThresholdIntent {
        NodeId newMaskId;
        std::string name;
        NodeId sourceImageNode;
        const XQImageVolume* image = nullptr;
        const XQMemoryImageBufferHandle* buffer = nullptr;
        XQSegmentationThresholdParameters params;
    };
    Status threshold(const ThresholdIntent& intent);

    // Region-growing segmentation from a voxel seed.
    struct RegionGrowIntent {
        NodeId newMaskId;
        std::string name;
        NodeId sourceImageNode;
        const XQImageVolume* image = nullptr;
        const XQMemoryImageBufferHandle* buffer = nullptr;
        XQRegionGrowingParameters params;
    };
    Status regionGrow(const RegionGrowIntent& intent);

    // AI segmentation via a backend (the real one is ONNX; tests pass a mock).
    struct AiSegmentIntent {
        NodeId newMaskId;
        std::string name;
        NodeId sourceImageNode;
        const XQImageVolume* image = nullptr;
        const XQMemoryImageBufferHandle* buffer = nullptr;
        XQAiSegmentationRequest request;
    };
    Status aiSegment(const AiSegmentIntent& intent, XQAiSegmentationBackend& backend);

private:
    // Builds + pushes the mask-insertion command for an already-computed mask.
    Status commitMask(const NodeId& newMaskId,
                      const std::string& name,
                      const std::shared_ptr<XQSegmentationMask>& mask);

    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H
