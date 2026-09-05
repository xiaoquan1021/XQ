#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQMesh.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>
#include <core/XQSimulationCasePayload.h>
#include <services/flow/BoundaryConditionService.h>

#include <cstddef>
#include <cstdio>
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

    // ---- command into scene + undo ----
    {
        xq::XQScene scene;
        const xq::NodeId caseNode(5101);

        // Seed the case node with an (unvalidated) case payload so ReplacePayload
        // has something to replace.
        xq::XQSimulationCase seed;
        auto seedPayload = std::make_shared<xq::XQSimulationCasePayload>(seed);
        const xq::XQDataNode node(caseNode, xq::XQDomainType::SimulationCase,
                                  "Sim Case", seedPayload);
        CHECK(scene.insert(node) == xq::XQScene::InsertResult::Inserted);

        const xq::XQMesh mesh = makeMesh();
        const xq::XQSimulationCase sim = makeValidCase();
        BCS::CommandResult result = BCS::bindCommand(&scene, caseNode, sim, mesh);
        CHECK(result.ok());
        CHECK(result.command != nullptr);

        result.command->execute();
        const xq::XQDataNode* afterExec = scene.find(caseNode);
        CHECK(afterExec != nullptr);
        CHECK(afterExec->domainType() == xq::XQDomainType::SimulationCase);
        const auto* boundPayload =
            dynamic_cast<const xq::XQSimulationCasePayload*>(afterExec->payload().get());
        CHECK(boundPayload != nullptr);
        CHECK(boundPayload->simulationCase().boundaryConditions().size() == 3);

        result.command->undo();
        const xq::XQDataNode* afterUndo = scene.find(caseNode);
        CHECK(afterUndo != nullptr);
        const auto* restored =
            dynamic_cast<const xq::XQSimulationCasePayload*>(afterUndo->payload().get());
        CHECK(restored != nullptr);
        CHECK(restored->simulationCase().boundaryConditions().empty());
    }

    std::printf("OK: BoundaryConditionService parse + validation + command/undo\n");
    return 0;
}
