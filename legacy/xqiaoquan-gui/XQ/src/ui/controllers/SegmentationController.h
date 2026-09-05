#ifndef XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H
#define XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQAiSegmentationRequest.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"

#include <memory>
#include <string>

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class IVoxelSource;
class IVascularPreprocessor;
class IVascularRoiPriorReader;
class IAutomaticVesselSegmenter;
class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the segmentation stage. It collects the user's
// intent, asks the injected XQ adapter contracts for a mask, builds the
// mask-insertion command, and commits it through the command stack. It
// implements no segmentation algorithm and never mutates the scene directly.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class SegmentationController {
public:
    SegmentationController(
        XQScene* scene,
        XQCommandStack* stack,
        IVascularPreprocessor* vascularPreprocessor = nullptr,
        IVascularRoiPriorReader* roiPriorReader = nullptr,
        IAutomaticVesselSegmenter* automaticVesselSegmenter = nullptr);

    enum class Status {
        Ok,
        Rejected,
        PreprocessFailed,
        RoiPriorFailed,
        SegmentationFailed,
        NullScene,
    };

    // A computed-but-not-committed command: prepare*() runs the service work
    // (safe on a worker thread -- pure computation over the copied intent) and
    // the caller pushes the command on the GUI thread.
    struct PreparedCommand {
        Status status = Status::Rejected;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const { return status == Status::Ok && command != nullptr; }
    };

    // Pushes a prepared command on the stack (GUI-thread half of prepare*).
    bool commitPrepared(std::unique_ptr<XQCommand> command)
    {
        return stack_ != nullptr && command != nullptr
            && stack_->push(std::move(command));
    }

    // The only three-dimensional product segmentation intent. The two ROI paths
    // are explicit offline TotalSegmentator outputs; no environment discovery,
    // threshold controls, seed controls, or legacy fallback participates.
    struct AutomaticVesselIntent {
        NodeId newMaskId;
        std::string name;
        NodeId sourceImageNode;
        const XQImageVolume* image = nullptr;
        const IVoxelSource* source = nullptr;
        std::string liverRoiPath;
        std::string coarseVesselRoiPath;
    };
    PreparedCommand prepareAutomaticVesselSegmentation(
        const AutomaticVesselIntent& intent);
    Status automaticVesselSegmentation(const AutomaticVesselIntent& intent);

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
    IVascularPreprocessor* vascularPreprocessor_;
    IVascularRoiPriorReader* roiPriorReader_;
    IAutomaticVesselSegmenter* automaticVesselSegmenter_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_SEGMENTATION_CONTROLLER_H
