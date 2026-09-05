#include "ui/controllers/FlowSmokeController.h"

#include "core/XQDataNode.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQProject.h"
#include "core/XQSimulationCasePayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"

#include <utility>

namespace xq {
namespace {

bool valid_output_asset(const AssetRecord& asset,
                        AssetKind expectedKind)
{
    return asset.id.is_valid()
        && asset.category == AssetCategory::Derived
        && asset.kind == expectedKind
        && !asset.contentFingerprint.empty();
}

} // namespace

FlowSmokeController::FlowSmokeController(
    XQProject* project,
    XQCommandStack* stack)
    : project_(project)
    , stack_(stack)
{
}

FlowSmokeController::Status FlowSmokeController::validateOutput(
    const OutputIntent& output) const
{
    if (project_ == nullptr || stack_ == nullptr) {
        return Status::NullContext;
    }
    if (project_->state() != XQProject::LifecycleState::Open) {
        return Status::ProjectNotOpen;
    }
    if (!output.caseNode.is_valid() || !output.resultNode.is_valid()
        || output.caseNode == output.resultNode
        || output.caseName.empty() || output.resultName.empty()) {
        return Status::InvalidIntent;
    }
    if (project_->scene().find(output.caseNode) != nullptr
        || project_->scene().find(output.resultNode) != nullptr) {
        return Status::TargetExists;
    }
    if (output.caseAsset.has_value()
        && (!valid_output_asset(
                output.caseAsset.value(), AssetKind::SimulationCase)
            || project_->assetRegistry().find(output.caseAsset->id) != nullptr)) {
        return Status::TargetExists;
    }
    if (output.resultAsset.has_value()
        && (!valid_output_asset(
                output.resultAsset.value(), AssetKind::FlowResult)
            || project_->assetRegistry().find(output.resultAsset->id) != nullptr)) {
        return Status::TargetExists;
    }
    if (output.caseAsset.has_value() && output.resultAsset.has_value()
        && output.caseAsset->id == output.resultAsset->id) {
        return Status::InvalidIntent;
    }
    return Status::Ok;
}

FlowSmokeController::CapturedInput FlowSmokeController::capture(
    const SmokeIntent& intent) const
{
    CapturedInput captured;
    captured.status = validateOutput(intent.output);
    if (captured.status != Status::Ok) {
        return captured;
    }
    if (!intent.sourceProfileNode.is_valid()
        || intent.sourceProfileNode == intent.output.caseNode
        || intent.sourceProfileNode == intent.output.resultNode) {
        captured.status = Status::InvalidIntent;
        return captured;
    }

    const XQDataNode* source = project_->scene().find(intent.sourceProfileNode);
    if (source == nullptr) {
        captured.status = Status::SourceNotFound;
        return captured;
    }
    if (source->domainType() != XQDomainType::VesselProfile) {
        captured.status = Status::SourceTypeMismatch;
        return captured;
    }
    if (project_->scene().is_stale(intent.sourceProfileNode)) {
        captured.status = Status::SourceStale;
        return captured;
    }
    const std::shared_ptr<XQVesselProfilePayload> payload =
        std::dynamic_pointer_cast<XQVesselProfilePayload>(source->payload());
    if (payload == nullptr) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }
    if (!source->hasScaleSlot() || source->scaleSlot() != ScaleSlot::Organ) {
        captured.status = Status::UnsupportedScale;
        return captured;
    }

    DerivationInputStamp stamp;
    stamp.nodeId = source->id();
    stamp.contentRevision = source->contentRevision();
    if (source->hasAssetId()) {
        const AssetRecord* asset = project_->assetRegistry().find(source->assetId());
        if (asset == nullptr || asset->kind != AssetKind::VesselProfile
            || asset->contentFingerprint.empty()) {
            captured.status = Status::SourceAssetInvalid;
            return captured;
        }
        stamp.assetId = source->assetId();
        stamp.assetFingerprint = asset->contentFingerprint;
    }

    captured.request_.profile = payload->profile();
    captured.request_.scaleSlot = ScaleSlot::Organ;
    captured.request_.sourceProfileNode = intent.sourceProfileNode;
    captured.request_.sourceProfileRevision = source->contentRevision();
    captured.request_.caseNode = intent.output.caseNode;
    captured.request_.resultNode = intent.output.resultNode;
    captured.output_ = intent.output;
    captured.projectGuard_.project = project_;
    captured.projectGuard_.lifecycleEpoch = project_->lifecycleEpoch();
    captured.sourceGuard_.stamp = stamp;
    captured.sourceGuard_.payloadIdentity = source->payload();
    captured.sourceGuard_.scaleSlot = source->scaleSlot();
    captured.status = Status::Ok;
    return captured;
}

FlowSmokeController::PreparedCommand FlowSmokeController::compute(
    CapturedInput captured)
{
    PreparedCommand prepared;
    if (!captured.ok()) {
        prepared.status = captured.status;
        return prepared;
    }

    FlowGeometrySmokeService::Result computed =
        FlowGeometrySmokeService::run(captured.request_);
    prepared.serviceStatus = computed.status;
    prepared.assemblyStatus = computed.assemblyStatus;
    prepared.solverStatus = computed.solverStatus;
    if (!computed.ok()) {
        prepared.status = Status::ComputeFailed;
        return prepared;
    }

    XQDataNode caseNode(
        captured.output_.caseNode,
        XQDomainType::SimulationCase,
        captured.output_.caseName,
        std::make_shared<XQSimulationCasePayload>(
            std::move(computed.simulationCase)));
    caseNode.setScaleSlot(ScaleSlot::Organ);
    ProjectNodeBatchSpec caseSpec(std::move(caseNode));
    caseSpec.sources.push_back(captured.sourceGuard_.stamp);
    if (captured.output_.caseAsset.has_value()) {
        caseSpec.assetToRegister = captured.output_.caseAsset;
        caseSpec.bindAsset = captured.output_.caseAsset->id;
    }

    XQDataNode resultNode(
        captured.output_.resultNode,
        XQDomainType::FlowResult,
        captured.output_.resultName,
        std::make_shared<XQFlowResultPayload>(std::move(computed.flowResult)));
    resultNode.setScaleSlot(ScaleSlot::Organ);
    ProjectNodeBatchSpec resultSpec(std::move(resultNode));
    resultSpec.sources.push_back(captured.sourceGuard_.stamp);
    DerivationInputStamp caseStamp;
    caseStamp.nodeId = captured.output_.caseNode;
    caseStamp.contentRevision = 0;
    if (captured.output_.caseAsset.has_value()) {
        caseStamp.assetId = captured.output_.caseAsset->id;
        caseStamp.assetFingerprint =
            captured.output_.caseAsset->contentFingerprint;
    }
    resultSpec.sources.push_back(caseStamp);
    if (captured.output_.resultAsset.has_value()) {
        resultSpec.assetToRegister = captured.output_.resultAsset;
        resultSpec.bindAsset = captured.output_.resultAsset->id;
    }

    prepared.specs_.push_back(std::move(caseSpec));
    prepared.specs_.push_back(std::move(resultSpec));
    prepared.projectGuard_ = captured.projectGuard_;
    prepared.sourceGuard_ = std::move(captured.sourceGuard_);
    prepared.output_ = std::move(captured.output_);
    prepared.status = Status::Ok;
    return prepared;
}

FlowSmokeController::PreparedCommand FlowSmokeController::prepare(
    const SmokeIntent& intent) const
{
    return compute(capture(intent));
}

bool FlowSmokeController::sourceStillCurrent(const SourceGuard& guard) const
{
    if (project_ == nullptr
        || project_->state() != XQProject::LifecycleState::Open) {
        return false;
    }
    const XQDataNode* source = project_->scene().find(guard.stamp.nodeId);
    if (source == nullptr
        || source->domainType() != XQDomainType::VesselProfile
        || source->contentRevision() != guard.stamp.contentRevision
        || source->payload() != guard.payloadIdentity
        || source->scaleSlot() != guard.scaleSlot
        || project_->scene().is_stale(guard.stamp.nodeId)) {
        return false;
    }
    if (guard.stamp.assetId.has_value()) {
        if (!source->hasAssetId()
            || source->assetId() != guard.stamp.assetId.value()) {
            return false;
        }
        const AssetRecord* asset = project_->assetRegistry().find(source->assetId());
        return asset != nullptr
            && asset->kind == AssetKind::VesselProfile
            && asset->contentFingerprint == guard.stamp.assetFingerprint;
    }
    return !source->hasAssetId();
}

bool FlowSmokeController::targetAssetsStillAvailable(
    const OutputIntent& output) const
{
    if (project_ == nullptr) {
        return false;
    }
    return (!output.caseAsset.has_value()
            || project_->assetRegistry().find(output.caseAsset->id) == nullptr)
        && (!output.resultAsset.has_value()
            || project_->assetRegistry().find(output.resultAsset->id) == nullptr);
}

FlowSmokeController::Status FlowSmokeController::commitPrepared(
    PreparedCommand prepared) const
{
    if (!prepared.ok()) {
        return prepared.status;
    }
    if (project_ == nullptr || stack_ == nullptr) {
        return Status::NullContext;
    }
    if (project_->state() != XQProject::LifecycleState::Open) {
        return Status::ProjectNotOpen;
    }
    if (prepared.projectGuard_.project != project_
        || prepared.projectGuard_.lifecycleEpoch != project_->lifecycleEpoch()
        || !sourceStillCurrent(prepared.sourceGuard_)
        || project_->scene().find(prepared.output_.caseNode) != nullptr
        || project_->scene().find(prepared.output_.resultNode) != nullptr
        || !targetAssetsStillAvailable(prepared.output_)) {
        return Status::SourceChanged;
    }

    std::unique_ptr<XQCommand> command(new ProjectNodeBundleCommand(
        project_, std::move(prepared.specs_), "Add L0 geometry smoke bundle"));
    return stack_->push(std::move(command))
        ? Status::Ok
        : Status::CommitRejected;
}

FlowSmokeController::Status FlowSmokeController::run(
    const SmokeIntent& intent) const
{
    return commitPrepared(prepare(intent));
}

} // namespace xq
