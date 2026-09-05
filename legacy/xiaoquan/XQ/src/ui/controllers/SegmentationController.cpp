#include "ui/controllers/SegmentationController.h"

#include "core/XQSegmentationMask.h"
#include "core/command/XQCommandStack.h"
#include "services/ai/AiService.h"

#include <utility>

namespace xq {

SegmentationController::SegmentationController(XQScene* scene, XQCommandStack* stack)
    : scene_(scene)
    , stack_(stack)
{
}

SegmentationController::Status SegmentationController::commitMask(
    const NodeId& newMaskId,
    const std::string& name,
    const std::shared_ptr<XQSegmentationMask>& mask)
{
    if (mask == nullptr) {
        return Status::Rejected;
    }
    SegmentationService::CommandResult cmd =
        SegmentationService::createMaskNodeCommand(scene_, newMaskId, name, mask);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }
    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

SegmentationController::Status SegmentationController::threshold(const ThresholdIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }
    if (intent.image == nullptr || intent.buffer == nullptr) {
        return Status::Rejected;
    }

    SegmentationService::Result result = SegmentationService::thresholdMask(
        *intent.image, *intent.buffer, intent.params, intent.sourceImageNode);
    if (!result.ok()) {
        return Status::Rejected;
    }
    return commitMask(intent.newMaskId, intent.name, result.mask);
}

SegmentationController::Status SegmentationController::regionGrow(const RegionGrowIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }
    if (intent.image == nullptr || intent.buffer == nullptr) {
        return Status::Rejected;
    }

    SegmentationService::Result result = SegmentationService::regionGrowMask(
        *intent.image, *intent.buffer, intent.params, intent.sourceImageNode);
    if (!result.ok()) {
        return Status::Rejected;
    }
    return commitMask(intent.newMaskId, intent.name, result.mask);
}

SegmentationController::Status SegmentationController::aiSegment(const AiSegmentIntent& intent,
                                                                XQAiSegmentationBackend& backend)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }
    if (intent.image == nullptr || intent.buffer == nullptr) {
        return Status::Rejected;
    }

    AiService::SegmentResult result = AiService::segment(
        *intent.image, *intent.buffer, intent.request, backend);
    if (!result.ok()) {
        return Status::Rejected;
    }
    // Bind the source image node onto the mask if the backend left it unset, so
    // the inserted node carries the image->mask relation.
    if (result.mask != nullptr && !result.mask->hasSourceImageNode()
        && intent.sourceImageNode.is_valid()) {
        result.mask->setSourceImageNode(intent.sourceImageNode);
    }
    return commitMask(intent.newMaskId, intent.name, result.mask);
}

} // namespace xq
