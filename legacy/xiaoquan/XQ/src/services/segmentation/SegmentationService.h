#ifndef XQ_SERVICES_SEGMENTATION_SEGMENTATION_SERVICE_H
#define XQ_SERVICES_SEGMENTATION_SEGMENTATION_SERVICE_H

#include "core/NodeId.h"
#include "core/command/XQCommand.h"
#include "core/XQAiSegmentationRequest.h"

#include <memory>
#include <string>

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class XQSegmentationMask;
class XQScene;

// Threshold segmentation parameters: a voxel is foreground when its scalar value
// is within [lower, upper] (inclusive). foregroundLabel is the label written for
// foreground voxels (must be in [1, 255]; 0 is reserved for background, and the
// label is stored as a uint8 so values outside that range are rejected rather
// than silently truncated).
struct XQSegmentationThresholdParameters {
    double lower = 0.0;
    double upper = 0.0;
    int foregroundLabel = 1;
};

// Region-growing parameters: starting from a voxel seed, flood-fill across
// 6-connected neighbours whose scalar value is within [lower, upper]. The seed
// must lie inside the image extent and itself satisfy the threshold.
struct XQRegionGrowingParameters {
    int seed[3] = {0, 0, 0};
    double lower = 0.0;
    double upper = 0.0;
    int foregroundLabel = 1;
};

// Pure domain service for deriving segmentation masks from images. Like the M1
// path service it computes results and (for scene insertion) returns a command;
// it never mutates the scene and holds no scene state. The traditional
// algorithms are XQ's own code with zero external dependencies (no ITK/VTK/Qt).
//
// Mask-producing methods return a Result carrying a status and a shared mask
// (null on validation failure) instead of throwing.
class SegmentationService {
public:
    enum class Status {
        Ok,
        InvalidBuffer,        // buffer is invalid, or its dims/type mismatch the image
        NotSingleComponent,   // image / buffer is not single-component
        InvalidThreshold,     // lower > upper
        InvalidLabel,         // foreground label is 0 (reserved) or out of [1, 255]
        SeedOutOfRange,       // region-grow seed lies outside the image extent
        SeedNotInThreshold,   // region-grow seed does not satisfy the threshold
        InvalidMask,          // input mask is invalid
        NullScene,            // scene pointer was null
    };

    struct Result {
        Status status;
        std::shared_ptr<XQSegmentationMask> mask; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    struct CommandResult {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    // Foreground = voxels with scalar in [lower, upper]. The mask carries the
    // image geometry and (if present) the image's node id as source. The image's
    // node id is passed separately because XQImageVolume itself holds no node id.
    static Result thresholdMask(const XQImageVolume& image,
                                const XQMemoryImageBufferHandle& buffer,
                                const XQSegmentationThresholdParameters& params,
                                const NodeId& sourceImageNode = NodeId::invalid());

    // 6-connected region grow within [lower, upper] starting at params.seed.
    static Result regionGrowMask(const XQImageVolume& image,
                                 const XQMemoryImageBufferHandle& buffer,
                                 const XQRegionGrowingParameters& params,
                                 const NodeId& sourceImageNode = NodeId::invalid());

    // Keeps only the largest 6-connected foreground component, clearing the
    // rest. A mask with no foreground returns an empty mask that preserves the
    // input mask's geometry and source image node.
    static Result keepLargestConnectedComponent(const XQSegmentationMask& mask);

    // Runs the AI backend over the image/buffer/request. The backend returns the
    // mask; the service binds the source image node onto it if not already set.
    static Result aiSegmentMask(const XQImageVolume& image,
                                const XQMemoryImageBufferHandle& buffer,
                                const XQAiSegmentationRequest& request,
                                XQAiSegmentationBackend& backend,
                                const NodeId& sourceImageNode = NodeId::invalid());

    // Builds the command that inserts a mask node. If the mask carries a source
    // image node, returns an AddNodeWithSourceRelationCommand (preserving the
    // ImageToSegmentationMask relation); otherwise an AddNodeCommand. The caller
    // assigns newMaskId (the service holds no id allocator).
    static CommandResult createMaskNodeCommand(XQScene* scene,
                                               const NodeId& newMaskId,
                                               const std::string& name,
                                               const std::shared_ptr<XQSegmentationMask>& mask);
};

} // namespace xq

#endif // XQ_SERVICES_SEGMENTATION_SEGMENTATION_SERVICE_H
