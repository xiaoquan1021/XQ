#include "ui/controllers/VesselProfileController.h"

#include "core/XQContourGroupPayload.h"
#include "core/XQDataNode.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"

#include <set>
#include <utility>

namespace xq {
namespace {

bool valid_scale_slot(ScaleSlot slot)
{
    return scaleSlotToToken(slot)[0] != '\0';
}

bool profile_has_imported_gold(const VesselProfileV1& profile)
{
    for (const VesselProfileSample& sample : profile.samples) {
        if (sample.evidenceKind == VesselEvidenceKind::ImportedGold) {
            return true;
        }
    }
    return false;
}

} // namespace

VesselProfileController::VesselProfileController(
    XQProject* project,
    XQCommandStack* stack)
    : project_(project)
    , stack_(stack)
{
}

VesselProfileController::Status VesselProfileController::validateOutput(
    const OutputIntent& output,
    std::optional<AssetGuard>* existingTargetGuard) const
{
    if (project_ == nullptr || stack_ == nullptr || existingTargetGuard == nullptr) {
        return Status::NullContext;
    }
    existingTargetGuard->reset();
    if (project_->state() != XQProject::LifecycleState::Open) {
        return Status::ProjectNotOpen;
    }
    if (!output.newProfileId.is_valid() || output.name.empty()
        || (output.scaleSlot.has_value()
            && !valid_scale_slot(output.scaleSlot.value()))
        || (output.newProfileAsset.has_value()
            && output.existingProfileAsset.has_value())) {
        return Status::InvalidIntent;
    }
    if (project_->scene().find(output.newProfileId) != nullptr) {
        return Status::TargetExists;
    }

    const AssetRegistry& registry = project_->assetRegistry();
    if (output.newProfileAsset.has_value()) {
        const AssetRecord& asset = output.newProfileAsset.value();
        if (!asset.id.is_valid() || asset.category != AssetCategory::Derived
            || asset.kind != AssetKind::VesselProfile
            || asset.contentFingerprint.empty()) {
            return Status::InvalidIntent;
        }
        if (registry.find(asset.id) != nullptr) {
            return Status::TargetExists;
        }
    }
    if (output.existingProfileAsset.has_value()) {
        const AssetId assetId = output.existingProfileAsset.value();
        const AssetRecord* asset = registry.find(assetId);
        if (!assetId.is_valid() || asset == nullptr
            || asset->id != assetId
            || asset->category != AssetCategory::Derived
            || asset->kind != AssetKind::VesselProfile
            || asset->contentFingerprint.empty()) {
            return Status::InvalidIntent;
        }
        AssetGuard guard;
        guard.id = assetId;
        guard.category = asset->category;
        guard.kind = asset->kind;
        guard.fingerprint = asset->contentFingerprint;
        *existingTargetGuard = std::move(guard);
    }
    return Status::Ok;
}

VesselProfileController::Status VesselProfileController::captureSource(
    const NodeId& id,
    XQDomainType expectedDomain,
    SourceGuard* guard,
    std::shared_ptr<XQPayload>* snapshot) const
{
    if (guard == nullptr || snapshot == nullptr || project_ == nullptr) {
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
    const std::shared_ptr<XQPayload>& payload = node->payload();
    if (payload == nullptr || payload->domainType() != expectedDomain) {
        return Status::SourcePayloadInvalid;
    }
    const std::optional<ScaleSlot> scaleSlot = node->scaleSlot();
    if (scaleSlot.has_value() && !valid_scale_slot(scaleSlot.value())) {
        return Status::SourcePayloadInvalid;
    }
    std::shared_ptr<XQPayload> cloned = payload->clone();
    if (cloned == nullptr || cloned->domainType() != expectedDomain) {
        return Status::SourcePayloadInvalid;
    }

    DerivationInputStamp stamp;
    stamp.nodeId = id;
    stamp.contentRevision = node->contentRevision();
    if (node->hasAssetId()) {
        if (!node->assetId().is_valid()) {
            return Status::SourceAssetInvalid;
        }
        const AssetRecord* asset = project_->assetRegistry().find(node->assetId());
        if (asset == nullptr || !assetKindMatchesDomain(asset->kind, expectedDomain)
            || asset->contentFingerprint.empty()) {
            return Status::SourceAssetInvalid;
        }
        stamp.assetId = node->assetId();
        stamp.assetFingerprint = asset->contentFingerprint;
    }

    guard->stamp = stamp;
    guard->domain = expectedDomain;
    guard->payloadIdentity = payload;
    guard->scaleSlot = scaleSlot;
    *snapshot = std::move(cloned);
    return Status::Ok;
}

bool VesselProfileController::sourceStillCurrent(const SourceGuard& guard) const
{
    if (project_ == nullptr || project_->state() != XQProject::LifecycleState::Open) {
        return false;
    }
    const XQDataNode* node = project_->scene().find(guard.stamp.nodeId);
    if (node == nullptr || node->domainType() != guard.domain
        || node->contentRevision() != guard.stamp.contentRevision
        || project_->scene().is_stale(guard.stamp.nodeId)
        || node->payload() != guard.payloadIdentity
        || node->scaleSlot() != guard.scaleSlot) {
        return false;
    }

    if (guard.stamp.assetId.has_value()) {
        if (!node->hasAssetId() || !node->assetId().is_valid()
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

bool VesselProfileController::targetAssetStillCurrent(const AssetGuard& guard) const
{
    if (project_ == nullptr || project_->state() != XQProject::LifecycleState::Open
        || !guard.id.is_valid() || guard.fingerprint.empty()) {
        return false;
    }
    const AssetRecord* asset = project_->assetRegistry().find(guard.id);
    return asset != nullptr
        && asset->id == guard.id
        && asset->category == guard.category
        && asset->kind == guard.kind
        && !asset->contentFingerprint.empty()
        && asset->contentFingerprint == guard.fingerprint;
}

std::optional<ScaleSlot> VesselProfileController::resolveScaleSlot(
    const std::optional<ScaleSlot>& explicitScale,
    const std::vector<SourceGuard>& sourceGuards)
{
    if (explicitScale.has_value()) {
        return explicitScale;
    }
    if (sourceGuards.empty()) {
        return std::nullopt;
    }

    std::optional<ScaleSlot> inferred;
    for (const SourceGuard& guard : sourceGuards) {
        if (!guard.scaleSlot.has_value()) {
            return std::nullopt;
        }
        if (!inferred.has_value()) {
            inferred = guard.scaleSlot.value();
        } else if (inferred.value() != guard.scaleSlot.value()) {
            return std::nullopt;
        }
    }
    return inferred;
}

VesselProfileController::PreparedCommand VesselProfileController::buildPrepared(
    const OutputIntent& output,
    VesselProfileV1 profile,
    ProjectGuard projectGuard,
    std::optional<AssetGuard> targetAssetGuard,
    std::vector<SourceGuard> guards,
    const std::vector<AssetId>& additionalAssetSources)
{
    PreparedCommand prepared;
    std::shared_ptr<XQVesselProfilePayload> payload =
        std::make_shared<XQVesselProfilePayload>(std::move(profile));
    XQDataNode node(
        output.newProfileId,
        XQDomainType::VesselProfile,
        output.name,
        payload);

    const std::optional<ScaleSlot> scale =
        resolveScaleSlot(output.scaleSlot, guards);
    if (scale.has_value()) {
        node.setScaleSlot(scale.value());
    }

    ProjectNodeBatchSpec batch(std::move(node));
    for (const SourceGuard& guard : guards) {
        batch.sources.push_back(guard.stamp);
    }
    if (output.newProfileAsset.has_value()) {
        batch.assetToRegister = output.newProfileAsset;
        batch.bindAsset = output.newProfileAsset->id;
    } else if (output.existingProfileAsset.has_value()) {
        batch.bindAsset = output.existingProfileAsset;
    }
    batch.additionalAssetSources = additionalAssetSources;

    prepared.batch_.reset(new ProjectNodeBatchSpec(std::move(batch)));
    prepared.projectGuard_ = projectGuard;
    prepared.targetAssetGuard_ = std::move(targetAssetGuard);
    prepared.sourceGuards_ = std::move(guards);
    prepared.status = Status::Ok;
    return prepared;
}

VesselProfileController::CapturedContourInput
VesselProfileController::captureFromContours(const ContourIntent& intent) const
{
    CapturedContourInput captured;
    captured.status = validateOutput(intent.output, &captured.targetAssetGuard_);
    if (captured.status != Status::Ok) {
        return captured;
    }
    captured.projectGuard_.project = project_;
    captured.projectGuard_.lifecycleEpoch = project_->lifecycleEpoch();
    if (!intent.pathNode.is_valid() || !intent.contourGroupNode.is_valid()
        || intent.pathNode == intent.contourGroupNode
        || intent.frameOfReferenceId.empty()) {
        captured.status = Status::InvalidIntent;
        return captured;
    }

    SourceGuard pathGuard;
    SourceGuard contourGuard;
    std::shared_ptr<XQPayload> pathSnapshot;
    std::shared_ptr<XQPayload> contourSnapshot;
    captured.status = captureSource(
        intent.pathNode, XQDomainType::Path, &pathGuard, &pathSnapshot);
    if (captured.status != Status::Ok) {
        return captured;
    }
    captured.status = captureSource(
        intent.contourGroupNode,
        XQDomainType::ContourGroup,
        &contourGuard,
        &contourSnapshot);
    if (captured.status != Status::Ok) {
        return captured;
    }

    const std::shared_ptr<XQPathPayload> pathPayload =
        std::dynamic_pointer_cast<XQPathPayload>(pathSnapshot);
    const std::shared_ptr<XQContourGroupPayload> contourPayload =
        std::dynamic_pointer_cast<XQContourGroupPayload>(contourSnapshot);
    if (pathPayload == nullptr || contourPayload == nullptr) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }

    captured.output_ = intent.output;
    captured.input_.pathSource = pathGuard.stamp;
    captured.input_.path = pathPayload->path();
    captured.input_.contourSource = contourGuard.stamp;
    captured.input_.contourGroup = contourPayload->group();
    captured.input_.frameOfReferenceId = intent.frameOfReferenceId;
    captured.options_ = intent.options;
    captured.sourceGuards_.push_back(std::move(pathGuard));
    captured.sourceGuards_.push_back(std::move(contourGuard));
    captured.status = Status::Ok;
    return captured;
}

VesselProfileController::PreparedCommand
VesselProfileController::computeFromContours(CapturedContourInput captured)
{
    PreparedCommand prepared;
    if (!captured.ok()) {
        prepared.status = captured.status;
        return prepared;
    }
    VesselProfileAssembler::Result assembled =
        VesselProfileAssembler::assemble(captured.input_, captured.options_);
    if (!assembled.ok()) {
        prepared.status = Status::AssemblyFailed;
        prepared.assemblyIssues = std::move(assembled.issues);
        return prepared;
    }
    return buildPrepared(
        captured.output_,
        std::move(assembled.profile.value()),
        captured.projectGuard_,
        std::move(captured.targetAssetGuard_),
        std::move(captured.sourceGuards_),
        {});
}

VesselProfileController::CapturedImportedGoldInput
VesselProfileController::captureImportedGold(const ImportedGoldIntent& intent) const
{
    CapturedImportedGoldInput captured;
    captured.status = validateOutput(intent.output, &captured.targetAssetGuard_);
    if (captured.status != Status::Ok) {
        return captured;
    }
    captured.projectGuard_.project = project_;
    captured.projectGuard_.lifecycleEpoch = project_->lifecycleEpoch();
    if (!intent.request.sourcePath.nodeId.is_valid()) {
        captured.status = Status::InvalidIntent;
        return captured;
    }
    const bool hasTargetAsset = intent.output.newProfileAsset.has_value()
        || intent.output.existingProfileAsset.has_value();
    if (intent.evidenceAsset.has_value() && !hasTargetAsset) {
        captured.status = Status::InvalidIntent;
        return captured;
    }

    SourceGuard pathGuard;
    std::shared_ptr<XQPayload> pathSnapshot;
    captured.status = captureSource(
        intent.request.sourcePath.nodeId,
        XQDomainType::Path,
        &pathGuard,
        &pathSnapshot);
    if (captured.status != Status::Ok) {
        return captured;
    }
    if (std::dynamic_pointer_cast<XQPathPayload>(pathSnapshot) == nullptr) {
        captured.status = Status::SourcePayloadInvalid;
        return captured;
    }

    captured.output_ = intent.output;
    captured.request_ = intent.request;
    captured.request_.sourcePath = pathGuard.stamp;
    captured.sourceGuards_.push_back(std::move(pathGuard));
    if (intent.evidenceAsset.has_value()) {
        const AssetId evidence = intent.evidenceAsset.value();
        const AssetRecord* record = project_->assetRegistry().find(evidence);
        if (!evidence.is_valid() || record == nullptr
            || record->contentFingerprint
                != captured.request_.externalEvidenceFingerprint) {
            captured.status = Status::EvidenceAssetInvalid;
            return captured;
        }
        captured.additionalAssetSources_.push_back(evidence);
    }
    captured.status = Status::Ok;
    return captured;
}

VesselProfileController::PreparedCommand
VesselProfileController::computeImportedGold(CapturedImportedGoldInput captured)
{
    PreparedCommand prepared;
    if (!captured.ok()) {
        prepared.status = captured.status;
        return prepared;
    }
    VesselProfileImporter::Result imported =
        VesselProfileImporter::importProfile(captured.request_);
    if (!imported.ok()) {
        prepared.status = Status::ImportFailed;
        prepared.importStatus = imported.status;
        prepared.importValidation = std::move(imported.validation);
        return prepared;
    }
    if (!profile_has_imported_gold(imported.profile.value())) {
        prepared.status = Status::ImportFailed;
        return prepared;
    }
    prepared = buildPrepared(
        captured.output_,
        std::move(imported.profile.value()),
        captured.projectGuard_,
        std::move(captured.targetAssetGuard_),
        std::move(captured.sourceGuards_),
        captured.additionalAssetSources_);
    prepared.importStatus = VesselProfileImporter::Status::Ok;
    return prepared;
}

VesselProfileController::PreparedCommand
VesselProfileController::prepareFromContours(const ContourIntent& intent) const
{
    return computeFromContours(captureFromContours(intent));
}

VesselProfileController::PreparedCommand
VesselProfileController::prepareImportedGold(const ImportedGoldIntent& intent) const
{
    return computeImportedGold(captureImportedGold(intent));
}

VesselProfileController::Status VesselProfileController::commitPrepared(
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
        || prepared.projectGuard_.lifecycleEpoch != project_->lifecycleEpoch()) {
        return Status::SourceChanged;
    }
    if (prepared.targetAssetGuard_.has_value()
        && !targetAssetStillCurrent(prepared.targetAssetGuard_.value())) {
        return Status::SourceChanged;
    }
    for (const SourceGuard& guard : prepared.sourceGuards_) {
        if (!sourceStillCurrent(guard)) {
            return Status::SourceChanged;
        }
    }
    std::unique_ptr<XQCommand> command(new ProjectNodeBatchCommand(
        project_, std::move(*prepared.batch_), "Add vessel profile"));
    return stack_->push(std::move(command))
        ? Status::Ok
        : Status::CommitRejected;
}

VesselProfileController::Status VesselProfileController::assembleFromContours(
    const ContourIntent& intent) const
{
    return commitPrepared(prepareFromContours(intent));
}

VesselProfileController::Status VesselProfileController::importGold(
    const ImportedGoldIntent& intent) const
{
    return commitPrepared(prepareImportedGold(intent));
}

} // namespace xq
