#include <core/XQDataNode.h>
#include <core/XQMesh.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>

#include <cmath>
#include <cstdio>
#include <cstddef>

namespace {

int fail(const char* check)
{
    std::fprintf(stderr, "FAIL: %s\n", check);
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

} // namespace

int main()
{
    {
        const xq::SimulationCaseId simulation_case_id(2101);
        const xq::NodeId source_mesh_node(2102);

        xq::XQSimulationCase simulation_case;
        simulation_case.setId(simulation_case_id);
        simulation_case.setSourceMeshNode(source_mesh_node);

        xq::SolverParameters parameters = {};
        parameters.timeSteps = 120;
        parameters.timeStepSize = 0.005;
        simulation_case.setSolverParameters(parameters);

        xq::BoundaryCondition wall = {};
        wall.faceId = 1;
        wall.type = xq::BoundaryConditionType::NoSlip;
        wall.value = 0.0;

        xq::BoundaryCondition inlet = {};
        inlet.faceId = 2;
        inlet.type = xq::BoundaryConditionType::PrescribedVelocity;
        inlet.value = 75.5;

        xq::BoundaryCondition outlet = {};
        outlet.faceId = 3;
        outlet.type = xq::BoundaryConditionType::Resistance;
        outlet.value = 1200.0;

        simulation_case.addBoundaryCondition(wall);
        simulation_case.addBoundaryCondition(inlet);
        simulation_case.addBoundaryCondition(outlet);

        if (simulation_case.id() != simulation_case_id) {
            return fail("simulation case id round-trips through accessors");
        }
        if (!simulation_case.hasSourceMeshNode()) {
            return fail("setting source mesh toggles source mesh presence");
        }
        if (simulation_case.sourceMeshNode() != source_mesh_node) {
            return fail("source mesh node round-trips through accessors");
        }
        if (simulation_case.solverParameters().timeSteps != 120) {
            return fail("solver time steps round-trip through accessors");
        }
        if (!close(simulation_case.solverParameters().timeStepSize, 0.005)) {
            return fail("solver time step size round-trips through accessors");
        }
        if (simulation_case.boundaryConditions().size() != 3) {
            return fail("boundary conditions round-trip through collection accessor");
        }
        if (simulation_case.boundaryConditions()[0].faceId != 1
            || simulation_case.boundaryConditions()[0].type != xq::BoundaryConditionType::NoSlip
            || !close(simulation_case.boundaryConditions()[0].value, 0.0)) {
            return fail("wall boundary condition metadata round-trips");
        }
        if (simulation_case.boundaryConditions()[1].faceId != 2
            || simulation_case.boundaryConditions()[1].type != xq::BoundaryConditionType::PrescribedVelocity
            || !close(simulation_case.boundaryConditions()[1].value, 75.5)) {
            return fail("inlet boundary condition metadata round-trips");
        }
        if (simulation_case.boundaryConditions()[2].faceId != 3
            || simulation_case.boundaryConditions()[2].type != xq::BoundaryConditionType::Resistance
            || !close(simulation_case.boundaryConditions()[2].value, 1200.0)) {
            return fail("outlet boundary condition metadata round-trips");
        }

        xq::BoundaryCondition found = {};
        if (!simulation_case.boundaryConditionByFaceId(2, &found)) {
            return fail("boundary condition lookup by existing face id succeeds");
        }
        if (found.faceId != inlet.faceId
            || found.type != inlet.type
            || !close(found.value, inlet.value)) {
            return fail("boundary condition lookup returns matching inlet metadata");
        }
        if (simulation_case.boundaryConditionByFaceId(999, &found)) {
            return fail("boundary condition lookup by missing face id returns false");
        }
    }

    {
        xq::XQMesh mesh;

        xq::MeshBoundaryFace wall_face = {};
        wall_face.faceId = 1;
        wall_face.name = "wall";
        wall_face.kind = xq::FaceKind::Wall;

        xq::MeshBoundaryFace inlet_face = {};
        inlet_face.faceId = 2;
        inlet_face.name = "inlet";
        inlet_face.kind = xq::FaceKind::Inlet;
        inlet_face.capId = 10;

        xq::MeshBoundaryFace outlet_face = {};
        outlet_face.faceId = 3;
        outlet_face.name = "outlet";
        outlet_face.kind = xq::FaceKind::Outlet;
        outlet_face.capId = 11;

        mesh.addBoundaryFace(wall_face);
        mesh.addBoundaryFace(inlet_face);
        mesh.addBoundaryFace(outlet_face);

        xq::XQSimulationCase simulation_case;
        simulation_case.addBoundaryCondition({1, xq::BoundaryConditionType::NoSlip, 0.0});
        simulation_case.addBoundaryCondition({2, xq::BoundaryConditionType::PrescribedVelocity, 75.5});
        simulation_case.addBoundaryCondition({3, xq::BoundaryConditionType::Resistance, 1200.0});

        for (std::size_t i = 0; i < simulation_case.boundaryConditions().size(); ++i) {
            xq::MeshBoundaryFace boundary_face = {};
            if (!mesh.boundaryFaceById(simulation_case.boundaryConditions()[i].faceId, &boundary_face)) {
                return fail("every simulation boundary condition references a mesh boundary face");
            }
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId mesh_node(2201);
        const xq::NodeId simulation_case_node(2202);
        const xq::NodeId unrelated_node(2203);

        if (scene.insert(xq::XQDataNode(mesh_node, "mesh", "Synthetic Mesh"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert mesh node");
        }
        if (scene.insert(xq::XQDataNode(simulation_case_node, "simulation_case", "Synthetic Simulation Case"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert simulation case node");
        }
        if (scene.insert(xq::XQDataNode(unrelated_node, "simulation_case", "Unrelated Simulation Case"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert unrelated simulation node");
        }
        if (scene.link_derived(mesh_node, simulation_case_node) != xq::XQScene::RelationResult::Linked) {
            return fail("link mesh as simulation case source");
        }
        if (scene.mark_source_changed(mesh_node) != 1) {
            return fail("marking mesh changed reports one newly stale simulation case");
        }
        if (!scene.is_stale(simulation_case_node)) {
            return fail("simulation case is stale after source mesh change");
        }
        if (scene.stale_reason(simulation_case_node) != xq::XQScene::StaleReason::SourceChanged) {
            return fail("simulation case stale reason is source changed");
        }
        if (scene.is_stale(unrelated_node)) {
            return fail("unrelated node is not marked stale");
        }
    }

    return 0;
}
