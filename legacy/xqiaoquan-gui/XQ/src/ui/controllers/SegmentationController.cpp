#include "ui/controllers/SegmentationController.h"

#include "core/XQDataNode.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQSceneCommands.h"
#include "core/image/IAutomaticVesselSegmenter.h"
#include "core/image/IVascularPreprocessor.h"
#include "core/image/IVascularRoiPriorReader.h"
#include "services/ai/AiService.h"

#include <utility>
#include <vector>

namespace xq {

namespace {

std::unique_ptr<XQCommand> makeMaskNodeCommand(
    XQScene* scene,
    const NodeId& newMaskId,
    const std::string& name,
    const std::shared_ptr<XQSegmentationMask>& mask)
{
    if (scene == nullptr || mask == nullptr || !mask->is_valid()) {
        return nullptr;
    }
    auto payload = std::make_shared<XQSegmentationMaskPayload>(*mask);
    XQDataNode node(newMaskId, XQDomainType::SegmentationMask, name, payload);
    if (mask->hasSourceImageNode()) {
        return std::unique_ptr<XQCommand>(new AddNodeWithSourceRelationCommand(
            scene, node, mask->sourceImageNode(), "Add segmentation mask"));
    }
    return std::unique_ptr<XQCommand>(
        new AddNodeCommand(scene, node, "Add segmentation mask"));
}

} // namespace

SegmentationController::SegmentationController(
    XQScene* scene,
    XQCommandStack* stack,
    IVascularPreprocessor* vascularPreprocessor,
    IVascularRoiPriorReader* roiPriorReader,
    IAutomaticVesselSegmenter* automaticVesselSegmenter)
    : scene_(scene)
    , stack_(stack)
    , vascularPreprocessor_(vascularPreprocessor)
    , roiPriorReader_(roiPriorReader)
    , automaticVesselSegmenter_(automaticVesselSegmenter)
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
    std::unique_ptr<XQCommand> command =
        makeMaskNodeCommand(scene_, newMaskId, name, mask);
    if (command == nullptr) {
        return Status::Rejected;
    }
    return stack_->push(std::move(command)) ? Status::Ok : Status::Rejected;
}

SegmentationController::PreparedCommand
SegmentationController::prepareAutomaticVesselSegmentation(
    const AutomaticVesselIntent& intent)
{
    PreparedCommand prepared;
    if (scene_ == nullptr || stack_ == nullptr) {
        prepared.status = Status::NullScene;
        return prepared;
    }
    if (vascularPreprocessor_ == nullptr || roiPriorReader_ == nullptr
        || automaticVesselSegmenter_ == nullptr || intent.image == nullptr
        || intent.source == nullptr || !intent.newMaskId.is_valid()
        || !intent.sourceImageNode.is_valid() || intent.liverRoiPath.empty()
        || intent.coarseVesselRoiPath.empty()) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    const VascularPreprocessResult preprocessed = vascularPreprocessor_->run(
        *intent.image, *intent.source, portalVenousCtPreprocessProfileV1());
    if (!preprocessed.ok()) {
        prepared.status = Status::PreprocessFailed;
        return prepared;
    }

    const std::vector<VascularRoiFileInput> roiInputs = {
        {VascularRoiRole::Organ, intent.liverRoiPath,
         "TotalSegmentator", "2.15.0"},
        {VascularRoiRole::CoarseVessel, intent.coarseVesselRoiPath,
         "TotalSegmentator", "2.15.0"}
    };
    const VascularRoiPriorReadResult roi = roiPriorReader_->read(
        *intent.image, preprocessed.output->inputFingerprint, roiInputs);
    if (!roi.ok()) {
        prepared.status = Status::RoiPriorFailed;
        return prepared;
    }

    const AutomaticVesselSegmentationResult segmented =
        automaticVesselSegmenter_->run(
            *intent.image, *intent.source, *preprocessed.output, *roi.prior,
            portalVenousCtAutomaticSegmentationProfileV2());
    if (!segmented.ok() || segmented.output->mask == nullptr) {
        prepared.status = Status::SegmentationFailed;
        return prepared;
    }

    // Bind the materialized mask to the active image before the command is
    // created, so the command atomically publishes the node and source relation.
    segmented.output->mask->setSourceImageNode(intent.sourceImageNode);
    std::unique_ptr<XQCommand> command = makeMaskNodeCommand(
        scene_, intent.newMaskId, intent.name, segmented.output->mask);
    if (command == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    prepared.status = Status::Ok;
    prepared.command = std::move(command);
    return prepared;
}

SegmentationController::Status
SegmentationController::automaticVesselSegmentation(
    const AutomaticVesselIntent& intent)
{
    PreparedCommand prepared = prepareAutomaticVesselSegmentation(intent);
    if (!prepared.ok()) {
        return prepared.status;
    }
    return stack_->push(std::move(prepared.command)) ? Status::Ok : Status::Rejected;
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
