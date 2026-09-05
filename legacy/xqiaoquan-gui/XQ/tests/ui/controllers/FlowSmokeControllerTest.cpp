#include <core/XQDataNode.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQSimulationCasePayload.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfilePayload.h>
#include <core/asset/AssetRecord.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQProjectCommands.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>
#include <ui/controllers/FlowSmokeController.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

const xq::NodeId kPathNode(10);
const xq::NodeId kProfileNode(30);
const xq::NodeId kCaseNode(40);
const xq::NodeId kResultNode(41);
const xq::AssetId kProfileAsset(300);
const xq::AssetId kCaseAsset(400);
const xq::AssetId kResultAsset(401);

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(condition)                  \
    do {                                  \
        if (!(condition)) {               \
            return fail(#condition, __LINE__); \
        }                                 \
    } while (false)

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relationCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

xq::AssetRecord asset(xq::AssetId id,
                      xq::AssetKind kind,
                      const std::string& fingerprint)
{
    xq::AssetRecord record;
    record.id = id;
    record.category = xq::AssetCategory::Derived;
    record.kind = kind;
    record.contentFingerprint = fingerprint;
    return record;
}

xq::VesselProfileV1 profile()
{
    xq::VesselProfileV1 value;
    value.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    value.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    value.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    value.frameOfReferenceId = "1.2.840.shell.flow";
    value.sourcePathNode = kPathNode;
    value.externalEvidenceId = "gold-flow-profile";
    value.externalEvidenceFingerprint = "sha256:gold-flow-profile";
    value.derivationStamp.algorithmId = "xq.flow-smoke.controller-test";
    value.derivationStamp.algorithmVersion = "1";
    value.derivationStamp.parameterSummary = "length-mm=50";
    value.derivationStamp.inputs.push_back({kPathNode, 0, std::nullopt, ""});
    const double arcs[] = {0.0, 9.0, 27.5, 50.0};
    for (std::size_t i = 0; i < 4; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(100 + i);
        sample.arcLengthMm = arcs[i];
        sample.positionMm = {arcs[i], 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 100.0 + 10.0 * i;
        sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
        sample.quality = xq::VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = xq::NodeId::invalid();
        value.samples.push_back(sample);
    }
    return value;
}

struct Fixture {
    xq::XQProject project;
    xq::XQCommandStack stack;
    bool ready = false;

    bool seed()
    {
        xq::XQDataNode path(
            kPathNode,
            xq::XQDomainType::Path,
            "Path",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, std::string()));
        path.setScaleSlot(xq::ScaleSlot::Organ);
        if (project.scene().insert(std::move(path))
            != xq::XQScene::InsertResult::Inserted) {
            return false;
        }
        if (!project.assetRegistry().registerAsset(asset(
                kProfileAsset,
                xq::AssetKind::VesselProfile,
                "sha256:profile"))) {
            return false;
        }
        xq::XQDataNode profileNode(
            kProfileNode,
            xq::XQDomainType::VesselProfile,
            "Profile",
            std::make_shared<xq::XQVesselProfilePayload>(profile()));
        profileNode.setScaleSlot(xq::ScaleSlot::Organ);
        profileNode.setAssetId(kProfileAsset);
        if (project.scene().insert(std::move(profileNode))
                != xq::XQScene::InsertResult::Inserted
            || project.scene().link_derived(kPathNode, kProfileNode)
                != xq::XQScene::RelationResult::Linked) {
            return false;
        }
        return true;
    }

    Fixture()
    {
        ready = project.open() == xq::XQProject::LifecycleResult::Ok
            && seed();
    }
};

xq::FlowSmokeController::SmokeIntent intent(bool withAssets = true)
{
    xq::FlowSmokeController::SmokeIntent value;
    value.sourceProfileNode = kProfileNode;
    value.output.caseNode = kCaseNode;
    value.output.caseName = "L0 geometry smoke case";
    value.output.resultNode = kResultNode;
    value.output.resultName = "L0 geometry smoke result";
    if (withAssets) {
        value.output.caseAsset = asset(
            kCaseAsset, xq::AssetKind::SimulationCase, "sha256:smoke-case");
        value.output.resultAsset = asset(
            kResultAsset, xq::AssetKind::FlowResult, "sha256:smoke-result");
    }
    return value;
}

} // namespace

int main()
{
    Fixture fixture;
    CHECK(fixture.ready);
    xq::FlowSmokeController controller(&fixture.project, &fixture.stack);
    CHECK(controller.run(intent()) == xq::FlowSmokeController::Status::Ok);
    CHECK(nodeCount(fixture.project.scene()) == 4);
    CHECK(relationCount(fixture.project.scene()) == 4);
    CHECK(fixture.stack.undo_count() == 1);
    CHECK(fixture.project.assetRegistry().assetCount() == 3);
    CHECK(fixture.project.assetRegistry().relationCount() == 3);
    CHECK(fixture.project.assetRegistry().hasRelation(kProfileAsset, kCaseAsset));
    CHECK(fixture.project.assetRegistry().hasRelation(kProfileAsset, kResultAsset));
    CHECK(fixture.project.assetRegistry().hasRelation(kCaseAsset, kResultAsset));

    const xq::XQDataNode* caseNode = fixture.project.scene().find(kCaseNode);
    const xq::XQDataNode* resultNode = fixture.project.scene().find(kResultNode);
    CHECK(caseNode != nullptr && resultNode != nullptr);
    CHECK(caseNode->scaleSlot() == xq::ScaleSlot::Organ);
    CHECK(resultNode->scaleSlot() == xq::ScaleSlot::Organ);
    const std::shared_ptr<xq::XQSimulationCasePayload> casePayload =
        std::dynamic_pointer_cast<xq::XQSimulationCasePayload>(caseNode->payload());
    const std::shared_ptr<xq::XQFlowResultPayload> resultPayload =
        std::dynamic_pointer_cast<xq::XQFlowResultPayload>(resultNode->payload());
    CHECK(casePayload != nullptr && resultPayload != nullptr);
    CHECK(casePayload->simulationCase().hasFlowSmokeProvenance());
    CHECK(resultPayload->result().hasFlowSmokeProvenance());
    CHECK(resultPayload->result().isConsistent());

    CHECK(fixture.stack.undo());
    CHECK(fixture.project.scene().find(kCaseNode) == nullptr);
    CHECK(fixture.project.scene().find(kResultNode) == nullptr);
    CHECK(nodeCount(fixture.project.scene()) == 2);
    CHECK(relationCount(fixture.project.scene()) == 1);
    CHECK(fixture.project.assetRegistry().assetCount() == 1);
    CHECK(fixture.project.assetRegistry().relationCount() == 0);
    CHECK(fixture.stack.redo());
    CHECK(nodeCount(fixture.project.scene()) == 4);
    CHECK(relationCount(fixture.project.scene()) == 4);

    CHECK(fixture.project.scene().mark_source_changed(kPathNode) == 3);
    CHECK(!fixture.project.scene().is_stale(kPathNode));
    CHECK(fixture.project.scene().is_stale(kProfileNode));
    CHECK(fixture.project.scene().is_stale(kCaseNode));
    CHECK(fixture.project.scene().is_stale(kResultNode));

    const std::filesystem::path projectPath =
        std::filesystem::temp_directory_path()
        / "xq_flow_smoke_controller_roundtrip.xqproj";
    std::error_code ec;
    std::filesystem::remove(projectPath, ec);
    std::filesystem::remove_all(
        projectPath.parent_path()
            / (projectPath.stem().string() + ".assets"),
        ec);
    CHECK(xq::XQProjectWriter::save(
              fixture.project, projectPath.string())
          == xq::XQProjectWriter::Status::Ok);
    xq::XQProjectReadResult loaded;
    CHECK(xq::XQProjectReader::load(projectPath.string(), &loaded)
          == xq::XQProjectReader::Status::Ok);
    const xq::XQDataNode* loadedCase = loaded.project.scene().find(kCaseNode);
    const xq::XQDataNode* loadedResult = loaded.project.scene().find(kResultNode);
    CHECK(loadedCase != nullptr && loadedResult != nullptr);
    const std::shared_ptr<xq::XQSimulationCasePayload> loadedCasePayload =
        std::dynamic_pointer_cast<xq::XQSimulationCasePayload>(loadedCase->payload());
    const std::shared_ptr<xq::XQFlowResultPayload> loadedResultPayload =
        std::dynamic_pointer_cast<xq::XQFlowResultPayload>(loadedResult->payload());
    CHECK(loadedCasePayload != nullptr && loadedResultPayload != nullptr);
    CHECK(loadedCasePayload->simulationCase().romSettings().vesselProfileNode
          == kProfileNode);
    CHECK(loadedCasePayload->simulationCase().flowSmokeProvenance().stationMap.size()
          == 11);
    CHECK(loadedResultPayload->result().flowSmokeProvenance().protocol.id
          == "engineering-smoke-v1");
    CHECK(loadedResultPayload->result().isConsistent());
    CHECK(loaded.project.scene().is_stale(kProfileNode));
    CHECK(loaded.project.scene().is_stale(kCaseNode));
    CHECK(loaded.project.scene().is_stale(kResultNode));
    // The writer may materialize an Asset for an unbound upstream Path, but
    // the three smoke lineage edges must survive unchanged.
    CHECK(loaded.project.assetRegistry().relationCount() >= 3);
    CHECK(loaded.project.assetRegistry().hasRelation(kProfileAsset, kCaseAsset));
    CHECK(loaded.project.assetRegistry().hasRelation(kProfileAsset, kResultAsset));
    CHECK(loaded.project.assetRegistry().hasRelation(kCaseAsset, kResultAsset));
    std::filesystem::remove(projectPath, ec);
    std::filesystem::remove_all(
        projectPath.parent_path()
            / (projectPath.stem().string() + ".assets"),
        ec);

    Fixture changed;
    CHECK(changed.ready);
    xq::FlowSmokeController changedController(&changed.project, &changed.stack);
    xq::FlowSmokeController::PreparedCommand prepared =
        changedController.prepare(intent(false));
    CHECK(prepared.ok());
    xq::XQDataNode* changedProfile = changed.project.scene().find(kProfileNode);
    CHECK(changedProfile != nullptr);
    changedProfile->setPayload(
        xq::XQDomainType::VesselProfile,
        std::make_shared<xq::XQVesselProfilePayload>(profile()));
    CHECK(changedController.commitPrepared(std::move(prepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(changed.project.scene().find(kCaseNode) == nullptr);
    CHECK(changed.project.scene().find(kResultNode) == nullptr);
    CHECK(changed.stack.undo_count() == 0);

    Fixture staleCapture;
    CHECK(staleCapture.ready);
    CHECK(staleCapture.project.scene().mark_source_changed(kPathNode) == 1);
    xq::FlowSmokeController staleCaptureController(
        &staleCapture.project, &staleCapture.stack);
    CHECK(staleCaptureController.capture(intent(false)).status
          == xq::FlowSmokeController::Status::SourceStale);
    CHECK(staleCapture.stack.undo_count() == 0);

    Fixture unsupportedScale;
    CHECK(unsupportedScale.ready);
    unsupportedScale.project.scene().find(kProfileNode)->setScaleSlot(
        xq::ScaleSlot::Micro);
    xq::FlowSmokeController unsupportedScaleController(
        &unsupportedScale.project, &unsupportedScale.stack);
    CHECK(unsupportedScaleController.capture(intent(false)).status
          == xq::FlowSmokeController::Status::UnsupportedScale);

    Fixture invalidPayload;
    CHECK(invalidPayload.ready);
    invalidPayload.project.scene().find(kProfileNode)->setPayload(
        xq::XQDomainType::VesselProfile,
        std::make_shared<xq::XQSourcePayload>(
            xq::XQDomainType::VesselProfile, "unresolved-profile"));
    xq::FlowSmokeController invalidPayloadController(
        &invalidPayload.project, &invalidPayload.stack);
    CHECK(invalidPayloadController.capture(intent(false)).status
          == xq::FlowSmokeController::Status::SourcePayloadInvalid);

    Fixture revisionChanged;
    CHECK(revisionChanged.ready);
    xq::FlowSmokeController revisionController(
        &revisionChanged.project, &revisionChanged.stack);
    xq::FlowSmokeController::PreparedCommand revisionPrepared =
        revisionController.prepare(intent(false));
    CHECK(revisionPrepared.ok());
    revisionChanged.project.scene().find(kProfileNode)->setContentRevision(1);
    CHECK(revisionController.commitPrepared(std::move(revisionPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(revisionChanged.project.scene().find(kCaseNode) == nullptr);
    CHECK(revisionChanged.project.scene().find(kResultNode) == nullptr);
    CHECK(revisionChanged.stack.undo_count() == 0);

    Fixture becameStale;
    CHECK(becameStale.ready);
    xq::FlowSmokeController becameStaleController(
        &becameStale.project, &becameStale.stack);
    xq::FlowSmokeController::PreparedCommand stalePrepared =
        becameStaleController.prepare(intent(false));
    CHECK(stalePrepared.ok());
    CHECK(becameStale.project.scene().mark_source_changed(kPathNode) == 1);
    CHECK(becameStaleController.commitPrepared(std::move(stalePrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(becameStale.project.scene().find(kCaseNode) == nullptr);
    CHECK(becameStale.project.scene().find(kResultNode) == nullptr);

    Fixture sourceAssetChanged;
    CHECK(sourceAssetChanged.ready);
    xq::FlowSmokeController sourceAssetController(
        &sourceAssetChanged.project, &sourceAssetChanged.stack);
    xq::FlowSmokeController::PreparedCommand sourceAssetPrepared =
        sourceAssetController.prepare(intent(false));
    CHECK(sourceAssetPrepared.ok());
    sourceAssetChanged.project.assetRegistry().find(kProfileAsset)
        ->contentFingerprint = "sha256:changed-profile";
    CHECK(sourceAssetController.commitPrepared(std::move(sourceAssetPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(sourceAssetChanged.stack.undo_count() == 0);

    Fixture targetCollision;
    CHECK(targetCollision.ready);
    xq::FlowSmokeController targetCollisionController(
        &targetCollision.project, &targetCollision.stack);
    xq::FlowSmokeController::PreparedCommand targetPrepared =
        targetCollisionController.prepare(intent(false));
    CHECK(targetPrepared.ok());
    CHECK(targetCollision.project.scene().insert(xq::XQDataNode(
              kCaseNode, "blocker", "Blocker"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(targetCollisionController.commitPrepared(std::move(targetPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(targetCollision.project.scene().find(kResultNode) == nullptr);
    CHECK(targetCollision.stack.undo_count() == 0);

    Fixture outputAssetCollision;
    CHECK(outputAssetCollision.ready);
    xq::FlowSmokeController outputAssetController(
        &outputAssetCollision.project, &outputAssetCollision.stack);
    xq::FlowSmokeController::PreparedCommand outputAssetPrepared =
        outputAssetController.prepare(intent(true));
    CHECK(outputAssetPrepared.ok());
    CHECK(outputAssetCollision.project.assetRegistry().registerAsset(asset(
        kCaseAsset, xq::AssetKind::SimulationCase, "sha256:blocker")));
    CHECK(outputAssetController.commitPrepared(std::move(outputAssetPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(outputAssetCollision.project.scene().find(kCaseNode) == nullptr);
    CHECK(outputAssetCollision.project.scene().find(kResultNode) == nullptr);
    CHECK(outputAssetCollision.stack.undo_count() == 0);

    Fixture originProject;
    Fixture otherProject;
    CHECK(originProject.ready && otherProject.ready);
    xq::FlowSmokeController originController(
        &originProject.project, &originProject.stack);
    xq::FlowSmokeController otherController(
        &otherProject.project, &otherProject.stack);
    xq::FlowSmokeController::PreparedCommand crossProjectPrepared =
        originController.prepare(intent(false));
    CHECK(crossProjectPrepared.ok());
    CHECK(otherController.commitPrepared(std::move(crossProjectPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(originProject.project.scene().find(kCaseNode) == nullptr);
    CHECK(otherProject.project.scene().find(kCaseNode) == nullptr);
    CHECK(originProject.stack.undo_count() == 0);
    CHECK(otherProject.stack.undo_count() == 0);

    Fixture lifecycleAba;
    CHECK(lifecycleAba.ready);
    xq::FlowSmokeController lifecycleController(
        &lifecycleAba.project, &lifecycleAba.stack);
    xq::FlowSmokeController::PreparedCommand lifecyclePrepared =
        lifecycleController.prepare(intent(false));
    CHECK(lifecyclePrepared.ok());
    CHECK(lifecycleAba.project.close()
          == xq::XQProject::LifecycleResult::Ok);
    CHECK(lifecycleAba.project.reopen()
          == xq::XQProject::LifecycleResult::Ok);
    CHECK(lifecycleAba.seed());
    CHECK(lifecycleController.commitPrepared(std::move(lifecyclePrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(lifecycleAba.project.scene().find(kCaseNode) == nullptr);
    CHECK(lifecycleAba.project.scene().find(kResultNode) == nullptr);
    CHECK(lifecycleAba.stack.undo_count() == 0);

    Fixture assignedProject;
    Fixture replacementProject;
    CHECK(assignedProject.ready && replacementProject.ready);
    xq::FlowSmokeController assignmentController(
        &assignedProject.project, &assignedProject.stack);
    xq::FlowSmokeController::PreparedCommand assignmentPrepared =
        assignmentController.prepare(intent(false));
    CHECK(assignmentPrepared.ok());
    assignedProject.project = replacementProject.project;
    CHECK(assignmentController.commitPrepared(std::move(assignmentPrepared))
          == xq::FlowSmokeController::Status::SourceChanged);
    CHECK(assignedProject.project.scene().find(kCaseNode) == nullptr);
    CHECK(assignedProject.project.scene().find(kResultNode) == nullptr);
    CHECK(assignedProject.stack.undo_count() == 0);

    xq::XQProject atomicProject;
    CHECK(atomicProject.open() == xq::XQProject::LifecycleResult::Ok);
    CHECK(atomicProject.scene().insert(xq::XQDataNode(
              xq::NodeId(1), "source", "Source"))
          == xq::XQScene::InsertResult::Inserted);
    xq::XQSimulationCase emptyCase;
    xq::ProjectNodeBatchSpec first(xq::XQDataNode(
        xq::NodeId(2), xq::XQDomainType::SimulationCase, "Case",
        std::make_shared<xq::XQSimulationCasePayload>(emptyCase)));
    first.sources.push_back({xq::NodeId(1), 0, std::nullopt, ""});
    xq::XQFlowResult emptyFlow;
    xq::ProjectNodeBatchSpec second(xq::XQDataNode(
        xq::NodeId(3), xq::XQDomainType::FlowResult, "Result",
        std::make_shared<xq::XQFlowResultPayload>(emptyFlow)));
    second.sources.push_back({xq::NodeId(999), 0, std::nullopt, ""});
    std::vector<xq::ProjectNodeBatchSpec> specs;
    specs.push_back(std::move(first));
    specs.push_back(std::move(second));
    xq::XQCommandStack atomicStack;
    CHECK(!atomicStack.push(std::unique_ptr<xq::XQCommand>(
        new xq::ProjectNodeBundleCommand(
            &atomicProject, std::move(specs), "atomic failure"))));
    CHECK(atomicProject.scene().find(xq::NodeId(2)) == nullptr);
    CHECK(atomicProject.scene().find(xq::NodeId(3)) == nullptr);
    CHECK(atomicStack.undo_count() == 0);
    return 0;
}
