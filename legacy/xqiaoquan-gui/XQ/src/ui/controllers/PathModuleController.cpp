#include "ui/controllers/PathModuleController.h"

#include "core/XQDataNode.h"
#include "core/XQProject.h"
#include "core/XQVesselProfilePayload.h"
#include "core/asset/AssetRecord.h"

namespace xq {

PathModuleController::PathModuleController(XQProject* project)
    : project_(project)
    , registry_(PathModuleRegistry::builtIn())
{
}

std::vector<PathModuleDescriptor> PathModuleController::modules() const
{
    return registry_.descriptors();
}

PathModuleController::Result PathModuleController::run(
    const Intent& intent) const
{
    Result result;
    if (project_ == nullptr) {
        result.status = Status::NullContext;
        return result;
    }
    if (project_->state() != XQProject::LifecycleState::Open) {
        result.status = Status::ProjectNotOpen;
        return result;
    }
    if (!intent.sourceProfileNode.is_valid() || intent.moduleId.empty()) {
        result.status = Status::InvalidIntent;
        return result;
    }

    const XQDataNode* node = project_->scene().find(intent.sourceProfileNode);
    if (node == nullptr) {
        result.status = Status::SourceNotFound;
        return result;
    }
    if (node->domainType() != XQDomainType::VesselProfile) {
        result.status = Status::SourceTypeMismatch;
        return result;
    }
    if (project_->scene().is_stale(intent.sourceProfileNode)) {
        result.status = Status::SourceStale;
        return result;
    }
    const std::shared_ptr<XQVesselProfilePayload> payload =
        std::dynamic_pointer_cast<XQVesselProfilePayload>(node->payload());
    if (payload == nullptr) {
        result.status = Status::SourcePayloadInvalid;
        return result;
    }

    DerivationInputStamp input;
    input.nodeId = node->id();
    input.contentRevision = node->contentRevision();
    if (node->hasAssetId()) {
        const AssetRecord* asset = project_->assetRegistry().find(node->assetId());
        if (asset == nullptr
            || asset->kind != AssetKind::VesselProfile
            || asset->contentFingerprint.empty()) {
            result.status = Status::SourceAssetInvalid;
            return result;
        }
        input.assetId = node->assetId();
        input.assetFingerprint = asset->contentFingerprint;
    }

    const VesselPathSnapshotService::Result snapshot =
        VesselPathSnapshotService::build(input, payload->profile());
    result.snapshotStatus = snapshot.status;
    result.profileValidation = snapshot.profileValidation;
    if (!snapshot.ok()) {
        result.status = Status::SnapshotFailed;
        result.validation = snapshot.pathValidation;
        return result;
    }

    const ShellGeometrySmokeService::Result smoke =
        ShellGeometrySmokeService::run(snapshot.path.value());
    result.smokeStatus = smoke.status;
    if (!smoke.ok()) {
        result.status = Status::GeometrySmokeFailed;
        result.validation = smoke.validation;
        return result;
    }

    const PathModuleRegistry::RunResult module =
        registry_.run(intent.moduleId, snapshot.path.value());
    result.registryStatus = module.status;
    result.moduleStatus = module.execution.status;
    result.module = module.descriptor;
    result.validation = module.execution.validation;
    result.diagnostic = module.execution.diagnostic;
    result.sourceKind = snapshot.path->source.kind;
    result.geometry = smoke.summary;
    if (module.status == PathModuleRegistry::RunStatus::UnknownModule) {
        result.status = Status::UnknownModule;
        return result;
    }
    if (!module.ok()) {
        result.status = Status::ModuleFailed;
        return result;
    }

    result.status = Status::Ok;
    return result;
}

} // namespace xq
