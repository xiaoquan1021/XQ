#include <core/NodeId.h>
#include <core/XQContourGroupPayload.h>
#include <core/XQDataNode.h>
#include <core/XQPathPayload.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfilePayload.h>
#include <core/asset/AssetRecord.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>
#include <ui/controllers/VesselProfileController.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

const xq::NodeId kPathNode(1001);
const xq::NodeId kContourNode(1002);
const xq::NodeId kProfileNode(1003);
const xq::AssetId kPathAsset(2001);
const xq::AssetId kContourAsset(2002);
const xq::AssetId kGoldAsset(2003);
const xq::AssetId kProfileAsset(2004);

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

xq::XQPath make_path()
{
    xq::XQPath path;
    path.setId(kPathNode);
    path.setControlPoints({{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 30.0}}});
    path.resample(1.0);
    return path;
}

xq::XQContour make_contour(const xq::XQPath& path,
                           unsigned long long id,
                           double arcLength,
                           double halfWidth)
{
    xq::PathFrame pathFrame = {};
    path.frameAtArcLength(arcLength, &pathFrame);
    xq::XQContour contour;
    contour.contourId = xq::ContourId(id);
    contour.pathArcLength = arcLength;
    contour.frame.origin = pathFrame.position;
    contour.frame.normal = pathFrame.tangent;
    contour.frame.xAxis = pathFrame.normal;
    contour.frame.yAxis = pathFrame.binormal;
    contour.type = xq::ContourType::SplinePolygon;
    contour.closed = true;
    contour.points = {
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, -halfWidth, -halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, halfWidth, -halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, halfWidth, halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, -halfWidth, halfWidth),
    };
    return contour;
}

xq::XQContourGroup make_group(const xq::XQPath& path)
{
    xq::XQContourGroup group;
    group.setId(kContourNode);
    group.setSourcePathNode(kPathNode);
    group.addContour(make_contour(path, 3001, 5.0, 1.0));
    group.addContour(make_contour(path, 3002, 15.0, 2.0));
    group.addContour(make_contour(path, 3003, 25.0, 3.0));
    return group;
}

xq::AssetRecord make_asset(const xq::AssetId& id,
                           xq::AssetKind kind,
                           const std::string& fingerprint)
{
    xq::AssetRecord asset;
    asset.id = id;
    asset.category = xq::AssetCategory::Derived;
    asset.kind = kind;
    asset.contentFingerprint = fingerprint;
    asset.displayName = "profile-controller-test";
    return asset;
}

struct NodeSnapshot {
    xq::NodeId id;
    xq::XQDomainType domain = xq::XQDomainType::Unknown;
    std::string name;
    std::shared_ptr<xq::XQPayload> payload;
    xq::ContentRevision revision = 0;
    std::optional<xq::ScaleSlot> scaleSlot;
};

NodeSnapshot snapshot_node(const xq::XQDataNode& node)
{
    NodeSnapshot snapshot;
    snapshot.id = node.id();
    snapshot.domain = node.domainType();
    snapshot.name = node.display_name();
    snapshot.payload = node.payload();
    snapshot.revision = node.contentRevision();
    snapshot.scaleSlot = node.scaleSlot();
    return snapshot;
}

bool insert_snapshot(xq::XQProject* project, const NodeSnapshot& snapshot)
{
    if (project == nullptr || snapshot.payload == nullptr) {
        return false;
    }
    xq::XQDataNode node(
        snapshot.id, snapshot.domain, snapshot.name, snapshot.payload);
    node.setContentRevision(snapshot.revision);
    if (snapshot.scaleSlot.has_value()) {
        node.setScaleSlot(snapshot.scaleSlot.value());
    }
    return project->scene().insert(std::move(node))
        == xq::XQScene::InsertResult::Inserted;
}

struct Fixture {
    explicit Fixture(std::optional<xq::ScaleSlot> pathScale = xq::ScaleSlot::Organ,
                     std::optional<xq::ScaleSlot> contourScale = xq::ScaleSlot::Organ,
                     bool withAssets = false)
    {
        ready = project.open() == xq::XQProject::LifecycleResult::Ok;
        if (!ready) return;

        const xq::XQPath path = make_path();
        xq::XQDataNode pathNode(
            kPathNode,
            xq::XQDomainType::Path,
            "path",
            std::make_shared<xq::XQPathPayload>(path));
        pathNode.setContentRevision(7);
        if (pathScale.has_value()) pathNode.setScaleSlot(pathScale.value());

        xq::XQDataNode contourNode(
            kContourNode,
            xq::XQDomainType::ContourGroup,
            "contours",
            std::make_shared<xq::XQContourGroupPayload>(make_group(path)));
        contourNode.setContentRevision(11);
        if (contourScale.has_value()) contourNode.setScaleSlot(contourScale.value());

        if (withAssets) {
            ready = project.assetRegistry().registerAsset(make_asset(
                        kPathAsset, xq::AssetKind::Path, "sha256:path"))
                && project.assetRegistry().registerAsset(make_asset(
                    kContourAsset, xq::AssetKind::Contour, "sha256:contour"))
                && project.assetRegistry().registerAsset(make_asset(
                    kGoldAsset, xq::AssetKind::VesselProfile, "sha256:gold"));
            pathNode.setAssetId(kPathAsset);
            contourNode.setAssetId(kContourAsset);
        }

        ready = ready
            && project.scene().insert(pathNode) == xq::XQScene::InsertResult::Inserted
            && project.scene().insert(contourNode)
                == xq::XQScene::InsertResult::Inserted;
    }

    xq::VesselProfileController::ContourIntent contour_intent(
        xq::NodeId profileId = kProfileNode) const
    {
        xq::VesselProfileController::ContourIntent intent;
        intent.output.newProfileId = profileId;
        intent.output.name = "measured profile";
        intent.pathNode = kPathNode;
        intent.contourGroupNode = kContourNode;
        intent.frameOfReferenceId = "1.2.840.controller.frame";
        return intent;
    }

    xq::VesselProfileController::ImportedGoldIntent gold_intent(
        xq::NodeId profileId = kProfileNode) const
    {
        xq::VesselProfileController::ImportedGoldIntent intent;
        intent.output.newProfileId = profileId;
        intent.output.name = "imported gold profile";
        intent.request.lengthUnit =
            xq::VesselProfileImporter::LengthUnit::Millimeter;
        intent.request.areaUnit =
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter;
        intent.request.frameOfReferenceId = "1.2.840.controller.gold.frame";
        intent.request.sourcePath.nodeId = kPathNode;
        intent.request.externalEvidenceId = "gold-v1";
        intent.request.externalEvidenceFingerprint = "sha256:gold";
        for (int i = 0; i < 3; ++i) {
            xq::VesselProfileImporter::Sample sample;
            sample.sampleId = xq::VesselSampleId(
                static_cast<unsigned long long>(4001 + i));
            sample.arcLength = 5.0 + static_cast<double>(i) * 10.0;
            sample.position = {0.0, 0.0, sample.arcLength};
            sample.unitTangent = {0.0, 0.0, 1.0};
            sample.area = 10.0 + static_cast<double>(i);
            intent.request.samples.push_back(sample);
        }
        return intent;
    }

    xq::XQProject project;
    xq::XQCommandStack stack;
    bool ready = false;
};

int test_contour_prepare_commit_undo_redo_and_stale()
{
    Fixture fixture;
    CHECK(fixture.ready);
    xq::VesselProfileController controller(&fixture.project, &fixture.stack);
    const std::size_t nodesBefore = node_count(fixture.project.scene());
    const std::size_t relationsBefore = relation_count(fixture.project.scene());

    xq::VesselProfileController::PreparedCommand prepared =
        controller.prepareFromContours(fixture.contour_intent());
    CHECK(prepared.ok());
    CHECK(node_count(fixture.project.scene()) == nodesBefore);
    CHECK(relation_count(fixture.project.scene()) == relationsBefore);
    CHECK(fixture.stack.undo_count() == 0);
    CHECK(controller.commitPrepared(std::move(prepared))
          == xq::VesselProfileController::Status::Ok);

    const xq::XQDataNode* profileNode = fixture.project.scene().find(kProfileNode);
    CHECK(profileNode != nullptr);
    CHECK(profileNode->domainType() == xq::XQDomainType::VesselProfile);
    CHECK(profileNode->scaleSlot().has_value());
    CHECK(profileNode->scaleSlot().value() == xq::ScaleSlot::Organ);
    const std::shared_ptr<xq::XQVesselProfilePayload> payload =
        std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(profileNode->payload());
    CHECK(payload != nullptr);
    CHECK(xq::VesselProfileValidator::validate(payload->profile()).ok());
    CHECK(payload->profile().derivationStamp.inputs.size() == 2);
    CHECK(payload->profile().derivationStamp.inputs[0].contentRevision == 7);
    CHECK(payload->profile().derivationStamp.inputs[1].contentRevision == 11);
    CHECK(relation_count(fixture.project.scene()) == relationsBefore + 2);

    CHECK(fixture.stack.undo());
    CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    CHECK(relation_count(fixture.project.scene()) == relationsBefore);
    CHECK(fixture.stack.redo());
    CHECK(fixture.project.scene().find(kProfileNode) != nullptr);
    CHECK(relation_count(fixture.project.scene()) == relationsBefore + 2);

    const xq::NodeId downstream(1004);
    CHECK(fixture.project.scene().insert(xq::XQDataNode(
              downstream, "flow_result", "downstream"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(fixture.project.scene().link_derived(kProfileNode, downstream)
          == xq::XQScene::RelationResult::Linked);
    CHECK(fixture.project.scene().mark_source_changed(kContourNode) == 2);
    CHECK(fixture.project.scene().is_stale(kProfileNode));
    CHECK(fixture.project.scene().is_stale(downstream));
    CHECK(!fixture.project.scene().is_stale(kContourNode));
    return 0;
}

int test_failures_and_freshness_guards()
{
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        const xq::XQDataNode* contourNode = fixture.project.scene().find(kContourNode);
        std::shared_ptr<xq::XQContourGroupPayload> payload =
            std::dynamic_pointer_cast<xq::XQContourGroupPayload>(contourNode->payload());
        xq::XQContourGroup badGroup = payload->group();
        std::vector<xq::XQContour> contours = badGroup.contours();
        contours[0].closed = false;
        xq::XQContourGroup rebuilt;
        rebuilt.setId(kContourNode);
        rebuilt.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) rebuilt.addContour(contour);
        fixture.project.scene().find(kContourNode)->setPayload(
            xq::XQDomainType::ContourGroup,
            std::make_shared<xq::XQContourGroupPayload>(rebuilt));
        const std::size_t nodesBefore = node_count(fixture.project.scene());
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::AssemblyFailed);
        CHECK(!prepared.assemblyIssues.empty());
        CHECK(node_count(fixture.project.scene()) == nodesBefore);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        CHECK(fixture.project.scene().restore_stale_snapshot(
            {{kPathNode, xq::XQScene::StaleReason::SourceChanged}}));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(fixture.contour_intent());
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::SourceStale);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::CapturedContourInput captured =
            controller.captureFromContours(fixture.contour_intent());
        CHECK(captured.ok());
        xq::VesselProfileController::PreparedCommand prepared =
            xq::VesselProfileController::computeFromContours(std::move(captured));
        CHECK(prepared.ok());

        const xq::XQDataNode* pathNode = fixture.project.scene().find(kPathNode);
        const std::shared_ptr<xq::XQPathPayload> pathPayload =
            std::dynamic_pointer_cast<xq::XQPathPayload>(pathNode->payload());
        xq::XQPath edited = pathPayload->path();
        edited.setControlPoints({{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 40.0}}});
        edited.resample(1.0);
        xq::XQCommandStack editStack;
        CHECK(editStack.push(std::unique_ptr<xq::XQCommand>(
            new xq::SemanticReplacePayloadCommand(
                &fixture.project.scene(),
                kPathNode,
                xq::XQDomainType::Path,
                std::make_shared<xq::XQPathPayload>(edited)))));
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::CapturedContourInput captured =
            controller.captureFromContours(fixture.contour_intent());
        CHECK(captured.ok());
        xq::VesselProfileController::PreparedCommand prepared =
            xq::VesselProfileController::computeFromContours(std::move(captured));
        CHECK(prepared.ok());
        fixture.project.scene().find(kPathNode)->setScaleSlot(xq::ScaleSlot::Micro);
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(fixture.contour_intent());
        CHECK(prepared.ok());
        xq::XQDataNode* contourNode = fixture.project.scene().find(kContourNode);
        contourNode->setPayload(
            xq::XQDomainType::ContourGroup, contourNode->payload()->clone());
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    }
    {
        Fixture fixture(std::nullopt, xq::ScaleSlot::Organ);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        CHECK(controller.assembleFromContours(fixture.contour_intent())
              == xq::VesselProfileController::Status::Ok);
        const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
        CHECK(profile != nullptr);
        CHECK(!profile->hasScaleSlot());
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Micro);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        CHECK(controller.assembleFromContours(fixture.contour_intent())
              == xq::VesselProfileController::Status::Ok);
        const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
        CHECK(profile != nullptr);
        CHECK(!profile->hasScaleSlot());
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Micro);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.scaleSlot = xq::ScaleSlot::Cell;
        CHECK(controller.assembleFromContours(intent)
              == xq::VesselProfileController::Status::Ok);
        const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
        CHECK(profile != nullptr && profile->hasScaleSlot());
        CHECK(profile->scaleSlot().value() == xq::ScaleSlot::Cell);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        fixture.project.scene().find(kPathNode)->setScaleSlot(
            static_cast<xq::ScaleSlot>(999));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.scaleSlot = xq::ScaleSlot::Cell;
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status
              == xq::VesselProfileController::Status::SourcePayloadInvalid);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        fixture.project.scene().find(kContourNode)->setScaleSlot(
            static_cast<xq::ScaleSlot>(999));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.scaleSlot = xq::ScaleSlot::Organ;
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status
              == xq::VesselProfileController::Status::SourcePayloadInvalid);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    }
    return 0;
}

int test_imported_gold_asset_lineage_and_atomic_failure()
{
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        CHECK(controller.importGold(fixture.gold_intent())
              == xq::VesselProfileController::Status::Ok);
        const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
        CHECK(profile != nullptr && !profile->hasAssetId());
        CHECK(relation_count(fixture.project.scene()) == 1);
        CHECK(fixture.project.assetRegistry().assetCount() == 0);
        CHECK(fixture.stack.undo());
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.redo());
        CHECK(fixture.project.scene().find(kProfileNode) != nullptr);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.output.newProfileAsset = make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:profile");
        intent.evidenceAsset = kGoldAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(prepared.ok());
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.project.assetRegistry().find(kProfileAsset) == nullptr);
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::Ok);

        const xq::XQDataNode* profile = fixture.project.scene().find(kProfileNode);
        CHECK(profile != nullptr && profile->hasAssetId());
        CHECK(profile->assetId() == kProfileAsset);
        CHECK(profile->hasScaleSlot());
        CHECK(profile->scaleSlot().value() == xq::ScaleSlot::Organ);
        CHECK(relation_count(fixture.project.scene()) == 1);
        CHECK(fixture.project.assetRegistry().hasRelation(
            kPathAsset, kProfileAsset));
        CHECK(fixture.project.assetRegistry().hasRelation(
            kGoldAsset, kProfileAsset));
        CHECK(!fixture.project.assetRegistry().hasRelation(
            kContourAsset, kProfileAsset));
        CHECK(fixture.project.assetRegistry().relationCount() == 2);

        CHECK(fixture.stack.undo());
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.project.assetRegistry().find(kProfileAsset) == nullptr);
        CHECK(fixture.project.assetRegistry().find(kGoldAsset) != nullptr);
        CHECK(fixture.project.assetRegistry().relationCount() == 0);
        CHECK(fixture.stack.redo());
        CHECK(fixture.project.scene().find(kProfileNode) != nullptr);
        CHECK(fixture.project.assetRegistry().relationCount() == 2);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.evidenceAsset = kGoldAsset;
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::InvalidIntent);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.request.externalEvidenceId.clear();
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::ImportFailed);
        CHECK(prepared.importStatus
              == xq::VesselProfileImporter::Status::ProfileValidationFailed);
        CHECK(prepared.importValidation.hasIssue(
            xq::VesselProfileValidationCode::MissingExternalEvidenceId));
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        CHECK(fixture.project.assetRegistry().registerAsset(make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:existing-profile")));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.existingProfileAsset = kProfileAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(prepared.ok());
        fixture.project.assetRegistry().find(kProfileAsset)->contentFingerprint.clear();
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        CHECK(fixture.project.assetRegistry().registerAsset(make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:existing-profile")));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.existingProfileAsset = kProfileAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(prepared.ok());
        fixture.project.assetRegistry().find(kProfileAsset)->category =
            xq::AssetCategory::ExternalSource;
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.request.contractVersion = xq::VesselProfileV1::ContractVersion + 1;
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::ImportFailed);
        CHECK(prepared.importStatus
              == xq::VesselProfileImporter::Status::UnsupportedVersion);
        CHECK(prepared.importValidation.issues.empty());
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.request.areaUnit =
            xq::VesselProfileImporter::AreaUnit::SquareCentimeter;
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::ImportFailed);
        CHECK(prepared.importStatus
              == xq::VesselProfileImporter::Status::InvalidUnits);
        CHECK(prepared.importValidation.issues.empty());
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.output.newProfileAsset = make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            std::string());
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::VesselProfileController::Status::InvalidIntent);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.output.newProfileAsset = make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:profile");
        intent.evidenceAsset = kGoldAsset;
        intent.request.externalEvidenceFingerprint = "sha256:not-gold";
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status
              == xq::VesselProfileController::Status::EvidenceAssetInvalid);
        CHECK(fixture.project.assetRegistry().find(kProfileAsset) == nullptr);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.output.newProfileAsset = make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:profile");
        intent.evidenceAsset = kGoldAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(prepared.ok());

        CHECK(fixture.project.scene().insert(xq::XQDataNode(
                  kProfileNode,
                  xq::XQDomainType::VesselProfile,
                  "blocker",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::VesselProfile, "blocker")))
              == xq::XQScene::InsertResult::Inserted);
        const std::size_t assetsBefore = fixture.project.assetRegistry().assetCount();
        const std::size_t relationsBefore =
            fixture.project.assetRegistry().relationCount();
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::CommitRejected);
        CHECK(fixture.project.assetRegistry().find(kProfileAsset) == nullptr);
        CHECK(fixture.project.assetRegistry().assetCount() == assetsBefore);
        CHECK(fixture.project.assetRegistry().relationCount() == relationsBefore);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        CHECK(fixture.project.assetRegistry().registerAsset(make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:existing-profile")));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.existingProfileAsset = kProfileAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(prepared.ok());
        fixture.project.assetRegistry().find(kProfileAsset)->contentFingerprint =
            "sha256:changed-profile";
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        CHECK(fixture.project.assetRegistry().registerAsset(make_asset(
            kProfileAsset,
            xq::AssetKind::VesselProfile,
            "sha256:existing-profile")));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ContourIntent intent = fixture.contour_intent();
        intent.output.existingProfileAsset = kProfileAsset;
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        CHECK(prepared.ok());
        CHECK(fixture.project.assetRegistry().unregisterAsset(kProfileAsset));
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::ImportedGoldIntent intent =
            fixture.gold_intent();
        intent.request.sourcePath.nodeId = xq::NodeId(999999);
        const xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareImportedGold(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status
              == xq::VesselProfileController::Status::SourceNotFound);
    }
    {
        Fixture fixture(xq::ScaleSlot::Organ, xq::ScaleSlot::Organ, true);
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(fixture.contour_intent());
        CHECK(prepared.ok());
        fixture.project.assetRegistry().find(kPathAsset)->contentFingerprint =
            "sha256:path-changed";
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    return 0;
}

int test_project_identity_and_lifecycle_guards()
{
    {
        Fixture fixture;
        CHECK(fixture.ready);
        const NodeSnapshot path = snapshot_node(
            *fixture.project.scene().find(kPathNode));
        const NodeSnapshot contours = snapshot_node(
            *fixture.project.scene().find(kContourNode));
        xq::VesselProfileController sourceController(
            &fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            sourceController.prepareFromContours(fixture.contour_intent());
        CHECK(prepared.ok());

        xq::XQProject otherProject;
        xq::XQCommandStack otherStack;
        CHECK(otherProject.open() == xq::XQProject::LifecycleResult::Ok);
        CHECK(insert_snapshot(&otherProject, path));
        CHECK(insert_snapshot(&otherProject, contours));
        xq::VesselProfileController otherController(&otherProject, &otherStack);
        CHECK(otherController.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(otherProject.scene().find(kProfileNode) == nullptr);
        CHECK(otherStack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        const NodeSnapshot path = snapshot_node(
            *fixture.project.scene().find(kPathNode));
        const NodeSnapshot contours = snapshot_node(
            *fixture.project.scene().find(kContourNode));
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(fixture.contour_intent());
        CHECK(prepared.ok());
        const std::uint64_t capturedEpoch = fixture.project.lifecycleEpoch();
        CHECK(fixture.project.close() == xq::XQProject::LifecycleResult::Ok);
        CHECK(fixture.project.reopen() == xq::XQProject::LifecycleResult::Ok);
        CHECK(fixture.project.lifecycleEpoch() == capturedEpoch + 2);
        // Reuse the exact captured shared_ptr identities and revisions. Without
        // the lifecycle epoch guard every older source check would still pass.
        CHECK(insert_snapshot(&fixture.project, path));
        CHECK(insert_snapshot(&fixture.project, contours));
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.ready);
        xq::VesselProfileController controller(&fixture.project, &fixture.stack);
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(fixture.contour_intent());
        CHECK(prepared.ok());

        // A project replacement can preserve the exact shared payload handles,
        // revisions, stale state, and node ids. The lifecycle epoch must still
        // invalidate work captured before assignment on this same object.
        xq::XQProject replacement = fixture.project;
        const std::uint64_t capturedEpoch = fixture.project.lifecycleEpoch();
        fixture.project = std::move(replacement);
        CHECK(fixture.project.lifecycleEpoch() == capturedEpoch + 1);
        CHECK(controller.commitPrepared(std::move(prepared))
              == xq::VesselProfileController::Status::SourceChanged);
        CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
        CHECK(fixture.stack.undo_count() == 0);
    }
    return 0;
}

} // namespace

int main()
{
    int result = test_contour_prepare_commit_undo_redo_and_stale();
    if (result != 0) return result;
    result = test_failures_and_freshness_guards();
    if (result != 0) return result;
    result = test_imported_gold_asset_lineage_and_atomic_failure();
    if (result != 0) return result;
    result = test_project_identity_and_lifecycle_guards();
    if (result != 0) return result;
    std::printf("OK: vessel profile controller preparation and atomic commit\n");
    return 0;
}
