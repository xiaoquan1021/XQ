#include <app/WorkflowCapabilities.h>
#include <app/XQWorkflowSession.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>
#include <core/XQSimulationCasePayload.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfile.h>
#include <core/XQVesselProfilePayload.h>
#include <core/command/XQCommandStack.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>
#include <ui/controllers/PathModuleController.h>

#include <QCoreApplication>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

std::filesystem::path asset_dir_for(const std::filesystem::path& projectPath)
{
    return projectPath.parent_path() / (projectPath.stem().string() + ".assets");
}

struct TempProjectPair {
    TempProjectPair()
    {
        const std::string suffix = std::to_string(QCoreApplication::applicationPid());
        first = std::filesystem::temp_directory_path()
            / ("xq-flow-capability-" + suffix + ".xqproj");
        second = std::filesystem::temp_directory_path()
            / ("xq-flow-capability-resave-" + suffix + ".xqproj");
        cleanup();
    }

    ~TempProjectPair()
    {
        cleanup();
    }

    void cleanup()
    {
        std::error_code ec;
        std::filesystem::remove(first, ec);
        std::filesystem::remove_all(asset_dir_for(first), ec);
        std::filesystem::remove(second, ec);
        std::filesystem::remove_all(asset_dir_for(second), ec);
    }

    std::filesystem::path first;
    std::filesystem::path second;
};

xq::VesselProfileV1 historical_profile(const xq::NodeId& pathId)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "historical-frame";
    profile.sourcePathNode = pathId;
    profile.externalEvidenceId = "historical-evidence-v1";
    profile.externalEvidenceFingerprint = "sha256:historical";
    profile.derivationStamp.algorithmId = "historical-import";
    profile.derivationStamp.algorithmVersion = "1";
    profile.derivationStamp.inputs.push_back({pathId, 1, std::nullopt, std::string()});

    for (unsigned long long i = 0; i < 3; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(i + 1);
        sample.arcLengthMm = static_cast<double>(i) * 25.0;
        sample.positionMm = {static_cast<double>(i) * 25.0, 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 100.0 + static_cast<double>(i) * 10.0;
        sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
        sample.quality = xq::VesselSampleQuality::Accepted;
        profile.samples.push_back(sample);
    }
    return profile;
}

xq::FlowSmokeProtocolStamp historical_protocol()
{
    xq::FlowSmokeProtocolStamp protocol;
    protocol.id = "engineering-smoke-v1";
    protocol.version = 1;
    protocol.label = "L0 geometry smoke";
    protocol.stationCount = 11;
    protocol.dtSeconds = 1.0e-4;
    protocol.numTimeSteps = 200;
    protocol.numCycles = 2;
    return protocol;
}

bool seed_historical_flow_project(xq::XQProject* project)
{
    if (project == nullptr
        || project->open() != xq::XQProject::LifecycleResult::Ok) {
        return false;
    }

    const xq::NodeId pathId(101);
    const xq::NodeId profileId(102);
    const xq::NodeId caseId(103);
    const xq::NodeId resultId(104);

    xq::XQDataNode path(
        pathId, xq::XQDomainType::Path, "Historical path",
        std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Path, std::string()));
    path.setScaleSlot(xq::ScaleSlot::Organ);
    path.setContentRevision(1);

    xq::XQDataNode profile(
        profileId, xq::XQDomainType::VesselProfile, "Historical profile",
        std::make_shared<xq::XQVesselProfilePayload>(historical_profile(pathId)));
    profile.setScaleSlot(xq::ScaleSlot::Organ);
    profile.setContentRevision(1);

    xq::XQSimulationCase simulationCase;
    simulationCase.setId(caseId);
    xq::RomSettings rom;
    rom.vesselProfileNode = profileId;
    rom.period = 0.02;
    rom.numTimeSteps = 200;
    rom.dt = 1.0e-4;
    rom.numCycles = 2;
    simulationCase.setRomSettings(rom);
    xq::FlowSmokeCaseProvenance caseProvenance;
    caseProvenance.protocol = historical_protocol();
    caseProvenance.assemblerId = "xq-flow-input-assembler";
    caseProvenance.assemblerVersion = "1";
    caseProvenance.sourceVesselProfileNode = profileId;
    caseProvenance.sourceVesselProfileRevision = 1;
    caseProvenance.conversion.lengthScale = 0.1;
    caseProvenance.conversion.areaScale = 0.01;
    for (std::size_t i = 0; i < caseProvenance.protocol.stationCount; ++i) {
        const double normalized = static_cast<double>(i)
            / static_cast<double>(caseProvenance.protocol.stationCount - 1);
        xq::FlowStationSourceMapping mapping;
        mapping.solverStationIndex = i;
        mapping.solverArcLengthCm = normalized * 5.0;
        if (normalized <= 0.5) {
            const double local = normalized * 2.0;
            mapping.leftSampleId = xq::VesselSampleId(1);
            mapping.rightSampleId = xq::VesselSampleId(2);
            mapping.leftWeight = 1.0 - local;
            mapping.rightWeight = local;
        } else {
            const double local = (normalized - 0.5) * 2.0;
            mapping.leftSampleId = xq::VesselSampleId(2);
            mapping.rightSampleId = xq::VesselSampleId(3);
            mapping.leftWeight = 1.0 - local;
            mapping.rightWeight = local;
        }
        caseProvenance.stationMap.push_back(mapping);
    }
    simulationCase.setFlowSmokeProvenance(caseProvenance);
    xq::XQDataNode caseNode(
        caseId, xq::XQDomainType::SimulationCase, "Historical smoke case",
        std::make_shared<xq::XQSimulationCasePayload>(std::move(simulationCase)));
    caseNode.setScaleSlot(xq::ScaleSlot::Organ);

    xq::XQFlowResult flow;
    flow.setSourceCaseNode(caseId);
    flow.setTimes({0.0, 1.0e-4});
    xq::FlowSegment segment;
    segment.segmentId = 0;
    segment.arcLengthStart = 0.0;
    segment.arcLengthEnd = 5.0;
    segment.faceId = 1;
    flow.addSegment(segment);
    flow.setSeries({{1.0, 1.1}}, {{1000.0, 1001.0}}, {{1.0, 1.0}});
    flow.setConverged(true);
    flow.setMaxCfl(0.25);
    xq::FlowSmokeResultProvenance resultProvenance;
    resultProvenance.protocol = historical_protocol();
    resultProvenance.solverId = "xq-flow-solver-1d";
    resultProvenance.solverVersion = "1";
    resultProvenance.sourceVesselProfileNode = profileId;
    resultProvenance.sourceVesselProfileRevision = 1;
    flow.setFlowSmokeProvenance(resultProvenance);
    xq::XQDataNode resultNode(
        resultId, xq::XQDomainType::FlowResult, "Historical smoke result",
        std::make_shared<xq::XQFlowResultPayload>(std::move(flow)));
    resultNode.setScaleSlot(xq::ScaleSlot::Organ);

    if (project->scene().insert(std::move(path)) != xq::XQScene::InsertResult::Inserted
        || project->scene().insert(std::move(profile)) != xq::XQScene::InsertResult::Inserted
        || project->scene().insert(std::move(caseNode)) != xq::XQScene::InsertResult::Inserted
        || project->scene().insert(std::move(resultNode)) != xq::XQScene::InsertResult::Inserted) {
        return false;
    }
    return project->scene().link_derived(pathId, profileId)
            == xq::XQScene::RelationResult::Linked
        && project->scene().link_derived(profileId, caseId)
            == xq::XQScene::RelationResult::Linked
        && project->scene().link_derived(profileId, resultId)
            == xq::XQScene::RelationResult::Linked
        && project->scene().link_derived(caseId, resultId)
            == xq::XQScene::RelationResult::Linked;
}

bool historical_flow_is_intact(const xq::XQProject& project)
{
    const xq::XQDataNode* profileNode = project.scene().find(xq::NodeId(102));
    const xq::XQDataNode* caseNode = project.scene().find(xq::NodeId(103));
    const xq::XQDataNode* resultNode = project.scene().find(xq::NodeId(104));
    const auto profile = profileNode == nullptr
        ? std::shared_ptr<xq::XQVesselProfilePayload>()
        : std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(profileNode->payload());
    const auto simulationCase = caseNode == nullptr
        ? std::shared_ptr<xq::XQSimulationCasePayload>()
        : std::dynamic_pointer_cast<xq::XQSimulationCasePayload>(caseNode->payload());
    const auto result = resultNode == nullptr
        ? std::shared_ptr<xq::XQFlowResultPayload>()
        : std::dynamic_pointer_cast<xq::XQFlowResultPayload>(resultNode->payload());
    if (profile == nullptr || simulationCase == nullptr || result == nullptr
        || profile->profile().samples.size() != 3
        || profileNode->scaleSlot() != xq::ScaleSlot::Organ
        || !simulationCase->simulationCase().hasFlowSmokeProvenance()
        || simulationCase->simulationCase().romSettings().vesselProfileNode
            != xq::NodeId(102)
        || !result->result().isConsistent()
        || !result->result().converged()
        || !result->result().hasFlowSmokeProvenance()
        || result->result().flowSmokeProvenance().protocol.id
            != "engineering-smoke-v1"
        || result->result().flowQ()[0][1] != 1.1) {
        return false;
    }

    bool profileToCase = false;
    bool profileToResult = false;
    bool caseToResult = false;
    project.scene().visit_derived_relations(
        [&](const xq::NodeId& source, const xq::NodeId& derived) {
            profileToCase = profileToCase
                || (source == xq::NodeId(102) && derived == xq::NodeId(103));
            profileToResult = profileToResult
                || (source == xq::NodeId(102) && derived == xq::NodeId(104));
            caseToResult = caseToResult
                || (source == xq::NodeId(103) && derived == xq::NodeId(104));
        });
    return profileToCase && profileToResult && caseToResult;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const xq::WorkflowCapabilities defaults =
        xq::WorkflowCapabilities::compiledDefaults();
    CHECK(defaults.flowSolver1D == xq::WorkflowCapabilities::flowCompiledIn());

    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    xq::XQCommandStack stack;

    xq::XQWorkflowSession defaultSession;
    defaultSession.attach(&project, &stack);
    CHECK(defaultSession.hasFlowCapability()
          == xq::WorkflowCapabilities::flowCompiledIn());
    CHECK(defaultSession.centerlineBController() != nullptr);
#if XQ_ENABLE_FLOW
    CHECK(defaultSession.flowController() != nullptr);
    CHECK(defaultSession.flowSmokeController() != nullptr);
#else
    CHECK(defaultSession.flowController() == nullptr);
    CHECK(defaultSession.flowSmokeController() == nullptr);
#endif

    xq::XQWorkflowSession disabledSession(
        xq::WorkflowCapabilities::withoutFlow());
    disabledSession.attach(&project, &stack);
    CHECK(!disabledSession.hasFlowCapability());
    CHECK(disabledSession.flowController() == nullptr);
    CHECK(disabledSession.flowSmokeController() == nullptr);
    CHECK(disabledSession.pathController() != nullptr);
    CHECK(disabledSession.segmentationController() != nullptr);
    CHECK(disabledSession.modelingController() != nullptr);
    CHECK(disabledSession.meshingController() != nullptr);
    CHECK(disabledSession.pathModuleController() != nullptr);
    CHECK(disabledSession.pathModuleController()->modules().size() == 2);
    CHECK(disabledSession.centerlineBController() != nullptr);
    CHECK(disabledSession.aiController() != nullptr);

    xq::WorkflowCapabilities flowRequested;
    flowRequested.flowSolver1D = true;
    xq::XQWorkflowSession constrainedSession(flowRequested);
    CHECK(constrainedSession.hasFlowCapability()
          == xq::WorkflowCapabilities::flowCompiledIn());

    xq::XQProject historical;
    CHECK(seed_historical_flow_project(&historical));
    TempProjectPair files;
    CHECK(xq::XQProjectWriter::save(historical, files.first.string())
          == xq::XQProjectWriter::Status::Ok);

    xq::XQProjectReadResult firstRead;
    CHECK(xq::XQProjectReader::load(files.first.string(), &firstRead)
          == xq::XQProjectReader::Status::Ok);
    CHECK(historical_flow_is_intact(firstRead.project));

    // Loading and saving in a no-Flow binary must preserve the typed historical
    // payload and lineage; no solver/controller symbol is needed for this path.
    CHECK(xq::XQProjectWriter::save(firstRead.project, files.second.string())
          == xq::XQProjectWriter::Status::Ok);
    xq::XQProjectReadResult secondRead;
    CHECK(xq::XQProjectReader::load(files.second.string(), &secondRead)
          == xq::XQProjectReader::Status::Ok);
    CHECK(historical_flow_is_intact(secondRead.project));

    std::printf("workflow capability and historical Flow persistence checks passed\n");
    return 0;
}
