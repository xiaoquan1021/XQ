#include "core/command/XQProjectCommands.h"

#include "core/XQVesselProfilePayload.h"

#include <map>
#include <set>

namespace xq {
namespace {

bool same_derivation_input(const DerivationInputStamp& left,
                           const DerivationInputStamp& right)
{
    return left.nodeId == right.nodeId
        && left.contentRevision == right.contentRevision
        && left.assetId == right.assetId
        && left.assetFingerprint == right.assetFingerprint;
}

bool validate_vessel_profile_sources(const XQProject& project,
                                     const ProjectNodeBatchSpec& spec)
{
    if (spec.node.domainType() != XQDomainType::VesselProfile) {
        return true;
    }
    const std::shared_ptr<XQVesselProfilePayload> payload =
        std::dynamic_pointer_cast<XQVesselProfilePayload>(spec.node.payload());
    if (payload == nullptr || !VesselProfileValidator::validate(payload->profile()).ok()
        || payload->profile().derivationStamp.inputs.size() != spec.sources.size()) {
        return false;
    }

    std::map<NodeId, const DerivationInputStamp*> preparedSources;
    for (std::vector<DerivationInputStamp>::const_iterator it = spec.sources.begin();
         it != spec.sources.end(); ++it) {
        preparedSources[it->nodeId] = &(*it);
    }
    for (std::vector<DerivationInputStamp>::const_iterator it =
             payload->profile().derivationStamp.inputs.begin();
         it != payload->profile().derivationStamp.inputs.end(); ++it) {
        const std::map<NodeId, const DerivationInputStamp*>::const_iterator prepared =
            preparedSources.find(it->nodeId);
        if (prepared == preparedSources.end()
            || !same_derivation_input(*prepared->second, *it)) {
            return false;
        }
    }

    const std::map<NodeId, const DerivationInputStamp*>::const_iterator path =
        preparedSources.find(payload->profile().sourcePathNode);
    const XQDataNode* pathNode = project.scene().find(payload->profile().sourcePathNode);
    if (path == preparedSources.end() || pathNode == nullptr
        || pathNode->domainType() != XQDomainType::Path) {
        return false;
    }

    std::set<NodeId> declaredSceneSources;
    declaredSceneSources.insert(payload->profile().sourcePathNode);
    for (std::vector<NodeId>::const_iterator evidence =
             payload->profile().sourceEvidenceNodes.begin();
         evidence != payload->profile().sourceEvidenceNodes.end(); ++evidence) {
        const XQDataNode* evidenceNode = project.scene().find(*evidence);
        if (preparedSources.find(*evidence) == preparedSources.end()
            || evidenceNode == nullptr
            || (evidenceNode->domainType() != XQDomainType::ContourGroup
                && evidenceNode->domainType() != XQDomainType::SegmentationMask)) {
            return false;
        }
        declaredSceneSources.insert(*evidence);
    }
    if (declaredSceneSources.size() != preparedSources.size()) {
        return false;
    }
    for (std::map<NodeId, const DerivationInputStamp*>::const_iterator source =
             preparedSources.begin();
         source != preparedSources.end(); ++source) {
        if (declaredSceneSources.find(source->first) == declaredSceneSources.end()) {
            return false;
        }
    }

    bool hasImportedGold = false;
    for (std::vector<VesselProfileSample>::const_iterator sample =
             payload->profile().samples.begin();
         sample != payload->profile().samples.end(); ++sample) {
        if (sample->evidenceKind == VesselEvidenceKind::ImportedGold) {
            hasImportedGold = true;
            break;
        }
    }
    if (!hasImportedGold) {
        if (!spec.additionalAssetSources.empty()) {
            return false;
        }
    } else if (!spec.additionalAssetSources.empty()) {
        if (spec.additionalAssetSources.size() != 1) {
            return false;
        }
        const AssetRecord* record = project.assetRegistry().find(
            spec.additionalAssetSources.front());
        if (record == nullptr
            || record->contentFingerprint
                != payload->profile().externalEvidenceFingerprint) {
            return false;
        }
    }
    return true;
}

} // namespace

ProjectNodeBatchCommand::ProjectNodeBatchCommand(
    XQProject* project,
    ProjectNodeBatchSpec spec,
    std::string label)
    : project_(project)
    , spec_(std::move(spec))
    , asset_registered_(false)
    , node_inserted_(false)
    , added_asset_relations_()
    , label_(std::move(label))
{
    if (spec_.node.payload()
        && spec_.node.domainType() == spec_.node.payload()->domainType()) {
        spec_.node.setPayload(spec_.node.domainType(), spec_.node.payload()->clone());
    }
}

bool ProjectNodeBatchCommand::validate_prepared_state() const
{
    if (project_ == nullptr || project_->state() != XQProject::LifecycleState::Open
        || !spec_.node.id().is_valid() || spec_.node.hasAssetId()
        || spec_.node.domainType() == XQDomainType::Unknown
        || spec_.node.payload() == nullptr
        || spec_.node.payload()->domainType() != spec_.node.domainType()
        || spec_.node.domain_type() != domainTypeToString(spec_.node.domainType())
        || (spec_.node.hasScaleSlot()
            && scaleSlotToToken(spec_.node.scaleSlot().value())[0] == '\0')
        || project_->scene().find(spec_.node.id()) != nullptr) {
        return false;
    }

    AssetKind expectedKind = AssetKind::Surface;
    if (!assetKindForDomain(spec_.node.domainType(), &expectedKind)) {
        return false;
    }

    const AssetRegistry& registry = project_->assetRegistry();
    if (spec_.assetToRegister.has_value()) {
        if (!spec_.bindAsset.has_value()
            || !spec_.assetToRegister->id.is_valid()
            || spec_.assetToRegister->id != spec_.bindAsset.value()
            || registry.find(spec_.assetToRegister->id) != nullptr
            || spec_.assetToRegister->kind != expectedKind) {
            return false;
        }
    }
    if (spec_.bindAsset.has_value()) {
        const AssetRecord* target = spec_.assetToRegister.has_value()
            ? &spec_.assetToRegister.value()
            : registry.find(spec_.bindAsset.value());
        if (!spec_.bindAsset->is_valid() || target == nullptr
            || target->kind != expectedKind) {
            return false;
        }
    } else if (spec_.assetToRegister.has_value()
               || !spec_.additionalAssetSources.empty()) {
        return false;
    }

    std::set<NodeId> sourceIds;
    for (std::vector<DerivationInputStamp>::const_iterator it = spec_.sources.begin();
         it != spec_.sources.end(); ++it) {
        if (!it->nodeId.is_valid() || it->nodeId == spec_.node.id()
            || !sourceIds.insert(it->nodeId).second) {
            return false;
        }
        const XQDataNode* source = project_->scene().find(it->nodeId);
        if (source == nullptr || source->contentRevision() != it->contentRevision
            || project_->scene().is_stale(it->nodeId)) {
            return false;
        }
        if (source->hasAssetId()) {
            const AssetRecord* boundSourceAsset = registry.find(source->assetId());
            if (boundSourceAsset == nullptr
                || !assetKindMatchesDomain(boundSourceAsset->kind, source->domainType())
                || !it->assetId.has_value()
                || source->assetId() != it->assetId.value()
                || boundSourceAsset->contentFingerprint != it->assetFingerprint) {
                return false;
            }
        } else if (it->assetId.has_value() || !it->assetFingerprint.empty()) {
            return false;
        }
    }

    if (!validate_vessel_profile_sources(*project_, spec_)) {
        return false;
    }

    std::set<AssetId> additionalSources;
    for (std::vector<AssetId>::const_iterator it = spec_.additionalAssetSources.begin();
         it != spec_.additionalAssetSources.end(); ++it) {
        if (!it->is_valid() || registry.find(*it) == nullptr
            || (spec_.bindAsset.has_value() && *it == spec_.bindAsset.value())
            || !additionalSources.insert(*it).second) {
            return false;
        }
    }
    return true;
}

bool ProjectNodeBatchCommand::execute()
{
    if (asset_registered_ || node_inserted_ || !added_asset_relations_.empty()
        || !validate_prepared_state()) {
        return false;
    }

    AssetRegistry& registry = project_->assetRegistry();
    XQScene& scene = project_->scene();
    if (spec_.assetToRegister.has_value()) {
        if (!registry.registerAsset(spec_.assetToRegister.value())) {
            return false;
        }
        asset_registered_ = true;
    }

    if (scene.insert(spec_.node) != XQScene::InsertResult::Inserted) {
        rollback_applied_steps();
        return false;
    }
    node_inserted_ = true;

    XQDataNode* liveNode = scene.find(spec_.node.id());
    if (liveNode == nullptr) {
        rollback_applied_steps();
        return false;
    }
    if (spec_.bindAsset.has_value()) {
        liveNode->setAssetId(spec_.bindAsset.value());
    }

    for (std::vector<DerivationInputStamp>::const_iterator it = spec_.sources.begin();
         it != spec_.sources.end(); ++it) {
        if (scene.link_derived(it->nodeId, spec_.node.id())
            != XQScene::RelationResult::Linked) {
            rollback_applied_steps();
            return false;
        }
    }

    if (spec_.bindAsset.has_value()) {
        const AssetId target = spec_.bindAsset.value();
        std::set<AssetId> parentAssets;
        for (std::vector<DerivationInputStamp>::const_iterator it = spec_.sources.begin();
             it != spec_.sources.end(); ++it) {
            const XQDataNode* source = scene.find(it->nodeId);
            if (source != nullptr && source->hasAssetId() && source->assetId() != target) {
                parentAssets.insert(source->assetId());
            }
        }
        parentAssets.insert(
            spec_.additionalAssetSources.begin(), spec_.additionalAssetSources.end());

        for (std::set<AssetId>::const_iterator parent = parentAssets.begin();
             parent != parentAssets.end(); ++parent) {
            if (registry.hasRelation(*parent, target)) {
                continue;
            }
            if (!registry.addRelation(*parent, target)) {
                rollback_applied_steps();
                return false;
            }
            AssetRelation relation;
            relation.source = *parent;
            relation.derived = target;
            added_asset_relations_.push_back(relation);
        }
    }

    return true;
}

void ProjectNodeBatchCommand::rollback_applied_steps()
{
    if (project_ == nullptr) {
        return;
    }
    AssetRegistry& registry = project_->assetRegistry();
    for (std::vector<AssetRelation>::reverse_iterator it = added_asset_relations_.rbegin();
         it != added_asset_relations_.rend(); ++it) {
        registry.removeRelation(it->source, it->derived);
    }
    added_asset_relations_.clear();

    if (node_inserted_) {
        project_->scene().remove(spec_.node.id());
        node_inserted_ = false;
    }
    if (asset_registered_ && spec_.assetToRegister.has_value()) {
        registry.unregisterAsset(spec_.assetToRegister->id);
        asset_registered_ = false;
    }
}

void ProjectNodeBatchCommand::undo()
{
    rollback_applied_steps();
}

std::string ProjectNodeBatchCommand::label() const
{
    return label_;
}

ProjectNodeBundleCommand::ProjectNodeBundleCommand(
    XQProject* project,
    std::vector<ProjectNodeBatchSpec> specs,
    std::string label)
    : project_(project)
    , commands_()
    , appliedCount_(0)
    , label_(std::move(label))
{
    commands_.reserve(specs.size());
    for (ProjectNodeBatchSpec& spec : specs) {
        commands_.push_back(std::unique_ptr<ProjectNodeBatchCommand>(
            new ProjectNodeBatchCommand(project_, std::move(spec), label_)));
    }
}

bool ProjectNodeBundleCommand::execute()
{
    if (project_ == nullptr || commands_.empty() || appliedCount_ != 0) {
        return false;
    }
    for (std::size_t i = 0; i < commands_.size(); ++i) {
        if (!commands_[i]->execute()) {
            while (appliedCount_ > 0) {
                --appliedCount_;
                commands_[appliedCount_]->undo();
            }
            return false;
        }
        ++appliedCount_;
    }
    return true;
}

void ProjectNodeBundleCommand::undo()
{
    while (appliedCount_ > 0) {
        --appliedCount_;
        commands_[appliedCount_]->undo();
    }
}

std::string ProjectNodeBundleCommand::label() const
{
    return label_;
}

} // namespace xq
