#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQMesh.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>
#include <core/XQSimulationCasePayload.h>
#include <core/command/XQCommandStack.h>
#include <services/flow/BoundaryConditionService.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

// A mesh with wall(1)/inlet(2)/outlet(3) boundary faces, the M5 authoritative
// binding target.
xq::XQMesh makeMesh()
{
    xq::XQMesh mesh;
    xq::MeshBoundaryFace wall = {};
    wall.faceId = 1;
    wall.name = "wall";
    wall.kind = xq::FaceKind::Wall;
    xq::MeshBoundaryFace inlet = {};
    inlet.faceId = 2;
    inlet.name = "inlet";
    inlet.kind = xq::FaceKind::Inlet;
    inlet.capId = 10;
    xq::MeshBoundaryFace outlet = {};
    outlet.faceId = 3;
    outlet.name = "outlet";
    outlet.kind = xq::FaceKind::Outlet;
    outlet.capId = 11;
    mesh.addBoundaryFace(wall);
    mesh.addBoundaryFace(inlet);
    mesh.addBoundaryFace(outlet);
    return mesh;
}

// A valid case: NoSlip wall + InletFlowWaveform inlet + RCR outlet.
xq::XQSimulationCase makeValidCase()
{
    xq::XQSimulationCase sim;

    xq::BoundaryCondition wall = {};
    wall.faceId = 1;
    wall.type = xq::BoundaryConditionType::NoSlip;

    xq::BoundaryCondition inlet = {};
    inlet.faceId = 2;
    inlet.type = xq::BoundaryConditionType::InletFlowWaveform;
    inlet.flowWaveform.push_back(std::make_pair(0.0, 10.0));
    inlet.flowWaveform.push_back(std::make_pair(0.5, 20.0));
    inlet.waveformPeriod = 1.0;

    xq::BoundaryCondition outlet = {};
    outlet.faceId = 3;
    outlet.type = xq::BoundaryConditionType::RCR;
    outlet.rcr.push_back(106.0);
    outlet.rcr.push_back(0.00068483);
    outlet.rcr.push_back(1784.0);

    sim.addBoundaryCondition(wall);
    sim.addBoundaryCondition(inlet);
    sim.addBoundaryCondition(outlet);
    return sim;
}

bool sameBoundaryCondition(const xq::BoundaryCondition& lhs,
                           const xq::BoundaryCondition& rhs)
{
    return lhs.faceId == rhs.faceId
        && lhs.type == rhs.type
        && lhs.value == rhs.value
        && lhs.rcr == rhs.rcr
        && lhs.flowWaveform == rhs.flowWaveform
        && lhs.waveformPeriod == rhs.waveformPeriod;
}

bool sameSimulationCase(const xq::XQSimulationCase& lhs,
                        const xq::XQSimulationCase& rhs)
{
    if (lhs.id() != rhs.id()
        || lhs.hasSourceMeshNode() != rhs.hasSourceMeshNode()) {
        return false;
    }
    if (lhs.hasSourceMeshNode() && lhs.sourceMeshNode() != rhs.sourceMeshNode()) {
        return false;
    }

    const xq::SolverParameters& lhs_solver = lhs.solverParameters();
    const xq::SolverParameters& rhs_solver = rhs.solverParameters();
    if (lhs_solver.timeSteps != rhs_solver.timeSteps
        || lhs_solver.timeStepSize != rhs_solver.timeStepSize) {
        return false;
    }

    const std::vector<xq::BoundaryCondition>& lhs_conditions = lhs.boundaryConditions();
    const std::vector<xq::BoundaryCondition>& rhs_conditions = rhs.boundaryConditions();
    if (lhs_conditions.size() != rhs_conditions.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs_conditions.size(); ++i) {
        if (!sameBoundaryCondition(lhs_conditions[i], rhs_conditions[i])) {
            return false;
        }
    }

    const xq::FluidProperties& lhs_fluid = lhs.fluidProperties();
    const xq::FluidProperties& rhs_fluid = rhs.fluidProperties();
    if (lhs_fluid.density != rhs_fluid.density
        || lhs_fluid.viscosity != rhs_fluid.viscosity) {
        return false;
    }

    const xq::RomSettings& lhs_rom = lhs.romSettings();
    const xq::RomSettings& rhs_rom = rhs.romSettings();
    return lhs_rom.centerlineNode == rhs_rom.centerlineNode
        && lhs_rom.inletFaceIds == rhs_rom.inletFaceIds
        && lhs_rom.outletFaceIds == rhs_rom.outletFaceIds
        && lhs_rom.period == rhs_rom.period
        && lhs_rom.numTimeSteps == rhs_rom.numTimeSteps
        && lhs_rom.dt == rhs_rom.dt
        && lhs_rom.numCycles == rhs_rom.numCycles;
}

} // namespace

int main()
{
    using BCS = xq::BoundaryConditionService;

    // ---- parseFlowFile: two columns t Q, tolerate blanks / comment rows ----
    {
        const std::string text =
            "# header line\n"
            "0.0 12.5\n"
            "\n"
            "0.01 16.2\n"
            "  0.02   21.0  \n"
            "garbage row\n"
            "0.03 26.8\n";
        const std::vector<std::pair<double, double>> samples = BCS::parseFlowFile(text);
        CHECK(samples.size() == 4);
        CHECK(samples[0].first == 0.0);
        CHECK(samples[0].second == 12.5);
        CHECK(samples[1].first == 0.01);
        CHECK(samples[3].first == 0.03);
        CHECK(samples[3].second == 26.8);
    }

    // ---- validateAndBind: happy path ----
    {
        const xq::XQMesh mesh = makeMesh();
        const xq::XQSimulationCase sim = makeValidCase();
        const BCS::Result result = BCS::validateAndBind(sim, mesh);
        CHECK(result.ok());
        CHECK(result.status == BCS::Status::Ok);
        CHECK(result.validatedCase.boundaryConditions().size() == 3);
    }

    // ---- MissingFace: a BC references a face not in the mesh ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim = makeValidCase();
        xq::BoundaryCondition stray = {};
        stray.faceId = 99;
        stray.type = xq::BoundaryConditionType::NoSlip;
        sim.addBoundaryCondition(stray);
        const BCS::Result result = BCS::validateAndBind(sim, mesh);
        CHECK(result.status == BCS::Status::MissingFace);
    }

    // ---- MissingInlet: only wall + outlet ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim;
        xq::BoundaryCondition wall = {};
        wall.faceId = 1;
        wall.type = xq::BoundaryConditionType::NoSlip;
        xq::BoundaryCondition outlet = {};
        outlet.faceId = 3;
        outlet.type = xq::BoundaryConditionType::RCR;
        outlet.rcr.push_back(106.0);
        outlet.rcr.push_back(0.00068483);
        outlet.rcr.push_back(1784.0);
        sim.addBoundaryCondition(wall);
        sim.addBoundaryCondition(outlet);
        const BCS::Result result = BCS::validateAndBind(sim, mesh);
        CHECK(result.status == BCS::Status::MissingInlet);
    }

    // ---- MissingOutlet: only wall + inlet ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim;
        xq::BoundaryCondition wall = {};
        wall.faceId = 1;
        wall.type = xq::BoundaryConditionType::NoSlip;
        xq::BoundaryCondition inlet = {};
        inlet.faceId = 2;
        inlet.type = xq::BoundaryConditionType::InletFlowWaveform;
        inlet.flowWaveform.push_back(std::make_pair(0.0, 10.0));
        inlet.waveformPeriod = 1.0;
        sim.addBoundaryCondition(wall);
        sim.addBoundaryCondition(inlet);
        const BCS::Result result = BCS::validateAndBind(sim, mesh);
        CHECK(result.status == BCS::Status::MissingOutlet);
    }

    // ---- InvalidRcr: wrong size ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim = makeValidCase();
        // overwrite outlet rcr with a bad triple
        xq::XQSimulationCase bad;
        xq::BoundaryCondition wall = sim.boundaryConditions()[0];
        xq::BoundaryCondition inlet = sim.boundaryConditions()[1];
        xq::BoundaryCondition outlet = {};
        outlet.faceId = 3;
        outlet.type = xq::BoundaryConditionType::RCR;
        outlet.rcr.push_back(106.0);
        outlet.rcr.push_back(0.00068483); // only 2 -> invalid
        bad.addBoundaryCondition(wall);
        bad.addBoundaryCondition(inlet);
        bad.addBoundaryCondition(outlet);
        const BCS::Result result = BCS::validateAndBind(bad, mesh);
        CHECK(result.status == BCS::Status::InvalidRcr);
    }

    // ---- InvalidRcr: non-positive entry ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim = makeValidCase();
        xq::XQSimulationCase bad;
        bad.addBoundaryCondition(sim.boundaryConditions()[0]);
        bad.addBoundaryCondition(sim.boundaryConditions()[1]);
        xq::BoundaryCondition outlet = {};
        outlet.faceId = 3;
        outlet.type = xq::BoundaryConditionType::RCR;
        outlet.rcr.push_back(106.0);
        outlet.rcr.push_back(-1.0); // negative C
        outlet.rcr.push_back(1784.0);
        bad.addBoundaryCondition(outlet);
        const BCS::Result result = BCS::validateAndBind(bad, mesh);
        CHECK(result.status == BCS::Status::InvalidRcr);
    }

    // ---- EmptyWaveform: inlet waveform empty / no period ----
    {
        const xq::XQMesh mesh = makeMesh();
        xq::XQSimulationCase sim = makeValidCase();
        xq::XQSimulationCase bad;
        bad.addBoundaryCondition(sim.boundaryConditions()[0]);
        xq::BoundaryCondition inlet = {};
        inlet.faceId = 2;
        inlet.type = xq::BoundaryConditionType::InletFlowWaveform;
        // empty waveform
        inlet.waveformPeriod = 0.0;
        bad.addBoundaryCondition(inlet);
        bad.addBoundaryCondition(sim.boundaryConditions()[2]);
        const BCS::Result result = BCS::validateAndBind(bad, mesh);
        CHECK(result.status == BCS::Status::EmptyWaveform);
    }

    // ---- NullScene on the command path ----
    {
        const xq::XQMesh mesh = makeMesh();
        const xq::XQSimulationCase sim = makeValidCase();
        const BCS::CommandResult result =
            BCS::bindCommand(nullptr, xq::NodeId(1), sim, mesh);
        CHECK(result.status == BCS::Status::NullScene);
        CHECK(result.command == nullptr);
    }

    // ---- semantic command: revision/stale and exact payload undo/redo ----
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId caseNode(5101);
        const xq::NodeId resultNode(5102);
        const xq::NodeId resultLeafNode(5103);
        const xq::NodeId unrelatedSourceNode(5104);
        const xq::NodeId unrelatedStaleNode(5105);

        // Seed a distinctive unvalidated payload so undo must restore every
        // simulation-case field, not merely an empty boundary-condition list.
        xq::XQSimulationCase seed;
        seed.setId(caseNode);
        seed.setSourceMeshNode(xq::NodeId(5001));
        xq::SolverParameters seedSolver = {};
        seedSolver.timeSteps = 17;
        seedSolver.timeStepSize = 0.125;
        seed.setSolverParameters(seedSolver);
        xq::BoundaryCondition seedCondition = {};
        seedCondition.faceId = 77;
        seedCondition.type = xq::BoundaryConditionType::PrescribedPressure;
        seedCondition.value = 88.0;
        seed.addBoundaryCondition(seedCondition);
        xq::FluidProperties seedFluid = {};
        seedFluid.density = 1.23;
        seedFluid.viscosity = 0.045;
        seed.setFluidProperties(seedFluid);
        xq::RomSettings seedRom = {};
        seedRom.centerlineNode = xq::NodeId(5002);
        seedRom.inletFaceIds.push_back(12);
        seedRom.outletFaceIds.push_back(13);
        seedRom.period = 0.9;
        seedRom.numTimeSteps = 90;
        seedRom.dt = 0.01;
        seedRom.numCycles = 4;
        seed.setRomSettings(seedRom);

        auto seedPayload = std::make_shared<xq::XQSimulationCasePayload>(seed);
        const xq::XQDataNode node(caseNode, xq::XQDomainType::SimulationCase,
                                  "Sim Case", seedPayload);
        CHECK(scene.insert(node) == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  resultNode,
                  xq::XQDomainType::SimulationCase,
                  "Flow result",
                  std::make_shared<xq::XQSimulationCasePayload>(xq::XQSimulationCase())))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  resultLeafNode,
                  xq::XQDomainType::SimulationCase,
                  "Analysis result",
                  std::make_shared<xq::XQSimulationCasePayload>(xq::XQSimulationCase())))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  unrelatedSourceNode,
                  xq::XQDomainType::SimulationCase,
                  "Unrelated source",
                  std::make_shared<xq::XQSimulationCasePayload>(xq::XQSimulationCase())))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  unrelatedStaleNode,
                  xq::XQDomainType::SimulationCase,
                  "Unrelated stale result",
                  std::make_shared<xq::XQSimulationCasePayload>(xq::XQSimulationCase())))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.link_derived(caseNode, resultNode) == xq::XQScene::RelationResult::Linked);
        CHECK(scene.link_derived(resultNode, resultLeafNode)
              == xq::XQScene::RelationResult::Linked);
        CHECK(scene.link_derived(unrelatedSourceNode, unrelatedStaleNode)
              == xq::XQScene::RelationResult::Linked);
        CHECK(scene.mark_source_changed(unrelatedSourceNode) == 1);

        xq::XQDataNode* seededNode = scene.find(caseNode);
        CHECK(seededNode != nullptr);
        seededNode->setContentRevision(23);
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();
        CHECK(staleBefore.size() == 1);
        CHECK(scene.is_stale(unrelatedStaleNode));

        const xq::XQMesh mesh = makeMesh();
        const xq::XQSimulationCase sim = makeValidCase();
        BCS::CommandResult result = BCS::bindCommand(&scene, caseNode, sim, mesh);
        CHECK(result.ok());
        CHECK(result.command != nullptr);

        CHECK(stack.push(std::move(result.command)));
        const xq::XQDataNode* afterExec = scene.find(caseNode);
        CHECK(afterExec != nullptr);
        CHECK(afterExec->domainType() == xq::XQDomainType::SimulationCase);
        CHECK(afterExec->contentRevision() == 24);
        const auto* boundPayload =
            dynamic_cast<const xq::XQSimulationCasePayload*>(afterExec->payload().get());
        CHECK(boundPayload != nullptr);
        CHECK(sameSimulationCase(boundPayload->simulationCase(), sim));
        CHECK(!scene.is_stale(caseNode));
        CHECK(scene.is_stale(resultNode));
        CHECK(scene.is_stale(resultLeafNode));
        CHECK(scene.is_stale(unrelatedStaleNode));
        const xq::XQScene::StaleSnapshot staleAfter = scene.stale_snapshot();
        CHECK(staleAfter.size() == 3);

        CHECK(stack.undo());
        const xq::XQDataNode* afterUndo = scene.find(caseNode);
        CHECK(afterUndo != nullptr);
        CHECK(afterUndo->contentRevision() == 23);
        const auto* restored =
            dynamic_cast<const xq::XQSimulationCasePayload*>(afterUndo->payload().get());
        CHECK(restored != nullptr);
        CHECK(sameSimulationCase(restored->simulationCase(), seed));
        CHECK(scene.stale_snapshot() == staleBefore);

        CHECK(stack.redo());
        const xq::XQDataNode* afterRedo = scene.find(caseNode);
        CHECK(afterRedo != nullptr);
        CHECK(afterRedo->contentRevision() == 24);
        const auto* redone =
            dynamic_cast<const xq::XQSimulationCasePayload*>(afterRedo->payload().get());
        CHECK(redone != nullptr);
        CHECK(sameSimulationCase(redone->simulationCase(), sim));
        CHECK(scene.stale_snapshot() == staleAfter);
    }

    std::printf("OK: BoundaryConditionService parse + validation + semantic command\n");
    return 0;
}
