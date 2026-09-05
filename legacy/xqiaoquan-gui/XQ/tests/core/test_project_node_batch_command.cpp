#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfile.h>
#include <core/XQVesselProfilePayload.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQProjectCommands.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

const xq::NodeId kPathNode(101);
const xq::NodeId kContourNode(102);
const xq::NodeId kProfileNode(201);
const xq::AssetId kPathAsset(1001);
const xq::AssetId kContourAsset(1002);
const xq::AssetId kGoldAsset(1003);
const xq::AssetId kProfileAsset(2001);

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::size_t node_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relation_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

xq::AssetRecord make_asset(const xq::AssetId& id,
                           xq::AssetCategory category,
                           xq::AssetKind kind,
                           const std::string& name,
                           const std::string& fingerprint)
{
    xq::AssetRecord record;
    record.id = id;
    record.category = category;
    record.kind = kind;
    record.displayName = name;
    record.contentFingerprint = fingerprint;
    return record;
}

xq::DerivationInputStamp stamp_for(const xq::XQProject& project,
                                   const xq::NodeId& id)
{
    xq::DerivationInputStamp stamp;
    const xq::XQDataNode* node = project.scene().find(id);
    if (node == nullptr) {
        return stamp;
    }
    stamp.nodeId = id;
    stamp.contentRevision = node->contentRevision();
    if (node->hasAssetId()) {
        stamp.assetId = node->assetId();
        const xq::AssetRecord* asset = project.assetRegistry().find(node->assetId());
        if (asset != nullptr) {
            stamp.assetFingerprint = asset->contentFingerprint;
        }
    }
    return stamp;
}

xq::VesselProfileV1 make_profile(const xq::XQProject& project)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "1.2.840.shell.frame";
    profile.sourcePathNode = kPathNode;
    profile.sourceEvidenceNodes.push_back(kContourNode);
    profile.externalEvidenceId = "gold-asset-1003";
    profile.externalEvidenceFingerprint = "sha256:gold";
    profile.derivationStamp.algorithmId = "xq.profile.test";
    profile.derivationStamp.algorithmVersion = "1.0.0";
    profile.derivationStamp.parameterSummary = "spacing-mm=5";
    profile.derivationStamp.inputs.push_back(stamp_for(project, kPathNode));
    profile.derivationStamp.inputs.push_back(stamp_for(project, kContourNode));

    for (unsigned long long i = 0; i < 3; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(5001 + i);
        sample.arcLengthMm = static_cast<double>(i) * 5.0;
        sample.positionMm = {static_cast<double>(i) * 5.0, 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 100.0 - static_cast<double>(i) * 5.0;
        if (i == 1) {
            sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
            sample.quality = xq::VesselSampleQuality::ReviewRequired;
            sample.sourceEvidenceNode = xq::NodeId::invalid();
        } else {
            sample.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
            sample.quality = xq::VesselSampleQuality::Accepted;
            sample.sourceEvidenceNode = kContourNode;
        }
        profile.samples.push_back(sample);
    }
    return profile;
}

struct Fixture {
    xq::XQProject project;
    bool ready = false;

    Fixture()
    {
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return;
        }
        if (!project.assetRegistry().registerAsset(make_asset(
                kPathAsset, xq::AssetCategory::Derived, xq::AssetKind::Path,
                "Path asset", "sha256:path"))
            || !project.assetRegistry().registerAsset(make_asset(
                kContourAsset, xq::AssetCategory::Derived, xq::AssetKind::Contour,
                "Contour asset", "sha256:contour"))
            || !project.assetRegistry().registerAsset(make_asset(
                kGoldAsset, xq::AssetCategory::ExternalSource,
                xq::AssetKind::VesselProfile, "Imported gold", "sha256:gold"))) {
            return;
        }

        xq::XQDataNode path(
            kPathNode, xq::XQDomainType::Path, "Navigation path",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, "Paths/aorta.pth"));
        path.setContentRevision(5);
        path.setAssetId(kPathAsset);
        xq::XQDataNode contour(
            kContourNode, xq::XQDomainType::ContourGroup, "Contour evidence",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::ContourGroup, "Segmentations/aorta.ctgr"));
        contour.setContentRevision(8);
        contour.setAssetId(kContourAsset);
        ready = project.scene().insert(path) == xq::XQScene::InsertResult::Inserted
            && project.scene().insert(contour) == xq::XQScene::InsertResult::Inserted;
    }

    xq::ProjectNodeBatchSpec profile_spec() const
    {
        const xq::VesselProfileV1 profile = make_profile(project);
        xq::XQDataNode node(
            kProfileNode, xq::XQDomainType::VesselProfile, "VesselProfileV1",
            std::make_shared<xq::XQVesselProfilePayload>(profile));
        node.setScaleSlot(xq::ScaleSlot::Organ);
        xq::ProjectNodeBatchSpec spec(node);
        spec.assetToRegister = make_asset(
            kProfileAsset, xq::AssetCategory::Derived, xq::AssetKind::VesselProfile,
            "Profile asset", "sha256:profile");
        spec.bindAsset = kProfileAsset;
        spec.sources = profile.derivationStamp.inputs;
        spec.additionalAssetSources.push_back(kGoldAsset);
        return spec;
    }
};

int test_success_undo_redo_and_monotonic_id()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare success fixture", __LINE__);
    }
    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(
                &fixture.project, fixture.profile_spec(), "Add profile")))) {
        return fail("atomic project batch succeeds", __LINE__);
    }

    const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
    if (profile == nullptr || !profile->hasAssetId() || profile->assetId() != kProfileAsset
        || !profile->hasScaleSlot()
        || profile->scaleSlot().value() != xq::ScaleSlot::Organ
        || std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(profile->payload()) == nullptr
        || node_count(fixture.project.scene()) != 3
        || relation_count(fixture.project.scene()) != 2) {
        return fail("batch commits node payload binding and two Scene parents", __LINE__);
    }
    const xq::AssetRegistry& registry = fixture.project.assetRegistry();
    const xq::AssetRecord* profileAsset = registry.find(kProfileAsset);
    if (profileAsset == nullptr || profileAsset->displayName != "Profile asset"
        || profileAsset->contentFingerprint != "sha256:profile"
        || registry.assetCount() != 4 || registry.relationCount() != 3
        || !registry.hasRelation(kPathAsset, kProfileAsset)
        || !registry.hasRelation(kContourAsset, kProfileAsset)
        || !registry.hasRelation(kGoldAsset, kProfileAsset)) {
        return fail("batch commits complete asset and all lineage", __LINE__);
    }

    if (!stack.undo()) {
        return fail("project batch undo succeeds", __LINE__);
    }
    if (fixture.project.scene().find(kProfileNode) != nullptr
        || fixture.project.assetRegistry().find(kProfileAsset) != nullptr
        || node_count(fixture.project.scene()) != 2
        || relation_count(fixture.project.scene()) != 0
        || fixture.project.assetRegistry().assetCount() != 3
        || fixture.project.assetRegistry().relationCount() != 0) {
        return fail("project batch undo removes only committed child state", __LINE__);
    }

    const xq::AssetId issuedAfterUndo = fixture.project.assetRegistry().createAsset(
        xq::AssetCategory::Derived, xq::AssetKind::AiAnalysis);
    if (!issuedAfterUndo.is_valid() || issuedAfterUndo.value() <= kProfileAsset.value()) {
        return fail("undo does not rewind automatic asset ids", __LINE__);
    }
    if (!stack.redo()) {
        return fail("project batch redo succeeds", __LINE__);
    }
    profile = fixture.project.scene().find(kProfileNode);
    if (profile == nullptr || !profile->hasAssetId() || profile->assetId() != kProfileAsset
        || fixture.project.assetRegistry().find(issuedAfterUndo) == nullptr
        || fixture.project.assetRegistry().relationCount() != 3) {
        return fail("redo restores the same explicit asset id without harming newer ids", __LINE__);
    }
    return 0;
}

int test_preexisting_lineage_is_not_owned_by_command()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare preexisting-lineage fixture", __LINE__);
    }
    xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
    const xq::AssetRecord existingTarget = spec.assetToRegister.value();
    spec.assetToRegister.reset();
    if (!fixture.project.assetRegistry().registerAsset(existingTarget)
        || !fixture.project.assetRegistry().addRelation(kPathAsset, kProfileAsset)) {
        return fail("prepare preexisting target and lineage", __LINE__);
    }
    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, spec)))) {
        return fail("batch can bind an existing target asset", __LINE__);
    }
    if (fixture.project.assetRegistry().relationCount() != 3) {
        return fail("batch adds only missing lineage around existing target", __LINE__);
    }
    if (!stack.undo()) {
        return fail("existing-target batch undo succeeds", __LINE__);
    }
    if (fixture.project.assetRegistry().find(kProfileAsset) == nullptr
        || fixture.project.assetRegistry().relationCount() != 1
        || !fixture.project.assetRegistry().hasRelation(kPathAsset, kProfileAsset)
        || fixture.project.assetRegistry().hasRelation(kContourAsset, kProfileAsset)
        || fixture.project.assetRegistry().hasRelation(kGoldAsset, kProfileAsset)) {
        return fail("undo preserves lineage and asset owned before the command", __LINE__);
    }
    return 0;
}

int test_scene_only_batch_without_asset()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare scene-only fixture", __LINE__);
    }
    xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
    spec.assetToRegister.reset();
    spec.bindAsset.reset();
    spec.additionalAssetSources.clear();
    const std::size_t assetsBefore = fixture.project.assetRegistry().assetCount();
    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, spec)))) {
        return fail("scene-only project batch succeeds", __LINE__);
    }
    const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
    if (profile == nullptr || profile->hasAssetId()
        || relation_count(fixture.project.scene()) != 2
        || fixture.project.assetRegistry().assetCount() != assetsBefore
        || fixture.project.assetRegistry().relationCount() != 0) {
        return fail("scene-only batch leaves AssetRegistry untouched", __LINE__);
    }
    if (!stack.undo() || fixture.project.scene().find(kProfileNode) != nullptr
        || fixture.project.assetRegistry().assetCount() != assetsBefore
        || !stack.redo()) {
        return fail("scene-only batch undo redo stays asset-free", __LINE__);
    }
    profile = fixture.project.scene().find(kProfileNode);
    if (profile == nullptr || profile->hasAssetId()
        || fixture.project.assetRegistry().assetCount() != assetsBefore) {
        return fail("scene-only redo restores only scene state", __LINE__);
    }
    return 0;
}

int test_imported_gold_without_optional_asset_parent()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare ImportedGold metadata-only fixture", __LINE__);
    }
    xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
    spec.additionalAssetSources.clear();

    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, std::move(spec))))) {
        return fail("ImportedGold accepts versioned external evidence without optional asset", __LINE__);
    }
    const xq::AssetRegistry& registry = fixture.project.assetRegistry();
    if (fixture.project.scene().find(kProfileNode) == nullptr
        || registry.relationCount() != 2
        || !registry.hasRelation(kPathAsset, kProfileAsset)
        || !registry.hasRelation(kContourAsset, kProfileAsset)
        || registry.hasRelation(kGoldAsset, kProfileAsset)) {
        return fail("only present ImportedGold assets become lineage parents", __LINE__);
    }
    if (!stack.undo() || fixture.project.scene().find(kProfileNode) != nullptr
        || registry.find(kProfileAsset) != nullptr || registry.relationCount() != 0) {
        return fail("metadata-only ImportedGold batch remains fully reversible", __LINE__);
    }
    return 0;
}

int expect_failed_without_side_effects(Fixture* fixture,
                                       xq::ProjectNodeBatchSpec spec,
                                       const char* message,
                                       int line)
{
    const std::size_t nodesBefore = node_count(fixture->project.scene());
    const std::size_t sceneRelationsBefore = relation_count(fixture->project.scene());
    const std::size_t assetsBefore = fixture->project.assetRegistry().assetCount();
    const std::size_t assetRelationsBefore = fixture->project.assetRegistry().relationCount();
    xq::XQCommandStack stack;
    if (stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture->project, std::move(spec))))) {
        return fail(message, line);
    }
    if (stack.undo_count() != 0 || stack.redo_count() != 0
        || node_count(fixture->project.scene()) != nodesBefore
        || relation_count(fixture->project.scene()) != sceneRelationsBefore
        || fixture->project.assetRegistry().assetCount() != assetsBefore
        || fixture->project.assetRegistry().relationCount() != assetRelationsBefore
        || fixture->project.scene().find(kProfileNode) != nullptr) {
        return fail("failed batch has zero project and undo-stack side effects", line);
    }
    return 0;
}

int test_failure_matrix()
{
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 invalid = profile->profile();
        invalid.samples[0].areaMm2 = 0.0;
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(invalid));
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "invalid VesselProfile is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.node.setScaleSlot(static_cast<xq::ScaleSlot>(999));
        const int result = expect_failed_without_side_effects(
            &fixture,
            std::move(spec),
            "invalid target ScaleSlot is rejected",
            __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.sources[0].contentRevision += 1;
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "revision mismatch is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.sources[1].nodeId = xq::NodeId(9999);
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "missing source node is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        fixture.project.scene().restore_stale_snapshot(
            {{kPathNode, xq::XQScene::StaleReason::SourceChanged}});
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "stale source is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        fixture.project.assetRegistry().find(kPathAsset)->kind = xq::AssetKind::Image;
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "source domain and AssetKind mismatch is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        fixture.project.assetRegistry().find(kPathAsset)->contentFingerprint = "sha256:changed";
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "fingerprint mismatch is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.sources[0].assetId.reset();
        spec.sources[0].assetFingerprint.clear();
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 incomplete = profile->profile();
        incomplete.derivationStamp.inputs[0] = spec.sources[0];
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(incomplete));
        const int result = expect_failed_without_side_effects(
            &fixture,
            std::move(spec),
            "bound source asset identity is mandatory in derivation stamp",
            __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.sources[0].assetFingerprint.clear();
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 incomplete = profile->profile();
        incomplete.derivationStamp.inputs[0] = spec.sources[0];
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(incomplete));
        const int result = expect_failed_without_side_effects(
            &fixture,
            std::move(spec),
            "available source fingerprint is mandatory in derivation stamp",
            __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.node.setAssetId(kProfileAsset);
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "prepared node must be unbound", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.bindAsset = xq::AssetId(2999);
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "new record and binding id mismatch is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.additionalAssetSources.push_back(kGoldAsset);
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "duplicate additional asset parent is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        fixture.project.assetRegistry().find(kGoldAsset)->contentFingerprint =
            "sha256:different-gold";
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "ImportedGold asset fingerprint mismatch is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 measured = profile->profile();
        measured.externalEvidenceId.clear();
        measured.externalEvidenceFingerprint.clear();
        for (xq::VesselProfileSample& sample : measured.samples) {
            sample.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
        }
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(measured));
        const int result = expect_failed_without_side_effects(
            &fixture,
            std::move(spec),
            "non-imported profile rejects undeclared asset-only lineage",
            __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.sources.push_back(spec.sources.front());
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "duplicate source is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.additionalAssetSources[0] = xq::AssetId(9999);
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "missing asset-only parent is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        spec.assetToRegister->kind = xq::AssetKind::Image;
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "wrong target AssetKind is rejected", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        xq::XQDataNode duplicate(
            kProfileNode, xq::XQDomainType::VesselProfile, "Existing profile",
            spec.node.payload());
        fixture.project.scene().insert(duplicate);
        const std::size_t nodesBefore = node_count(fixture.project.scene());
        xq::XQCommandStack stack;
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::ProjectNodeBatchCommand(&fixture.project, std::move(spec))))
            || node_count(fixture.project.scene()) != nodesBefore
            || fixture.project.assetRegistry().find(kProfileAsset) != nullptr
            || stack.undo_count() != 0) {
            return fail("duplicate target node fails before asset registration", __LINE__);
        }
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        if (!fixture.project.assetRegistry().registerAsset(spec.assetToRegister.value())) {
            return fail("prepare duplicate target asset", __LINE__);
        }
        const std::size_t assetsBefore = fixture.project.assetRegistry().assetCount();
        xq::XQCommandStack stack;
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::ProjectNodeBatchCommand(&fixture.project, std::move(spec))))
            || fixture.project.assetRegistry().assetCount() != assetsBefore
            || fixture.project.scene().find(kProfileNode) != nullptr
            || stack.undo_count() != 0) {
            return fail("duplicate target asset has zero side effects", __LINE__);
        }
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 split = profile->profile();
        split.derivationStamp.inputs[0].contentRevision += 1;
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(split));
        const int result = expect_failed_without_side_effects(
            &fixture, std::move(spec), "payload and command source stamps cannot diverge", __LINE__);
        if (result != 0) return result;
    }
    {
        Fixture fixture;
        xq::ProjectNodeBatchSpec spec = fixture.profile_spec();
        const xq::NodeId undeclaredSource(1999);
        fixture.project.scene().insert(xq::XQDataNode(
            undeclaredSource,
            xq::XQDomainType::Image,
            "Undeclared image source",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Image, "Images/extra.vti")));
        const xq::DerivationInputStamp extra = stamp_for(fixture.project, undeclaredSource);
        spec.sources.push_back(extra);
        std::shared_ptr<xq::XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(spec.node.payload());
        xq::VesselProfileV1 split = profile->profile();
        split.derivationStamp.inputs.push_back(extra);
        spec.node.setPayload(
            xq::XQDomainType::VesselProfile,
            std::make_shared<xq::XQVesselProfilePayload>(split));
        const int result = expect_failed_without_side_effects(
            &fixture,
            std::move(spec),
            "profile Scene sources must equal declared path and evidence",
            __LINE__);
        if (result != 0) return result;
    }
    return 0;
}

int test_failed_redo_is_atomic_and_retryable()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare failed-redo fixture", __LINE__);
    }
    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, fixture.profile_spec())))
        || !stack.undo()) {
        return fail("prepare batch command on redo stack", __LINE__);
    }
    const xq::AssetRecord occupant = make_asset(
        kProfileAsset, xq::AssetCategory::Derived, xq::AssetKind::VesselProfile,
        "External occupant", "sha256:occupant");
    if (!fixture.project.assetRegistry().registerAsset(occupant)) {
        return fail("occupy target asset id before redo", __LINE__);
    }
    if (stack.redo()) {
        return fail("blocked project batch redo reports failure", __LINE__);
    }
    const xq::AssetRecord* preserved = fixture.project.assetRegistry().find(kProfileAsset);
    if (fixture.project.scene().find(kProfileNode) != nullptr
        || preserved == nullptr || preserved->displayName != "External occupant"
        || fixture.project.assetRegistry().relationCount() != 0
        || relation_count(fixture.project.scene()) != 0
        || stack.redo_count() != 1 || stack.undo_count() != 0) {
        return fail("failed redo is atomic and remains retryable", __LINE__);
    }
    if (!fixture.project.assetRegistry().unregisterAsset(kProfileAsset)
        || !stack.redo()) {
        return fail("project batch redo succeeds after blocker is removed", __LINE__);
    }
    const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
    if (profile == nullptr || !profile->hasAssetId()
        || profile->assetId() != kProfileAsset
        || fixture.project.assetRegistry().find(kProfileAsset) == nullptr
        || fixture.project.assetRegistry().relationCount() != 3
        || relation_count(fixture.project.scene()) != 2
        || stack.redo_count() != 0 || stack.undo_count() != 1) {
        return fail("retry commits the complete batch exactly once", __LINE__);
    }
    return 0;
}

int test_failed_push_preserves_existing_redo()
{
    Fixture fixture;
    if (!fixture.ready) {
        return fail("prepare failed-push redo fixture", __LINE__);
    }
    xq::XQCommandStack stack;
    if (!stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, fixture.profile_spec())))
        || !stack.undo() || stack.redo_count() != 1) {
        return fail("prepare reusable project batch redo entry", __LINE__);
    }

    xq::ProjectNodeBatchSpec invalid = fixture.profile_spec();
    invalid.sources[0].contentRevision += 1;
    if (stack.push(std::unique_ptr<xq::XQCommand>(
            new xq::ProjectNodeBatchCommand(&fixture.project, std::move(invalid))))
        || stack.redo_count() != 1 || stack.undo_count() != 0
        || fixture.project.scene().find(kProfileNode) != nullptr
        || fixture.project.assetRegistry().find(kProfileAsset) != nullptr) {
        return fail("failed project batch push leaves existing redo untouched", __LINE__);
    }
    if (!stack.redo() || fixture.project.scene().find(kProfileNode) == nullptr) {
        return fail("preserved project batch redo remains executable", __LINE__);
    }
    return 0;
}

} // namespace

int main()
{
    int result = test_success_undo_redo_and_monotonic_id();
    if (result != 0) return result;
    result = test_preexisting_lineage_is_not_owned_by_command();
    if (result != 0) return result;
    result = test_scene_only_batch_without_asset();
    if (result != 0) return result;
    result = test_imported_gold_without_optional_asset_parent();
    if (result != 0) return result;
    result = test_failure_matrix();
    if (result != 0) return result;
    result = test_failed_redo_is_atomic_and_retryable();
    if (result != 0) return result;
    result = test_failed_push_preserves_existing_redo();
    if (result != 0) return result;
    std::printf("OK: project node batch command\n");
    return 0;
}
