// End-to-end workflow integration test (M7) over the real 0007 aorta dataset.
//
// It drives the six stage controllers in main-line order through one shared
// scene + command stack:
//   image -> path -> segmentation -> modeling -> meshing -> flow -> AI
// then asserts the project tree contains every stage's domain node, that the
// whole workflow undoes back to the initial state (just the seeded image node),
// and that redo restores it. AI segmentation uses a mock backend (onnxruntime
// is not installed -- M6 tech debt); every other stage runs the real services
// over the real data.
//
// Real inputs: the 0007 .vti image (decoded via xq_adapter_vtk), the aorta
// .ctgr contour group and the inflow .flow waveform (read via xq_io).

#include <adapters/vtk/VtkImageAdapter.h>

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
#include <core/XQSegmentationMask.h>
#include <core/XQSimulationCase.h>
#include <core/XQSourcePayload.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>

#include <io/project/CTGRContourReader.h>

#include <services/ai/AiService.h>
#include <services/flow/BoundaryConditionService.h>
#include <services/flow/FlowSolver1D.h>
#include <core/XQAiSegmentationRequest.h>

#include <ui/controllers/AiController.h>
#include <ui/controllers/FlowController.h>
#include <ui/controllers/MeshingController.h>
#include <ui/controllers/ModelingController.h>
#include <ui/controllers/PathController.h>
#include <ui/controllers/SegmentationController.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
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

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t n = 0;
    scene.visit_nodes([&n](const xq::XQDataNode&) { ++n; });
    return n;
}

bool hasNode(const xq::XQScene& scene, const xq::NodeId& id, xq::XQDomainType domain)
{
    const xq::XQDataNode* node = scene.find(id);
    return node != nullptr && node->domainType() == domain;
}

// Polygon area of a contour by the shoelace formula in its own frame plane.
double contourArea(const xq::XQContour& contour)
{
    const std::size_t m = contour.points.size();
    if (m < 3) {
        return 0.0;
    }
    double area2 = 0.0;
    double prevU = 0.0;
    double prevV = 0.0;
    bool first = true;
    double firstU = 0.0;
    double firstV = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        double u = 0.0;
        double v = 0.0;
        xq::XQContourGroup::projectToFrame(contour.frame, contour.points[i], &u, &v);
        if (first) {
            firstU = u;
            firstV = v;
            first = false;
        } else {
            area2 += prevU * v - u * prevV;
        }
        prevU = u;
        prevV = v;
    }
    area2 += prevU * firstV - firstU * prevV;
    return std::fabs(area2) * 0.5;
}

// Mock AI segmentation backend: threshold >= mid-intensity -> label.
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
            if (buffer.scalarAt(i, 0) >= 500.0) {
                mask->setLabelAt(i, static_cast<xq::XQSegmentationMask::LabelType>(label));
            }
        }
        return mask;
    }
};

// A mesh whose boundary faces (wall/inlet/outlet) match the case BCs, the
// authoritative binding target for BoundaryConditionService.
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

xq::XQSimulationCase makeCase(const std::vector<std::pair<double, double>>& waveform,
                              double period)
{
    xq::XQSimulationCase sim;
    xq::BoundaryCondition wall = {};
    wall.faceId = 1;
    wall.type = xq::BoundaryConditionType::NoSlip;
    xq::BoundaryCondition inlet = {};
    inlet.faceId = 2;
    inlet.type = xq::BoundaryConditionType::InletFlowWaveform;
    inlet.flowWaveform = waveform;
    inlet.waveformPeriod = period;
    xq::BoundaryCondition outlet = {};
    outlet.faceId = 3;
    outlet.type = xq::BoundaryConditionType::RCR;
    outlet.rcr = {106.0, 0.00068483, 1784.0};
    sim.addBoundaryCondition(wall);
    sim.addBoundaryCondition(inlet);
    sim.addBoundaryCondition(outlet);
    return sim;
}

} // namespace

int main()
{
    xq::XQScene scene;
    xq::XQCommandStack stack;

    // Stage node ids.
    const xq::NodeId imageId(1);
    const xq::NodeId pathId(2);
    const xq::NodeId maskId(3);
    const xq::NodeId groupId(4);
    const xq::NodeId modelId(5);
    const xq::NodeId surfMeshId(6);
    const xq::NodeId volMeshId(7);
    const xq::NodeId caseId(8);
    const xq::NodeId flowId(9);
    const xq::NodeId analysisId(10);

    // ---- initial state: seed the image source node directly (the tree's root;
    // everything else is created through the command stack so the whole workflow
    // is undoable back to this point) ----
    scene.insert(xq::XQDataNode(imageId, xq::XQDomainType::Image, "0007 aorta image",
                                std::make_shared<xq::XQSourcePayload>(
                                    xq::XQDomainType::Image, "Images/OSMSC0090-cm.vti")));
    const std::size_t initialNodes = nodeCount(scene);
    CHECK(initialNodes == 1);

    std::size_t pushes = 0; // count stack-pushed commands for the undo sweep

    // ---- read the real 0007 aorta contour group (used by modeling + flow) ----
    const std::string ctgrPath = std::string(XQ_CTGR_DIR) + "/aorta_final.ctgr";
    xq::CTGRReadResult ctgrRead;
    CHECK(xq::CTGRContourReader::read(ctgrPath, &ctgrRead)
          == xq::CTGRContourReader::Status::Ok);
    CHECK(ctgrRead.group.contours().size() >= 3);

    // =====================================================================
    // 1. PATH -- control points along the vessel (interaction is programmatic
    //    in the headless build), derived from the image.
    // =====================================================================
    {
        xq::PathController controller(&scene, &stack);
        xq::PathController::AddPathIntent intent;
        intent.newPathId = pathId;
        intent.name = "aorta centerline";
        intent.sourceImageNode = imageId;
        intent.controlPoints = {{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 5.0}}, {{0.0, 0.0, 10.0}}};
        intent.spacing = 0.5;
        CHECK(controller.addPath(intent) == xq::PathController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, pathId, xq::XQDomainType::Path));

    // =====================================================================
    // 2. SEGMENTATION -- AI segmentation of the real image (mock backend).
    // =====================================================================
    {
        xq::XQImageVolume image;
        std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
        CHECK(xq::VtkImageAdapter::loadVtiWithBuffer(XQ_TEST_VTI_PATH, &image, &buffer)
              == xq::VtkImageAdapter::LoadStatus::Ok);
        CHECK(buffer != nullptr && buffer->is_valid());

        xq::SegmentationController controller(&scene, &stack);
        MockSegBackend backend;
        xq::SegmentationController::AiSegmentIntent intent;
        intent.newMaskId = maskId;
        intent.name = "aorta mask (AI)";
        intent.sourceImageNode = imageId;
        intent.image = &image;
        intent.buffer = buffer.get();
        intent.request.modelId = "mock-seg";
        intent.request.targetLabels = {1};
        CHECK(controller.aiSegment(intent, backend) == xq::SegmentationController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, maskId, xq::XQDomainType::SegmentationMask));

    // =====================================================================
    // 3. MODELING -- loft + cap the real contour group into a surface model.
    //    The contour-group node is added through the stack (M0 AddNode command)
    //    so it too is part of the undoable workflow; the model derives from it.
    // =====================================================================
    {
        stack.push(std::unique_ptr<xq::XQCommand>(new xq::AddNodeWithSourceRelationCommand(
            &scene,
            xq::XQDataNode(groupId, xq::XQDomainType::ContourGroup, "aorta contours",
                           std::shared_ptr<xq::XQPayload>()),
            pathId, "Add contour group")));
        ++pushes;

        xq::ModelingController controller(&scene, &stack);
        xq::ModelingController::LoftIntent intent;
        intent.newModelId = modelId;
        intent.name = "aorta model";
        intent.contourGroupNode = groupId;
        intent.contourGroup = ctgrRead.group;
        intent.capEnds = true;
        CHECK(controller.loft(intent) == xq::ModelingController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, groupId, xq::XQDomainType::ContourGroup));
    CHECK(hasNode(scene, modelId, xq::XQDomainType::SurfaceModel));

    // =====================================================================
    // 4. MESHING -- surface mesh, then volume mesh from the model.
    // =====================================================================
    {
        xq::MeshingController controller(&scene, &stack);

        xq::MeshingController::SurfaceMeshIntent smi;
        smi.newMeshId = surfMeshId;
        smi.name = "aorta surface mesh";
        smi.modelNode = modelId;
        CHECK(controller.buildSurfaceMesh(smi) == xq::MeshingController::Status::Ok);
        ++pushes;

        xq::MeshingController::VolumeMeshIntent vmi;
        vmi.newMeshId = volMeshId;
        vmi.name = "aorta volume mesh";
        vmi.modelNode = modelId;
        vmi.sourceNode = surfMeshId;
        CHECK(controller.buildVolumeMesh(vmi) == xq::MeshingController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, surfMeshId, xq::XQDomainType::Mesh));
    CHECK(hasNode(scene, volMeshId, xq::XQDomainType::Mesh));

    // =====================================================================
    // 5. FLOW -- build A0(x) from the real contour areas, read the real inflow
    //    waveform, validate BCs, solve. The case node is added through the
    //    stack and the flow result derives from it.
    // =====================================================================
    {
        // A0(x) from contour polygon areas ordered along the path (skip
        // degenerate / non-increasing stations), as in FlowIntegrationTest.
        std::vector<xq::XQContour> ordered = ctgrRead.group.orderedByPathPosition();
        std::vector<double> arcLength;
        std::vector<double> area0;
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            const double a = contourArea(ordered[i]);
            if (a <= 0.0) {
                continue;
            }
            if (!arcLength.empty() && ordered[i].pathArcLength <= arcLength.back()) {
                continue;
            }
            arcLength.push_back(ordered[i].pathArcLength);
            area0.push_back(a);
        }
        CHECK(arcLength.size() >= 3);

        // real inflow waveform
        const std::string flowPath = std::string(XQ_FLOW_DIR) + "/inflow_1d.flow";
        std::ifstream flowFile(flowPath.c_str());
        CHECK(flowFile.good());
        std::stringstream ss;
        ss << flowFile.rdbuf();
        const std::vector<std::pair<double, double>> waveform =
            xq::BoundaryConditionService::parseFlowFile(ss.str());
        CHECK(waveform.size() >= 100);
        const double period = waveform.back().first;
        CHECK(period > 0.5 && period < 1.5);

        // case node, derived from the volume mesh
        stack.push(std::unique_ptr<xq::XQCommand>(new xq::AddNodeWithSourceRelationCommand(
            &scene,
            xq::XQDataNode(caseId, xq::XQDomainType::SimulationCase, "aorta case",
                           std::shared_ptr<xq::XQPayload>()),
            volMeshId, "Add simulation case")));
        ++pushes;

        xq::FlowController controller(&scene, &stack);
        xq::FlowController::SolveIntent intent;
        intent.newResultId = flowId;
        intent.name = "aorta flow";
        intent.caseNode = caseId;
        intent.simulationCase = makeCase(waveform, period);
        intent.mesh = makeMesh();
        intent.solverInput.arcLength = arcLength;
        intent.solverInput.area0 = area0;
        intent.solverInput.fluid = xq::FluidProperties{};
        intent.solverInput.inletWaveform = waveform;
        intent.solverInput.period = period;
        intent.solverInput.rcr = {106.0, 0.00068483, 1784.0};
        intent.solverInput.numCycles = 2;
        intent.solverInput.numTimeSteps = 2000;
        intent.solverInput.dt = period / 2000.0;

        xq::FlowController::Status status = controller.solve(intent);
        if (status == xq::FlowController::Status::SolveFailed) {
            // Shrink dt once if the chosen step trips the CFL limit (same retry
            // discipline as FlowIntegrationTest), keeping the run deterministic.
            intent.solverInput.numTimeSteps = 20000;
            intent.solverInput.dt = period / 20000.0;
            status = controller.solve(intent);
        }
        CHECK(status == xq::FlowController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, caseId, xq::XQDomainType::SimulationCase));
    CHECK(hasNode(scene, flowId, xq::XQDomainType::FlowResult));

    // =====================================================================
    // 6. AI -- hemodynamic metrics from the flow result.
    // =====================================================================
    {
        xq::AiController controller(&scene, &stack);
        xq::AiController::AnalyzeFlowIntent intent;
        intent.newAnalysisId = analysisId;
        intent.name = "aorta flow metrics";
        intent.flowNode = flowId;
        // The reduced-order solver records wall-relative pressure centered near
        // zero (can be negative); FFR is an absolute-pressure ratio, so lift the
        // lowest segment time-mean pressure to a physiological floor (60 mmHg),
        // exactly as AiIntegrationTest does. Read the flow back from its scene
        // node (the controller's authoritative source) to size the baseline.
        const xq::XQDataNode* flowDataNode = scene.find(flowId);
        CHECK(flowDataNode != nullptr);
        const auto* flowPayload =
            dynamic_cast<const xq::XQFlowResultPayload*>(flowDataNode->payload().get());
        CHECK(flowPayload != nullptr);
        const std::vector<std::vector<double>>& Pseg = flowPayload->result().pressureP();
        double minMeanP = 1.0e300;
        for (std::size_t s = 0; s < Pseg.size(); ++s) {
            double sum = 0.0;
            for (std::size_t t = 0; t < Pseg[s].size(); ++t) {
                sum += Pseg[s][t];
            }
            const double meanP = sum / static_cast<double>(Pseg[s].size());
            if (meanP < minMeanP) {
                minMeanP = meanP;
            }
        }
        const double physiologicalFloor = 8.0e4; // 60 mmHg
        intent.request.referencePressure = physiologicalFloor - minMeanP;
        CHECK(controller.analyzeFlow(intent) == xq::AiController::Status::Ok);
        ++pushes;
    }
    CHECK(hasNode(scene, analysisId, xq::XQDomainType::AiAnalysis));

    // =====================================================================
    // Final tree: every stage's domain node is present.
    // =====================================================================
    CHECK(hasNode(scene, imageId, xq::XQDomainType::Image));
    CHECK(hasNode(scene, pathId, xq::XQDomainType::Path));
    CHECK(hasNode(scene, maskId, xq::XQDomainType::SegmentationMask));
    CHECK(hasNode(scene, groupId, xq::XQDomainType::ContourGroup));
    CHECK(hasNode(scene, modelId, xq::XQDomainType::SurfaceModel));
    CHECK(hasNode(scene, surfMeshId, xq::XQDomainType::Mesh));
    CHECK(hasNode(scene, volMeshId, xq::XQDomainType::Mesh));
    CHECK(hasNode(scene, caseId, xq::XQDomainType::SimulationCase));
    CHECK(hasNode(scene, flowId, xq::XQDomainType::FlowResult));
    CHECK(hasNode(scene, analysisId, xq::XQDomainType::AiAnalysis));

    const std::size_t fullNodes = nodeCount(scene);
    CHECK(fullNodes == initialNodes + pushes); // every push added exactly one node

    std::printf("0007 workflow: %zu stage commands, %zu nodes "
                "(image+path+mask+group+model+surfMesh+volMesh+case+flow+analysis)\n",
                pushes, fullNodes);

    // =====================================================================
    // Undo the entire workflow back to the initial (image-only) state.
    // =====================================================================
    for (std::size_t i = 0; i < pushes; ++i) {
        CHECK(stack.undo());
    }
    CHECK(!stack.can_undo());
    CHECK(nodeCount(scene) == initialNodes);
    CHECK(hasNode(scene, imageId, xq::XQDomainType::Image));
    CHECK(scene.find(pathId) == nullptr);
    CHECK(scene.find(analysisId) == nullptr);
    CHECK(scene.find(modelId) == nullptr);
    CHECK(scene.find(flowId) == nullptr);

    // =====================================================================
    // Redo the entire workflow; every stage node returns.
    // =====================================================================
    for (std::size_t i = 0; i < pushes; ++i) {
        CHECK(stack.redo());
    }
    CHECK(!stack.can_redo());
    CHECK(nodeCount(scene) == fullNodes);
    CHECK(hasNode(scene, pathId, xq::XQDomainType::Path));
    CHECK(hasNode(scene, maskId, xq::XQDomainType::SegmentationMask));
    CHECK(hasNode(scene, modelId, xq::XQDomainType::SurfaceModel));
    CHECK(hasNode(scene, volMeshId, xq::XQDomainType::Mesh));
    CHECK(hasNode(scene, flowId, xq::XQDomainType::FlowResult));
    CHECK(hasNode(scene, analysisId, xq::XQDomainType::AiAnalysis));

    std::printf("OK: 0007 end-to-end workflow (path->seg->model->mesh->flow->ai), "
                "full undo to initial + redo restored\n");
    return 0;
}
