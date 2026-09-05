#include "ui/controllers/CenterlineBController.h"

#include "core/XQDataNode.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"
#include "core/path/ICenterlineSkeletonizer3D.h"
#include "core/source/ResidentVoxelSource.h"

#include <cmath>
#include <limits>
#include <utility>

namespace xq {
namespace {

bool sameGeometry(const ImageGeometry& left, const ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || left.spacing[axis] != right.spacing[axis]
            || left.origin[axis] != right.origin[axis]) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (left.direction[axis][column]
                != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

bool validGraphProfile(const CenterlineBGraphProfileV1& profile)
{
    return profile.contractVersion == CenterlineBGraphProfileV1::ContractVersion
        && std::isfinite(profile.shortSpurLengthMm)
        && profile.shortSpurLengthMm >= 0.0;
}

AssetRecord derivedAsset(
    AssetId id,
    AssetKind kind,
    const std::string& name,
    const std::string& fingerprint)
{
    AssetRecord asset;
    asset.id = id;
    asset.category = AssetCategory::Derived;
    asset.kind = kind;
    asset.displayName = name;
    asset.contentFingerprint = fingerprint;
    return asset;
}

} // namespace

const char* centerlineBControllerStatusToken(
    CenterlineBController::Status status)
{
    switch (status) {
    case CenterlineBController::Status::Ok: return "ok";
    case CenterlineBController::Status::NullContext: return "null_context";
    case CenterlineBController::Status::ProjectNotOpen:
        return "project_not_open";
    case CenterlineBController::Status::InvalidIntent: return "invalid_intent";
    case CenterlineBController::Status::TargetExists: return "target_exists";
    case CenterlineBController::Status::SourceNotFound:
        return "source_not_found";
    case CenterlineBController::Status::SourceTypeMismatch:
        return "source_type_mismatch";
    case CenterlineBController::Status::SourceStale: return "source_stale";
    case CenterlineBController::Status::SourcePayloadInvalid:
        return "source_payload_invalid";
    case CenterlineBController::Status::SourceAssetInvalid:
        return "source_asset_invalid";
    case CenterlineBController::Status::UnsupportedScale:
        return "unsupported_scale";
    case CenterlineBController::Status::AssetIdExhausted:
        return "asset_id_exhausted";
    case CenterlineBController::Status::ComputeFailed:
        return "compute_failed";
    case CenterlineBController::Status::SourceChanged:
        return "source_changed";
    case CenterlineBController::Status::CommitRejected:
        return "commit_rejected";
    }
    return "unknown";
}

CenterlineBController::CenterlineBController(
    XQProject* project,
    XQCommandStack* stack,
    const ICenterlineSkeletonizer3D* skeletonizer)
    : project_(project)
    , stack_(stack)
    , skeletonizer_(skeletonizer)
{
}

CenterlineBController::Status CenterlineBController::captureSourceGuard(
    const NodeId& id,
    XQDomainType expectedDomain,
    SourceGuard* guard) const
{
    if (project_ == nullptr || guard == nullptr) {
        return Status::NullContext;
    }
    const XQDataNode* node = project_->scene().find(id);
    if (node == nullptr) {
        return Status::SourceNotFound;
    }
    if (node->domainType() != expectedDomain) {
        return Status::SourceTypeMismatch;
    }
    if (project_->scene().is_stale(id)) {
        return Status::SourceStale;
    }
    if (node->payload() == nullptr
        || node->payload()->domainType() != expectedDomain) {
        return Status::SourcePayloadInvalid;
    }

    DerivationInputStamp stamp;
    stamp.nodeId = id;
    stamp.contentRevision = node->contentRevision();
    if (node->hasAssetId()) {
        const AssetRecord* asset = project_->assetRegistry().find(node->assetId());
        if (!node->assetId().is_valid() || asset == nullptr
            || !assetKindMatchesDomain(asset->kind, expectedDomain)
            || asset->contentFingerprint.empty()) {
            return Status::SourceAssetInvalid;
        }
        stamp.assetId = node->assetId();
        stamp.assetFingerprint = asset->contentFingerprint;
    }

    guard->stamp = std::move(stamp);
    guard->domain = expectedDomain;
    guard->payloadIdentity = node->payload();
    guard->scaleSlot = node->scaleSlot();
    return Status::Ok;
}

CenterlineBController::CapturedInput CenterlineBController::capture(
    const Intent& intent) const
{
    CapturedInput captured;
    if (project_ == nullptr || stack_ == nullptr || skeletonizer_ == nullptr) {
        captured.status = Status::NullContext;
        return captured;
    }
    if (project_->state() != XQProject::LifecycleState::Open) {
        captured.status = Status::ProjectNotOpen;
        return captured;
    }
    if (!intent.sourceMaskNode.is_valid()
        || !intent.output.pathNode.is_valid()
        || !intent.output.profileNode.is_valid()
        || intent.output.pathNode == intent.output.profileNode
        || intent.output.pathName.empty() || intent.output.profileName.empty()
        || intent.sourceMaskNode == intent.output.pathNode
        || intent.sourceMaskNode == intent.output.profileNode
        || !validGraphProfile(intent.graphProfile)) {
        captured.status = Status::InvalidIntent;
        return captured;
    }
    if (project_->scene().find(intent.output.pathNode) != nullptr
        || project_->scene().find(intent.output.profileNode) != nullptr) {
        captured.status = Status::TargetExists;
        return captured;
    }

    captured.status = captureSourceGuard(
        intent.sourceMaskNode,
        XQDomainType::SegmentationMask,
        &captured.maskGuard_);
    if (captured.status != Status::Ok) {
        return captured;
    }
    const std::shared_ptr<XQSegmentationMaskPayload> maskPayload =
        std::dynamic_pointer_cast<XQSegmentationMaskPayload>(
            captured.maskGuard_.payloadIdentity);
    if (maskPayload == nullptr || !maskPayload->mask().is_valid()
        || !maskPayload->mask().hasGeometry()
        || !maskPayload->mask().hasSourceImageNode()) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }

    const NodeId sourceImageNode = maskPayload->mask().sourceImageNode();
    if (!sourceImageNode.is_valid() || sourceImageNode == intent.sourceMaskNode
        || sourceImageNode == intent.output.pathNode
        || sourceImageNode == intent.output.profileNode) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }
    captured.status = captureSourceGuard(
        sourceImageNode, XQDomainType::Image, &captured.imageGuard_);
    if (captured.status != Status::Ok) {
        return captured;
    }
    const std::shared_ptr<XQImageVolumePayload> imagePayload =
        std::dynamic_pointer_cast<XQImageVolumePayload>(
            captured.imageGuard_.payloadIdentity);
    if (imagePayload == nullptr || !imagePayload->volume().hasGeometry()
        || !imagePayload->volume().hasDicomIdentity()
        || imagePayload->volume().dicomIdentity().frameOfReferenceUid.empty()
        || !sameGeometry(maskPayload->mask().geometry(),
                         imagePayload->volume().geometry())) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }
    if (!captured.imageGuard_.scaleSlot.has_value()
        || captured.imageGuard_.scaleSlot.value() != ScaleSlot::Organ
        || (captured.maskGuard_.scaleSlot.has_value()
            && captured.maskGuard_.scaleSlot.value()
                != captured.imageGuard_.scaleSlot.value())) {
        captured.status = Status::UnsupportedScale;
        return captured;
    }

    const AssetId pathAsset = project_->assetRegistry().nextAvailableAssetId();
    if (!pathAsset.is_valid()
        || pathAsset.value()
            == (std::numeric_limits<AssetId::ValueType>::max)()) {
        captured.status = Status::AssetIdExhausted;
        return captured;
    }
    const AssetId profileAsset(pathAsset.value() + 1);
    if (!profileAsset.is_valid()
        || project_->assetRegistry().find(pathAsset) != nullptr
        || project_->assetRegistry().find(profileAsset) != nullptr) {
        captured.status = Status::AssetIdExhausted;
        return captured;
    }

    std::vector<std::uint8_t> maskBytes = maskPayload->mask().voxels();
    captured.maskBuffer_ = std::make_shared<XQMemoryImageBufferHandle>(
        ScalarType::UInt8,
        maskPayload->mask().dimensions(),
        1,
        std::move(maskBytes));
    if (!captured.maskBuffer_->is_valid()) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }

    captured.geometry_ = maskPayload->mask().geometry();
    captured.request_.sourceImageNode = sourceImageNode;
    captured.request_.frameOfReferenceId =
        imagePayload->volume().dicomIdentity().frameOfReferenceUid;
    captured.request_.sourceMask = captured.maskGuard_.stamp;
    captured.request_.outputPathNode = intent.output.pathNode;
    captured.request_.outputPathAsset = pathAsset;
    captured.request_.outputProfileNode = intent.output.profileNode;
    captured.request_.outputProfileAsset = profileAsset;
    captured.request_.graphProfile = intent.graphProfile;
    captured.output_ = intent.output;
    captured.projectGuard_.project = project_;
    captured.projectGuard_.lifecycleEpoch = project_->lifecycleEpoch();
    captured.status = Status::Ok;
    return captured;
}

CenterlineBController::PreparedCommand CenterlineBController::compute(
    CapturedInput captured) const
{
    PreparedCommand prepared;
    if (!captured.ok()) {
        prepared.status = captured.status;
        return prepared;
    }
    if (skeletonizer_ == nullptr || captured.maskBuffer_ == nullptr) {
        prepared.status = Status::NullContext;
        return prepared;
    }

    ResidentVoxelSource source(captured.maskBuffer_);
    CenterlineBService::Result computed = CenterlineBService::run(
        *skeletonizer_, captured.geometry_, source, captured.request_);
    prepared.serviceStatus = computed.status;
    prepared.skeletonizationStatus = computed.skeletonizationStatus;
    prepared.skeletonizationStage = computed.skeletonizationStage;
    prepared.graphStatus = computed.graphStatus;
    if (!computed.ok()) {
        prepared.status = Status::ComputeFailed;
        return prepared;
    }

    CenterlineBService::Output output = std::move(computed.output.value());
    XQDataNode pathNode(
        captured.output_.pathNode,
        XQDomainType::Path,
        captured.output_.pathName,
        std::make_shared<XQPathPayload>(std::move(output.path)));
    pathNode.setScaleSlot(ScaleSlot::Organ);
    ProjectNodeBatchSpec pathSpec(std::move(pathNode));
    pathSpec.sources.push_back(captured.maskGuard_.stamp);
    pathSpec.assetToRegister = derivedAsset(
        captured.request_.outputPathAsset,
        AssetKind::Path,
        captured.output_.pathName,
        output.pathContentFingerprint);
    pathSpec.bindAsset = captured.request_.outputPathAsset;

    XQDataNode profileNode(
        captured.output_.profileNode,
        XQDomainType::VesselProfile,
        captured.output_.profileName,
        std::make_shared<XQVesselProfilePayload>(std::move(output.profile)));
    profileNode.setScaleSlot(ScaleSlot::Organ);
    ProjectNodeBatchSpec profileSpec(std::move(profileNode));
    const std::shared_ptr<XQVesselProfilePayload> preparedProfile =
        std::dynamic_pointer_cast<XQVesselProfilePayload>(
            profileSpec.node.payload());
    if (preparedProfile == nullptr) {
        prepared.status = Status::ComputeFailed;
        return prepared;
    }
    profileSpec.sources = preparedProfile->profile().derivationStamp.inputs;
    profileSpec.assetToRegister = derivedAsset(
        captured.request_.outputProfileAsset,
        AssetKind::VesselProfile,
        captured.output_.profileName,
        output.profileContentFingerprint);
    profileSpec.bindAsset = captured.request_.outputProfileAsset;

    prepared.specs_.push_back(std::move(pathSpec));
    prepared.specs_.push_back(std::move(profileSpec));
    prepared.output_ = std::move(captured.output_);
    prepared.pathAsset_ = captured.request_.outputPathAsset;
    prepared.profileAsset_ = captured.request_.outputProfileAsset;
    prepared.projectGuard_ = captured.projectGuard_;
    prepared.imageGuard_ = std::move(captured.imageGuard_);
    prepared.maskGuard_ = std::move(captured.maskGuard_);
    prepared.status = Status::Ok;
    return prepared;
}

CenterlineBController::PreparedCommand CenterlineBController::prepare(
    const Intent& intent) const
{
    return compute(capture(intent));
}

bool CenterlineBController::sourceStillCurrent(
    const SourceGuard& guard) const
{
    if (project_ == nullptr
        || project_->state() != XQProject::LifecycleState::Open) {
        return false;
    }
    const XQDataNode* node = project_->scene().find(guard.stamp.nodeId);
    if (node == nullptr || node->domainType() != guard.domain
        || node->contentRevision() != guard.stamp.contentRevision
        || node->payload() != guard.payloadIdentity
        || node->scaleSlot() != guard.scaleSlot
        || project_->scene().is_stale(guard.stamp.nodeId)) {
        return false;
    }
    if (guard.stamp.assetId.has_value()) {
        if (!node->hasAssetId()
            || node->assetId() != guard.stamp.assetId.value()) {
            return false;
        }
        const AssetRecord* asset = project_->assetRegistry().find(node->assetId());
        return asset != nullptr
            && assetKindMatchesDomain(asset->kind, guard.domain)
            && asset->contentFingerprint == guard.stamp.assetFingerprint;
    }
    return !node->hasAssetId();
}

bool CenterlineBController::targetsStillAvailable(
    const PreparedCommand& prepared) const
{
    return project_ != nullptr
        && project_->scene().find(prepared.output_.pathNode) == nullptr
        && project_->scene().find(prepared.output_.profileNode) == nullptr
        && project_->assetRegistry().find(prepared.pathAsset_) == nullptr
        && project_->assetRegistry().find(prepared.profileAsset_) == nullptr;
}

CenterlineBController::Status CenterlineBController::commitPrepared(
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
        || !sourceStillCurrent(prepared.imageGuard_)
        || !sourceStillCurrent(prepared.maskGuard_)
        || !targetsStillAvailable(prepared)) {
        return Status::SourceChanged;
    }

    std::unique_ptr<XQCommand> command(new ProjectNodeBundleCommand(
        project_, std::move(prepared.specs_), "Build Centerline B"));
    return stack_->push(std::move(command))
        ? Status::Ok
        : Status::CommitRejected;
}

CenterlineBController::Status CenterlineBController::run(
    const Intent& intent) const
{
    return commitPrepared(prepare(intent));
}

} // namespace xq
