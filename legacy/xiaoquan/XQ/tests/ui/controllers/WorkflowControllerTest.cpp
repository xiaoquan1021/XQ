// Headless workflow-controller test (M7). Each controller is driven directly
// (no Qt / widgets): build a scene + command stack, prepare the prerequisite
// nodes, hand the controller an intent, then assert the scene gains the right
// domain node + derived relation, the controller reports Ok, undo() reverts the
// node/relation, and redo() restores them. Invalid intents map to a failure
// status and leave the scene untouched.
//
// Pure C++: synthetic inputs throughout (no VTK / file I/O), so this links only
// xq_controllers. This is the false-green tamper target for the milestone.

#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQMesh.h>
#include <core/XQPayload.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>
#include <core/XQSourcePayload.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/command/XQCommandStack.h>

#include <services/ai/AiService.h>
#include <core/XQAiSegmentationRequest.h>

#include <ui/controllers/AiController.h>
#include <ui/controllers/FlowController.h>
#include <ui/controllers/MeshingController.h>
#include <ui/controllers/ModelingController.h>
#include <ui/controllers/PathController.h>
#include <ui/controllers/SegmentationController.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
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

const double kPi = 3.14159265358979323846;

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t n = 0;
    scene.visit_nodes([&n](const xq::XQDataNode&) { ++n; });
    return n;
}

std::size_t relationCount(const xq::XQScene& scene)
{
    std::size_t n = 0;
    scene.visit_derived_relations([&n](const xq::NodeId&, const xq::NodeId&) { ++n; });
    return n;
}

xq::XQDomainType domainOf(const xq::XQScene& scene, const xq::NodeId& id)
{
    const xq::XQDataNode* node = scene.find(id);
    if (node == nullptr) {
        return xq::XQDomainType::Unknown;
    }
    return node->domainType();
}

// --- synthetic single-row UInt8 image (like AiServiceTest) ---
struct SyntheticImage {
    xq::XQImageVolume image;
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
};

SyntheticImage makeImage(int dimX, const std::vector<std::uint8_t>& values)
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = dimX;
    geometry.dimensions[1] = 1;
    geometry.dimensions[2] = 1;
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 1.0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            geometry.direction[r][c] = r == c ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    SyntheticImage out;
    out.image.setGeometry(geometry);
    out.image.setScalarType(xq::ScalarType::UInt8);
    out.image.setComponentCount(1);
    std::vector<std::uint8_t> bytes(values.begin(), values.end());
    out.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, geometry.dimensions, 1, std::move(bytes));
    return out;
}

// --- synthetic circular contour (like ModelingServiceTest) ---
xq::XQContour makeCircle(int contourId, std::size_t n, double r, double z)
{
    xq::XQContour c;
    c.contourId = xq::NodeId(contourId);
    c.pathArcLength = z;
    c.frame.origin = {0.0, 0.0, z};
    c.frame.normal = {0.0, 0.0, 1.0};
    c.frame.xAxis = {1.0, 0.0, 0.0};
    c.frame.yAxis = {0.0, 1.0, 0.0};
    c.type = xq::ContourType::Circle;
    c.closed = true;
    for (std::size_t k = 0; k < n; ++k) {
        const double a = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(n);
        xq::Point3 p;
        p.x = r * std::cos(a);
        p.y = r * std::sin(a);
        p.z = z;
        c.points.push_back(p);
    }
    return c;
}

xq::XQContourGroup makeTubeGroup(std::size_t rings, std::size_t pointsPerRing)
{
    xq::XQContourGroup group;
    for (std::size_t i = 0; i < rings; ++i) {
        group.addContour(makeCircle(static_cast<int>(i + 1), pointsPerRing, 2.0,
                                    static_cast<double>(i) * 2.0));
    }
    return group;
}

// --- a mesh with wall(1)/inlet(2)/outlet(3) boundary faces ---
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

// --- a valid case: NoSlip wall + InletFlowWaveform inlet + RCR outlet ---
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

// A solver input that yields a valid, consistent flow result. The parameters
// mirror the verified-Ok transient case in FlowSolver1DTest (small dt avoids any
// CFL violation), so the controller's solve path is deterministic here.
xq::FlowSolver1D::SolverInput makeSolverInput()
{
    const std::size_t n = 11;
    std::vector<double> x(n, 0.0);
    std::vector<double> area(n, 0.5);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
    }

    xq::FlowSolver1D::SolverInput in;
    in.arcLength = x;
    in.area0 = area;
    in.fluid = xq::FluidProperties{};
    in.inletWaveform = {{0.0, 3.0}, {0.5, 8.0}, {1.0, 3.0}};
    in.period = 1.0;
    in.rcr = {106.0, 0.00068483, 1784.0};
    in.numTimeSteps = 200;
    in.dt = 1.0e-4;
    in.numCycles = 2;
    return in;
}

// A small consistent flow result to seed a flow node for the AI stage.
xq::XQFlowResult makeFlowResult()
{
    xq::XQFlowResult flow;
    flow.setTimes({0.0, 1.0});
    for (int s = 0; s < 2; ++s) {
        xq::FlowSegment seg;
        seg.segmentId = s;
        seg.arcLengthStart = static_cast<double>(s);
        seg.arcLengthEnd = static_cast<double>(s + 1);
        flow.addSegment(seg);
    }
    std::vector<std::vector<double>> q = {{5.0, 5.0}, {5.0, 5.0}};
    std::vector<std::vector<double>> p = {{1.0e5, 1.0e5}, {9.0e4, 9.0e4}};
    std::vector<std::vector<double>> a = {{kPi, kPi}, {kPi, kPi}};
    flow.setSeries(q, p, a);
    return flow;
}

// Mock AI segmentation backend: threshold >= 128 -> label.
class MockSegBackend : public xq::XQAiSegmentationBackend {
public:
    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume& image,
        const xq::XQMemoryImageBufferHandle& buffer,
        const xq::XQAiSegmentationRequest& request) override
    {
        const xq::ImageGeometry& geometry = image.geometry();
        auto mask = std::make_shared<xq::XQSegmentationMask>(geometry.dimensions);
        mask->setGeometry(geometry);
        const int label = request.targetLabels.empty() ? 1 : request.targetLabels.front();
        const std::size_t voxels = buffer.voxelCount();
        for (std::size_t i = 0; i < voxels; ++i) {
            if (buffer.scalarAt(i, 0) >= 128.0) {
                mask->setLabelAt(i, static_cast<xq::XQSegmentationMask::LabelType>(label));
            }
        }
        return mask;
    }
};

// Inserts an image source node into the scene.
void seedImage(xq::XQScene* scene, const xq::NodeId& id)
{
    scene->insert(xq::XQDataNode(id, xq::XQDomainType::Image, "img",
                                 std::make_shared<xq::XQSourcePayload>(
                                     xq::XQDomainType::Image, "Images/x.vti")));
}

} // namespace

int main()
{
    // ===================================================================
    // PathController: control points + source image -> path node + relation
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId imageId(1);
        const xq::NodeId pathId(2);
        seedImage(&scene, imageId);

        xq::PathController controller(&scene, &stack);

        xq::PathController::AddPathIntent intent;
        intent.newPathId = pathId;
        intent.name = "aorta-path";
        intent.sourceImageNode = imageId;
        intent.controlPoints = {{{0.0, 0.0, 0.0}}, {{10.0, 0.0, 0.0}}, {{10.0, 10.0, 0.0}}};
        intent.spacing = 1.0;

        const std::size_t before = nodeCount(scene);
        CHECK(controller.addPath(intent) == xq::PathController::Status::Ok);
        CHECK(nodeCount(scene) == before + 1);
        CHECK(relationCount(scene) == 1);
        CHECK(domainOf(scene, pathId) == xq::XQDomainType::Path);

        CHECK(stack.undo());
        CHECK(scene.find(pathId) == nullptr);
        CHECK(relationCount(scene) == 0);
        CHECK(stack.redo());
        CHECK(domainOf(scene, pathId) == xq::XQDomainType::Path);
        CHECK(relationCount(scene) == 1);

        const std::size_t duplicateNodes = nodeCount(scene);
        const std::size_t duplicateRelations = relationCount(scene);
        const std::size_t undoEntries = stack.undo_count();
        CHECK(controller.addPath(intent) == xq::PathController::Status::Rejected);
        CHECK(nodeCount(scene) == duplicateNodes);
        CHECK(relationCount(scene) == duplicateRelations);
        CHECK(stack.undo_count() == undoEntries);

        // invalid: only one control point -> Rejected, no scene change.
        xq::PathController::AddPathIntent bad = intent;
        bad.newPathId = xq::NodeId(3);
        bad.controlPoints = {{{0.0, 0.0, 0.0}}};
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.addPath(bad) == xq::PathController::Status::Rejected);
        CHECK(nodeCount(scene) == nodesNow);
    }

    // ===================================================================
    // SegmentationController: threshold + region grow + AI (mock backend)
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId imageId(1);
        seedImage(&scene, imageId);
        SyntheticImage syn = makeImage(4, {10, 200, 50, 130});

        xq::SegmentationController controller(&scene, &stack);

        // threshold
        const xq::NodeId maskId(10);
        xq::SegmentationController::ThresholdIntent ti;
        ti.newMaskId = maskId;
        ti.name = "thresh-mask";
        ti.sourceImageNode = imageId;
        ti.image = &syn.image;
        ti.buffer = syn.buffer.get();
        ti.params.lower = 128.0;
        ti.params.upper = 255.0;
        ti.params.foregroundLabel = 1;
        CHECK(controller.threshold(ti) == xq::SegmentationController::Status::Ok);
        CHECK(domainOf(scene, maskId) == xq::XQDomainType::SegmentationMask);
        CHECK(relationCount(scene) == 1);
        CHECK(stack.undo());
        CHECK(scene.find(maskId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, maskId) == xq::XQDomainType::SegmentationMask);

        // region grow from a foreground voxel
        const xq::NodeId rgId(11);
        xq::SegmentationController::RegionGrowIntent ri;
        ri.newMaskId = rgId;
        ri.name = "rg-mask";
        ri.sourceImageNode = imageId;
        ri.image = &syn.image;
        ri.buffer = syn.buffer.get();
        ri.params.seed[0] = 1; // voxel value 200
        ri.params.seed[1] = 0;
        ri.params.seed[2] = 0;
        ri.params.lower = 128.0;
        ri.params.upper = 255.0;
        ri.params.foregroundLabel = 1;
        CHECK(controller.regionGrow(ri) == xq::SegmentationController::Status::Ok);
        CHECK(domainOf(scene, rgId) == xq::XQDomainType::SegmentationMask);

        // AI segmentation via mock backend
        const xq::NodeId aiId(12);
        MockSegBackend backend;
        xq::SegmentationController::AiSegmentIntent ai;
        ai.newMaskId = aiId;
        ai.name = "ai-mask";
        ai.sourceImageNode = imageId;
        ai.image = &syn.image;
        ai.buffer = syn.buffer.get();
        ai.request.modelId = "mock-seg";
        ai.request.targetLabels = {1};
        CHECK(controller.aiSegment(ai, backend) == xq::SegmentationController::Status::Ok);
        CHECK(domainOf(scene, aiId) == xq::XQDomainType::SegmentationMask);
        CHECK(scene.find(aiId) != nullptr);
        CHECK(stack.undo());
        CHECK(scene.find(aiId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, aiId) == xq::XQDomainType::SegmentationMask);

        // invalid: null buffer -> Rejected, no scene change
        xq::SegmentationController::ThresholdIntent badTi = ti;
        badTi.newMaskId = xq::NodeId(99);
        badTi.buffer = nullptr;
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.threshold(badTi) == xq::SegmentationController::Status::Rejected);
        CHECK(nodeCount(scene) == nodesNow);
    }

    // ===================================================================
    // ModelingController: contour group -> capped surface model + relation
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId groupId(20);
        const xq::NodeId modelId(21);
        // contour-group node (no payload type for groups; structure only)
        scene.insert(xq::XQDataNode(groupId, xq::XQDomainType::ContourGroup, "ctgr",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::ModelingController controller(&scene, &stack);

        xq::ModelingController::LoftIntent intent;
        intent.newModelId = modelId;
        intent.name = "aorta-model";
        intent.contourGroupNode = groupId;
        intent.contourGroup = makeTubeGroup(5, 16);
        intent.capEnds = true;
        CHECK(controller.loft(intent) == xq::ModelingController::Status::Ok);
        CHECK(domainOf(scene, modelId) == xq::XQDomainType::SurfaceModel);
        CHECK(relationCount(scene) == 1); // group -> model

        // the model carries triangle geometry + the source binding
        const xq::XQDataNode* node = scene.find(modelId);
        CHECK(node != nullptr);
        const auto* payload =
            dynamic_cast<const xq::XQSurfaceModelPayload*>(node->payload().get());
        CHECK(payload != nullptr);
        CHECK(payload->model().hasTriangleGeometry());
        CHECK(payload->model().sourceContourGroupNode() == groupId);

        CHECK(stack.undo());
        CHECK(scene.find(modelId) == nullptr);
        CHECK(relationCount(scene) == 0);
        CHECK(stack.redo());
        CHECK(domainOf(scene, modelId) == xq::XQDomainType::SurfaceModel);

        // invalid: a single-ring group cannot loft -> Rejected
        xq::ModelingController::LoftIntent bad = intent;
        bad.newModelId = xq::NodeId(29);
        bad.contourGroup = makeTubeGroup(1, 16);
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.loft(bad) == xq::ModelingController::Status::Rejected);
        CHECK(nodeCount(scene) == nodesNow);
    }

    // ===================================================================
    // MeshingController: model node -> surface mesh, then volume mesh
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId groupId(30);
        const xq::NodeId modelId(31);
        const xq::NodeId surfMeshId(32);
        const xq::NodeId volMeshId(33);

        // Seed a real capped model node via the modeling controller (so the mesh
        // controller reads an authoritative model payload from the scene).
        scene.insert(xq::XQDataNode(groupId, xq::XQDomainType::ContourGroup, "ctgr",
                                    std::shared_ptr<xq::XQPayload>()));
        xq::ModelingController modeler(&scene, &stack);
        xq::ModelingController::LoftIntent loftIntent;
        loftIntent.newModelId = modelId;
        loftIntent.name = "model";
        loftIntent.contourGroupNode = groupId;
        loftIntent.contourGroup = makeTubeGroup(5, 16);
        loftIntent.capEnds = true;
        CHECK(modeler.loft(loftIntent) == xq::ModelingController::Status::Ok);

        xq::MeshingController controller(&scene, &stack);

        // surface mesh
        xq::MeshingController::SurfaceMeshIntent smi;
        smi.newMeshId = surfMeshId;
        smi.name = "surface-mesh";
        smi.modelNode = modelId;
        CHECK(controller.buildSurfaceMesh(smi) == xq::MeshingController::Status::Ok);
        CHECK(domainOf(scene, surfMeshId) == xq::XQDomainType::Mesh);
        CHECK(stack.undo());
        CHECK(scene.find(surfMeshId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, surfMeshId) == xq::XQDomainType::Mesh);

        // volume mesh, derived from the surface-mesh node
        xq::MeshingController::VolumeMeshIntent vmi;
        vmi.newMeshId = volMeshId;
        vmi.name = "volume-mesh";
        vmi.modelNode = modelId;
        vmi.sourceNode = surfMeshId;
        CHECK(controller.buildVolumeMesh(vmi) == xq::MeshingController::Status::Ok);
        CHECK(domainOf(scene, volMeshId) == xq::XQDomainType::Mesh);
        CHECK(stack.undo());
        CHECK(scene.find(volMeshId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, volMeshId) == xq::XQDomainType::Mesh);

        // invalid: missing model node -> ModelNotFound
        xq::MeshingController::SurfaceMeshIntent badSmi = smi;
        badSmi.newMeshId = xq::NodeId(39);
        badSmi.modelNode = xq::NodeId(9999);
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.buildSurfaceMesh(badSmi) == xq::MeshingController::Status::ModelNotFound);
        CHECK(nodeCount(scene) == nodesNow);
    }

    // ===================================================================
    // FlowController: validate BCs + solve -> flow result + relation
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId caseId(40);
        const xq::NodeId flowId(41);
        // simulation-case node (structure only; BCs come on the intent)
        scene.insert(xq::XQDataNode(caseId, xq::XQDomainType::SimulationCase, "case",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::FlowController controller(&scene, &stack);

        xq::FlowController::SolveIntent intent;
        intent.newResultId = flowId;
        intent.name = "flow";
        intent.caseNode = caseId;
        intent.simulationCase = makeValidCase();
        intent.mesh = makeMesh();
        intent.solverInput = makeSolverInput();
        const xq::FlowController::Status solveStatus = controller.solve(intent);
        CHECK(solveStatus == xq::FlowController::Status::Ok);
        CHECK(domainOf(scene, flowId) == xq::XQDomainType::FlowResult);
        CHECK(relationCount(scene) == 1); // case -> flow
        CHECK(stack.undo());
        CHECK(scene.find(flowId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, flowId) == xq::XQDomainType::FlowResult);

        // invalid: a case missing its outlet -> InvalidBoundaryConditions
        xq::FlowController::SolveIntent badBc = intent;
        badBc.newResultId = xq::NodeId(49);
        xq::XQSimulationCase noOutlet;
        noOutlet.addBoundaryCondition(makeValidCase().boundaryConditions()[0]); // wall
        noOutlet.addBoundaryCondition(makeValidCase().boundaryConditions()[1]); // inlet
        badBc.simulationCase = noOutlet;
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.solve(badBc)
              == xq::FlowController::Status::InvalidBoundaryConditions);
        CHECK(nodeCount(scene) == nodesNow);

        // invalid: valid BCs but a solver input the solver rejects (empty
        // stations / no waveform) -> SolveFailed, no scene change.
        xq::FlowController::SolveIntent badSolve = intent;
        badSolve.newResultId = xq::NodeId(48);
        badSolve.solverInput = xq::FlowSolver1D::SolverInput{};
        const std::size_t nodesBeforeSolveFail = nodeCount(scene);
        CHECK(controller.solve(badSolve)
              == xq::FlowController::Status::SolveFailed);
        CHECK(nodeCount(scene) == nodesBeforeSolveFail);

        // null scene/stack -> NullScene, before any validation runs.
        xq::FlowController nullController(nullptr, nullptr);
        CHECK(nullController.solve(intent) == xq::FlowController::Status::NullScene);

        // command push fails (result id collides with the existing case node) ->
        // Rejected, no scene change.
        xq::FlowController::SolveIntent collide = intent;
        collide.newResultId = caseId; // caseId already occupies the scene
        const std::size_t nodesBeforeReject = nodeCount(scene);
        CHECK(controller.solve(collide) == xq::FlowController::Status::Rejected);
        CHECK(nodeCount(scene) == nodesBeforeReject);
    }

    // ===================================================================
    // AiController: flow node -> hemodynamic-metrics analysis + relation
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId flowId(50);
        const xq::NodeId analysisId(51);
        // flow-result node with a real flow payload (so the controller reads it)
        scene.insert(xq::XQDataNode(flowId, xq::XQDomainType::FlowResult, "flow",
                                    std::make_shared<xq::XQFlowResultPayload>(makeFlowResult())));

        xq::AiController controller(&scene, &stack);

        xq::AiController::AnalyzeFlowIntent intent;
        intent.newAnalysisId = analysisId;
        intent.name = "flow-metrics";
        intent.flowNode = flowId;
        CHECK(controller.analyzeFlow(intent) == xq::AiController::Status::Ok);
        CHECK(domainOf(scene, analysisId) == xq::XQDomainType::AiAnalysis);
        CHECK(relationCount(scene) == 1); // flow -> analysis
        CHECK(stack.undo());
        CHECK(scene.find(analysisId) == nullptr);
        CHECK(stack.redo());
        CHECK(domainOf(scene, analysisId) == xq::XQDomainType::AiAnalysis);

        // invalid: a flow node id that does not exist -> FlowNotFound
        xq::AiController::AnalyzeFlowIntent bad = intent;
        bad.newAnalysisId = xq::NodeId(59);
        bad.flowNode = xq::NodeId(9999);
        const std::size_t nodesNow = nodeCount(scene);
        CHECK(controller.analyzeFlow(bad) == xq::AiController::Status::FlowNotFound);
        CHECK(nodeCount(scene) == nodesNow);
    }

    std::printf("OK: all six workflow controllers (intent -> service -> command -> "
                "stack -> scene) with undo/redo\n");
    return 0;
}
