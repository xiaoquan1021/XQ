// S4 acceptance: entity payload round-trip through content-addressed blobs.
// Covers (AC3/4/5/6/9): segMask / surface / mesh large arrays survive save->load
// field for field via blobs; small-scalar payloads (source/path/simCase/
// flowResult/aiAnalysis) survive in the assets text block; the main document
// carries NO per-point / per-tet rows (AC4); a corrupt or missing blob fails the
// whole load with a structured ASSET_BLOB_* error (AC5); an external image asset
// carries no voxel blob (AC6).

#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQAiAnalysis.h>
#include <core/XQAiAnalysisPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQImageVolume.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/XQSegmentation.h>
#include <core/XQSegmentationMask.h>
#include <core/XQSegmentationMaskPayload.h>
#include <core/XQSimulationCase.h>
#include <core/XQSimulationCasePayload.h>
#include <core/XQSourcePayload.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::filesystem::path temp_dir()
{
    return std::filesystem::temp_directory_path() / "xq_s4_payload_roundtrip";
}

void build_all(xq::XQProject* project);

bool set_blob_token(const std::filesystem::path& path,
                    const std::string& role,
                    std::size_t tokenIndex,
                    const std::string& value)
{
    std::vector<std::string> lines;
    {
        std::ifstream input(path.c_str());
        std::string line;
        while (std::getline(input, line)) {
            lines.push_back(line);
        }
    }

    bool changed = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::istringstream stream(lines[i]);
        std::vector<std::string> tokens;
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
        if (tokens.size() == 10 && tokens[0] == "blob" && tokens[1] == role) {
            if (tokenIndex >= tokens.size()) {
                return false;
            }
            tokens[tokenIndex] = value;
            std::string rebuilt = "  ";
            for (std::size_t t = 0; t < tokens.size(); ++t) {
                if (t != 0) {
                    rebuilt += " ";
                }
                rebuilt += tokens[t];
            }
            lines[i] = rebuilt;
            changed = true;
            break;
        }
    }
    if (!changed) {
        return false;
    }

    std::ofstream output(path.c_str(), std::ios::trunc);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i] << "\n";
    }
    return static_cast<bool>(output);
}

// Rewrites the token following "sampleSpacing" on the path payload line, so a
// test can inject a hostile spacing without hand-authoring the whole payload
// (see memory: hand-built xqproj text is error-prone). Returns false if no path
// line with a sampleSpacing token was found.
bool set_path_sample_spacing(const std::filesystem::path& path,
                             const std::string& value)
{
    std::vector<std::string> lines;
    {
        std::ifstream input(path.c_str());
        std::string line;
        while (std::getline(input, line)) {
            lines.push_back(line);
        }
    }

    bool changed = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::istringstream stream(lines[i]);
        std::vector<std::string> tokens;
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
        if (tokens.empty() || tokens[0] != "pathId") {
            continue;
        }
        for (std::size_t t = 0; t + 1 < tokens.size(); ++t) {
            if (tokens[t] == "sampleSpacing") {
                tokens[t + 1] = value;
                std::string rebuilt = "    ";
                for (std::size_t k = 0; k < tokens.size(); ++k) {
                    if (k != 0) {
                        rebuilt += " ";
                    }
                    rebuilt += tokens[k];
                }
                lines[i] = rebuilt;
                changed = true;
                break;
            }
        }
        if (changed) {
            break;
        }
    }
    if (!changed) {
        return false;
    }

    std::ofstream output(path.c_str(), std::ios::trunc);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i] << "\n";
    }
    return static_cast<bool>(output);
}

int expect_load_fails(const std::filesystem::path& path,
                      bool lazyGeometry,
                      const char* requiredDiagnostic,
                      const char* message)
{
    xq::XQProjectReadResult result = {};
    xq::XQProjectReader::Status status = xq::XQProjectReader::Status::ParseError;
    if (lazyGeometry) {
        xq::XQProjectReadOptions opts;
        opts.lazyGeometry = true;
        status = xq::XQProjectReader::load(path.string(), &result, opts);
    } else {
        status = xq::XQProjectReader::load(path.string(), &result);
    }
    if (status == xq::XQProjectReader::Status::Ok) {
        return fail(message, __LINE__);
    }
    if (requiredDiagnostic == 0) {
        return 0;
    }
    bool saw = false;
    for (std::vector<xq::Diagnostic>::const_iterator it = result.diagnostics.begin();
         it != result.diagnostics.end();
         ++it) {
        if (it->code() == requiredDiagnostic) {
            saw = true;
        }
    }
    return saw ? 0 : fail(message, __LINE__);
}

int save_all_project(const char* stem, std::filesystem::path* outPath)
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / (std::string(stem) + ".xqproj");
    xq::XQProject project;
    project.open();
    build_all(&project);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save schema test project", __LINE__);
    }
    *outPath = path;
    return 0;
}

// Node ids for each payload-bearing node.
const xq::NodeId kSegNode(5101);
const xq::NodeId kSurfNode(5102);
const xq::NodeId kMeshNode(5103);
const xq::NodeId kSimNode(5104);
const xq::NodeId kFlowNode(5105);
const xq::NodeId kAiNode(5106);
const xq::NodeId kPathNode(5107);
const xq::NodeId kSourceNode(5108);

void build_seg_mask(xq::XQProject* project)
{
    int dims[3] = {4, 4, 2};
    xq::XQSegmentationMask mask(dims);
    // A few foreground voxels.
    mask.setLabelAt(mask.voxelIndex(1, 1, 0), 1);
    mask.setLabelAt(mask.voxelIndex(2, 2, 1), 2);
    mask.setLabelAt(mask.voxelIndex(3, 0, 1), 1);
    xq::ImageGeometry g = {};
    g.dimensions[0] = 4;
    g.dimensions[1] = 4;
    g.dimensions[2] = 2;
    g.spacing[0] = 0.5;
    g.spacing[1] = 0.5;
    g.spacing[2] = 0.75;
    g.origin[0] = -1.0;
    g.origin[1] = 2.0;
    g.origin[2] = -3.5;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            g.direction[r][c] = (r == c) ? 1.0 : 0.0;
        }
    }
    g.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    mask.setGeometry(g);
    mask.setSourceImageNode(xq::NodeId(4001));
    std::vector<xq::SegmentationLabel> labels;
    labels.push_back({1, "lumen"});
    labels.push_back({2, "wall"});
    mask.setLabels(labels);

    project->scene().insert(xq::XQDataNode(
        kSegNode, xq::XQDomainType::SegmentationMask, "Mask",
        std::make_shared<xq::XQSegmentationMaskPayload>(std::move(mask))));
}

void build_surface(xq::XQProject* project)
{
    auto handle = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    handle->addPoint({0.0, 0.0, 0.0});
    handle->addPoint({1.0, 0.0, 0.0});
    handle->addPoint({0.0, 1.0, 0.0});
    handle->addPoint({0.0, 0.0, 1.0});
    handle->addTriangle(0, 1, 2, 7);
    handle->addTriangle(0, 1, 3, 7);
    handle->addTriangle(0, 2, 3, 8);
    handle->addTriangle(1, 2, 3, 8);

    xq::XQSurfaceModel model;
    model.setId(xq::NodeId(6001));
    model.setSource(xq::ModelSource::Generated);
    model.setSourceContourGroupNode(xq::NodeId(4002));
    xq::PreservedVtpArrays preserved = {};
    preserved.hasGlobalNodeID = true;
    preserved.hasModelFaceID = true;
    model.setPreservedArrays(preserved);
    model.setTriangleGeometry(handle);
    xq::ModelFace f1 = {};
    f1.faceId = 7;
    f1.name = "wall";
    f1.kind = xq::FaceKind::Wall;
    f1.boundaryLoopIds.push_back(1);
    f1.boundaryLoopIds.push_back(2);
    model.addFace(f1);
    xq::ModelFace f2 = {};
    f2.faceId = 8;
    f2.name = "outlet";
    f2.kind = xq::FaceKind::Outlet;
    f2.capId = 3;
    model.addFace(f2);

    project->scene().insert(xq::XQDataNode(
        kSurfNode, xq::XQDomainType::SurfaceModel, "Surface",
        std::make_shared<xq::XQSurfaceModelPayload>(std::move(model))));
}

void build_mesh(xq::XQProject* project)
{
    auto surf = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    surf->addPoint({0.0, 0.0, 0.0});
    surf->addPoint({1.0, 0.0, 0.0});
    surf->addPoint({0.0, 1.0, 0.0});
    surf->addTriangle(0, 1, 2, 5);

    auto vol = std::make_shared<xq::XQTetVolumeMeshHandle>();
    vol->addPoint({0.0, 0.0, 0.0});
    vol->addPoint({1.0, 0.0, 0.0});
    vol->addPoint({0.0, 1.0, 0.0});
    vol->addPoint({0.0, 0.0, 1.0});
    vol->addPoint({0.25, 0.25, 0.25});
    vol->addTet(0, 1, 2, 4);
    vol->addTet(0, 1, 3, 4);

    xq::XQMesh mesh;
    mesh.setId(xq::NodeId(7001));
    mesh.setSourceModelNode(xq::NodeId(6001));
    xq::PreservedMeshArrays preserved = {};
    preserved.hasGlobalElementID = true;
    mesh.setPreservedArrays(preserved);
    xq::MeshQualitySummary q = {};
    q.minQuality = 0.12;
    q.maxQuality = 0.98;
    q.meanQuality = 0.55;
    q.elementCount = 2;
    mesh.setQuality(q);
    mesh.setSurfaceTriangles(surf);
    mesh.setVolumeTets(vol);
    mesh.addRegion({1, "fluid"});
    xq::MeshBoundaryFace bf = {};
    bf.faceId = 5;
    bf.name = "inlet";
    bf.kind = xq::FaceKind::Inlet;
    bf.capId = 9;
    bf.cellIds.push_back(0);
    bf.localFaces.push_back(3);
    mesh.addBoundaryFace(bf);

    project->scene().insert(xq::XQDataNode(
        kMeshNode, xq::XQDomainType::Mesh, "Mesh",
        std::make_shared<xq::XQMeshPayload>(std::move(mesh))));
}

void build_sim_case(xq::XQProject* project)
{
    xq::XQSimulationCase sim;
    sim.setId(xq::NodeId(8001));
    sim.setSourceMeshNode(xq::NodeId(7001));
    xq::SolverParameters solver = {};
    solver.timeSteps = 200;
    solver.timeStepSize = 0.001;
    sim.setSolverParameters(solver);
    xq::FluidProperties fluid = {};
    fluid.density = 1.06;
    fluid.viscosity = 0.04;
    sim.setFluidProperties(fluid);
    xq::RomSettings rom = {};
    rom.centerlineNode = xq::NodeId(9001);
    rom.inletFaceIds.push_back(1);
    rom.outletFaceIds.push_back(8);
    rom.outletFaceIds.push_back(9);
    rom.period = 0.8;
    rom.numTimeSteps = 1000;
    rom.dt = 0.0008;
    rom.numCycles = 3;
    sim.setRomSettings(rom);
    xq::BoundaryCondition rcr = {};
    rcr.faceId = 8;
    rcr.type = xq::BoundaryConditionType::RCR;
    rcr.value = 0.0;
    rcr.rcr.push_back(100.0);
    rcr.rcr.push_back(0.0001);
    rcr.rcr.push_back(1200.0);
    sim.addBoundaryCondition(rcr);
    xq::BoundaryCondition inlet = {};
    inlet.faceId = 1;
    inlet.type = xq::BoundaryConditionType::InletFlowWaveform;
    inlet.flowWaveform.push_back(std::make_pair(0.0, 1.0));
    inlet.flowWaveform.push_back(std::make_pair(0.4, 5.0));
    inlet.waveformPeriod = 0.8;
    sim.addBoundaryCondition(inlet);

    project->scene().insert(xq::XQDataNode(
        kSimNode, xq::XQDomainType::SimulationCase, "Case",
        std::make_shared<xq::XQSimulationCasePayload>(std::move(sim))));
}

void build_flow_result(xq::XQProject* project)
{
    xq::XQFlowResult flow;
    flow.setSourceCaseNode(xq::NodeId(8001));
    std::vector<double> times;
    times.push_back(0.0);
    times.push_back(0.4);
    flow.setTimes(times);
    xq::FlowSegment s0 = {};
    s0.segmentId = 0;
    s0.arcLengthStart = 0.0;
    s0.arcLengthEnd = 1.5;
    s0.faceId = 1;
    flow.addSegment(s0);
    xq::FlowSegment s1 = {};
    s1.segmentId = 1;
    s1.arcLengthStart = 1.5;
    s1.arcLengthEnd = 3.0;
    s1.faceId = 8;
    flow.addSegment(s1);
    std::vector<std::vector<double>> q;
    std::vector<std::vector<double>> p;
    std::vector<std::vector<double>> a;
    q.push_back({1.0, 2.0});
    q.push_back({3.0, 4.0});
    p.push_back({10.0, 20.0});
    p.push_back({30.0, 40.0});
    a.push_back({0.5, 0.6});
    a.push_back({0.7, 0.8});
    flow.setSeries(q, p, a);
    flow.setConverged(true);
    flow.setMaxCfl(0.42);

    project->scene().insert(xq::XQDataNode(
        kFlowNode, xq::XQDomainType::FlowResult, "Flow",
        std::make_shared<xq::XQFlowResultPayload>(std::move(flow))));
}

void build_ai_analysis(xq::XQProject* project)
{
    xq::XQAiAnalysis ai;
    ai.setKind(xq::AnalysisKind::FlowMetrics);
    ai.setProvenance(xq::AnalysisProvenance::Computed);
    ai.setModelId("ffr-net-v2");
    ai.setSourceNode(xq::NodeId(8001));
    ai.setDiagnostic("ok");
    ai.addMetric({"FFR", 0.78, ""});
    ai.addMetric({"WSS_max", 1234.5, "dyn/cm^2"});
    xq::Annotation ann = {};
    ann.label = "stenosis";
    ann.arcLength = 1.2;
    ann.faceId = 8;
    ann.score = 0.91;
    ai.addAnnotation(ann);

    project->scene().insert(xq::XQDataNode(
        kAiNode, xq::XQDomainType::AiAnalysis, "AI",
        std::make_shared<xq::XQAiAnalysisPayload>(std::move(ai))));
}

void build_path(xq::XQProject* project)
{
    xq::XQPath path;
    path.setId(xq::NodeId(3001));
    path.setInterpolation(xq::PathInterpolation::Spline);
    path.setSourceImageNode(xq::NodeId(4001));
    std::vector<xq::PathControlPoint> cps;
    cps.push_back({{0.0, 0.0, 0.0}});
    cps.push_back({{0.0, 0.0, 1.0}});
    cps.push_back({{0.0, 0.0, 2.0}});
    path.setControlPoints(cps);

    project->scene().insert(xq::XQDataNode(
        kPathNode, xq::XQDomainType::Path, "Path",
        std::make_shared<xq::XQPathPayload>(std::move(path))));
}

void build_source(xq::XQProject* project)
{
    project->scene().insert(xq::XQDataNode(
        kSourceNode, xq::XQDomainType::Image, "Image",
        std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Image, "images/OSMSC0090.vti")));
}

void build_all(xq::XQProject* project)
{
    build_seg_mask(project);
    build_surface(project);
    build_mesh(project);
    build_sim_case(project);
    build_flow_result(project);
    build_ai_analysis(project);
    build_path(project);
    build_source(project);
}

template <typename PayloadT>
std::shared_ptr<PayloadT> payload_of(const xq::XQProject& project, const xq::NodeId& id)
{
    const xq::XQDataNode* node = project.scene().find(id);
    if (node == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<PayloadT>(node->payload());
}

int check_seg_mask(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQSegmentationMaskPayload>(project, kSegNode);
    if (!payload) {
        return fail("seg mask payload restored", __LINE__);
    }
    const xq::XQSegmentationMask& mask = payload->mask();
    if (mask.dimensionX() != 4 || mask.dimensionY() != 4 || mask.dimensionZ() != 2) {
        return fail("seg mask dims", __LINE__);
    }
    if (mask.labelAt(mask.voxelIndex(1, 1, 0)) != 1
        || mask.labelAt(mask.voxelIndex(2, 2, 1)) != 2
        || mask.labelAt(mask.voxelIndex(3, 0, 1)) != 1
        || mask.labelAt(mask.voxelIndex(0, 0, 0)) != 0) {
        return fail("seg mask voxel labels", __LINE__);
    }
    if (!mask.hasGeometry()
        || mask.geometry().spacing[2] != 0.75
        || mask.geometry().origin[2] != -3.5
        || mask.geometry().coordinateSystem != xq::ImageCoordinateSystem::LPS) {
        return fail("seg mask geometry", __LINE__);
    }
    if (!mask.hasSourceImageNode() || mask.sourceImageNode() != xq::NodeId(4001)) {
        return fail("seg mask source image", __LINE__);
    }
    if (mask.labels().size() != 2
        || mask.labels()[0].value != 1 || mask.labels()[0].name != "lumen"
        || mask.labels()[1].value != 2 || mask.labels()[1].name != "wall") {
        return fail("seg mask labels", __LINE__);
    }
    return 0;
}

int check_surface(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQSurfaceModelPayload>(project, kSurfNode);
    if (!payload) {
        return fail("surface payload restored", __LINE__);
    }
    const xq::XQSurfaceModel& model = payload->model();
    if (model.id() != xq::NodeId(6001) || model.source() != xq::ModelSource::Generated) {
        return fail("surface id/source", __LINE__);
    }
    if (!model.hasSourceContourGroupNode() || model.sourceContourGroupNode() != xq::NodeId(4002)) {
        return fail("surface source contour", __LINE__);
    }
    if (!model.preservedArrays().hasGlobalNodeID || !model.preservedArrays().hasModelFaceID
        || model.preservedArrays().hasGlobalElementID || model.preservedArrays().hasCapID) {
        return fail("surface preserved arrays", __LINE__);
    }
    if (!model.hasTriangleGeometry()) {
        return fail("surface has triangle geometry", __LINE__);
    }
    const xq::XQTriangleSurfaceGeometryHandle& g = *model.triangleGeometry();
    if (g.pointCount() != 4 || g.triangleCount() != 4) {
        return fail("surface point/triangle counts", __LINE__);
    }
    if (g.point(1).x != 1.0 || g.point(3).z != 1.0) {
        return fail("surface point coords", __LINE__);
    }
    if (g.triangle(0)[0] != 0 || g.triangle(0)[1] != 1 || g.triangle(0)[2] != 2) {
        return fail("surface triangle indices", __LINE__);
    }
    if (g.triangleFaceId(0) != 7 || g.triangleFaceId(3) != 8) {
        return fail("surface triangle face ids", __LINE__);
    }
    if (model.faces().size() != 2) {
        return fail("surface face count", __LINE__);
    }
    if (model.faces()[0].faceId != 7 || model.faces()[0].name != "wall"
        || model.faces()[0].kind != xq::FaceKind::Wall
        || model.faces()[0].boundaryLoopIds.size() != 2
        || model.faces()[0].boundaryLoopIds[1] != 2) {
        return fail("surface face 0", __LINE__);
    }
    if (model.faces()[1].faceId != 8 || model.faces()[1].kind != xq::FaceKind::Outlet
        || !model.faces()[1].capId.has_value() || model.faces()[1].capId.value() != 3) {
        return fail("surface face 1", __LINE__);
    }
    return 0;
}

int check_mesh(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQMeshPayload>(project, kMeshNode);
    if (!payload) {
        return fail("mesh payload restored", __LINE__);
    }
    const xq::XQMesh& mesh = payload->mesh();
    if (mesh.id() != xq::NodeId(7001)
        || !mesh.hasSourceModelNode() || mesh.sourceModelNode() != xq::NodeId(6001)) {
        return fail("mesh id/source", __LINE__);
    }
    if (!mesh.preservedArrays().hasGlobalElementID || mesh.preservedArrays().hasGlobalNodeID) {
        return fail("mesh preserved arrays", __LINE__);
    }
    if (mesh.quality().minQuality != 0.12 || mesh.quality().maxQuality != 0.98
        || mesh.quality().meanQuality != 0.55 || mesh.quality().elementCount != 2) {
        return fail("mesh quality", __LINE__);
    }
    if (!mesh.hasSurfaceTriangles() || mesh.surfaceTriangles()->triangleCount() != 1
        || mesh.surfaceTriangles()->triangleFaceId(0) != 5) {
        return fail("mesh surface triangles", __LINE__);
    }
    if (!mesh.hasVolumeTets() || mesh.volumeTets()->pointCount() != 5
        || mesh.volumeTets()->tetCount() != 2) {
        return fail("mesh volume tet counts", __LINE__);
    }
    if (mesh.volumeTets()->point(4).x != 0.25) {
        return fail("mesh volume point coords", __LINE__);
    }
    if (mesh.volumeTets()->tet(1)[0] != 0 || mesh.volumeTets()->tet(1)[1] != 1
        || mesh.volumeTets()->tet(1)[2] != 3 || mesh.volumeTets()->tet(1)[3] != 4) {
        return fail("mesh tet indices", __LINE__);
    }
    if (mesh.regions().size() != 1 || mesh.regions()[0].regionId != 1
        || mesh.regions()[0].name != "fluid") {
        return fail("mesh regions", __LINE__);
    }
    if (mesh.boundaryFaces().size() != 1) {
        return fail("mesh boundary face count", __LINE__);
    }
    const xq::MeshBoundaryFace& bf = mesh.boundaryFaces()[0];
    if (bf.faceId != 5 || bf.name != "inlet" || bf.kind != xq::FaceKind::Inlet
        || !bf.capId.has_value() || bf.capId.value() != 9
        || bf.cellIds.size() != 1 || bf.cellIds[0] != 0
        || bf.localFaces.size() != 1 || bf.localFaces[0] != 3) {
        return fail("mesh boundary face fields", __LINE__);
    }
    return 0;
}

int check_sim_case(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQSimulationCasePayload>(project, kSimNode);
    if (!payload) {
        return fail("sim case payload restored", __LINE__);
    }
    const xq::XQSimulationCase& sim = payload->simulationCase();
    if (sim.id() != xq::NodeId(8001)
        || !sim.hasSourceMeshNode() || sim.sourceMeshNode() != xq::NodeId(7001)) {
        return fail("sim id/source", __LINE__);
    }
    if (sim.solverParameters().timeSteps != 200 || sim.solverParameters().timeStepSize != 0.001) {
        return fail("sim solver params", __LINE__);
    }
    if (sim.fluidProperties().density != 1.06 || sim.fluidProperties().viscosity != 0.04) {
        return fail("sim fluid props", __LINE__);
    }
    const xq::RomSettings& rom = sim.romSettings();
    if (rom.centerlineNode != xq::NodeId(9001) || rom.period != 0.8
        || rom.numTimeSteps != 1000 || rom.dt != 0.0008 || rom.numCycles != 3
        || rom.inletFaceIds.size() != 1 || rom.inletFaceIds[0] != 1
        || rom.outletFaceIds.size() != 2 || rom.outletFaceIds[1] != 9) {
        return fail("sim rom settings", __LINE__);
    }
    if (sim.boundaryConditions().size() != 2) {
        return fail("sim bc count", __LINE__);
    }
    const xq::BoundaryCondition& rcr = sim.boundaryConditions()[0];
    if (rcr.faceId != 8 || rcr.type != xq::BoundaryConditionType::RCR
        || rcr.rcr.size() != 3 || rcr.rcr[2] != 1200.0) {
        return fail("sim rcr bc", __LINE__);
    }
    const xq::BoundaryCondition& inlet = sim.boundaryConditions()[1];
    if (inlet.faceId != 1 || inlet.type != xq::BoundaryConditionType::InletFlowWaveform
        || inlet.flowWaveform.size() != 2
        || inlet.flowWaveform[1].first != 0.4 || inlet.flowWaveform[1].second != 5.0
        || inlet.waveformPeriod != 0.8) {
        return fail("sim inlet waveform bc", __LINE__);
    }
    return 0;
}

int check_flow_result(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQFlowResultPayload>(project, kFlowNode);
    if (!payload) {
        return fail("flow result payload restored", __LINE__);
    }
    const xq::XQFlowResult& flow = payload->result();
    if (!flow.hasSourceCaseNode() || flow.sourceCaseNode() != xq::NodeId(8001)) {
        return fail("flow source case", __LINE__);
    }
    if (flow.times().size() != 2 || flow.times()[1] != 0.4) {
        return fail("flow times", __LINE__);
    }
    if (flow.segments().size() != 2
        || flow.segments()[1].segmentId != 1 || flow.segments()[1].faceId != 8
        || flow.segments()[1].arcLengthEnd != 3.0) {
        return fail("flow segments", __LINE__);
    }
    if (flow.flowQ().size() != 2 || flow.flowQ()[1][0] != 3.0 || flow.flowQ()[1][1] != 4.0) {
        return fail("flow Q series", __LINE__);
    }
    if (flow.pressureP()[0][1] != 20.0 || flow.areaA()[1][1] != 0.8) {
        return fail("flow P/A series", __LINE__);
    }
    if (!flow.converged() || flow.maxCfl() != 0.42) {
        return fail("flow convergence", __LINE__);
    }
    return 0;
}

int check_ai_analysis(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQAiAnalysisPayload>(project, kAiNode);
    if (!payload) {
        return fail("ai analysis payload restored", __LINE__);
    }
    const xq::XQAiAnalysis& ai = payload->analysis();
    if (ai.kind() != xq::AnalysisKind::FlowMetrics
        || ai.provenance() != xq::AnalysisProvenance::Computed
        || ai.modelId() != "ffr-net-v2" || ai.diagnostic() != "ok") {
        return fail("ai scalars", __LINE__);
    }
    if (!ai.hasSourceNode() || ai.sourceNode() != xq::NodeId(8001)) {
        return fail("ai source", __LINE__);
    }
    if (ai.metrics().size() != 2
        || ai.metrics()[0].name != "FFR" || ai.metrics()[0].value != 0.78
        || ai.metrics()[1].name != "WSS_max" || ai.metrics()[1].unit != "dyn/cm^2") {
        return fail("ai metrics", __LINE__);
    }
    if (ai.annotations().size() != 1
        || ai.annotations()[0].label != "stenosis" || ai.annotations()[0].faceId != 8
        || ai.annotations()[0].score != 0.91) {
        return fail("ai annotations", __LINE__);
    }
    return 0;
}

int check_path(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQPathPayload>(project, kPathNode);
    if (!payload) {
        return fail("path payload restored", __LINE__);
    }
    const xq::XQPath& path = payload->path();
    if (path.id() != xq::NodeId(3001) || path.interpolation() != xq::PathInterpolation::Spline) {
        return fail("path id/interp", __LINE__);
    }
    if (!path.hasSourceImageNode() || path.sourceImageNode() != xq::NodeId(4001)) {
        return fail("path source image", __LINE__);
    }
    if (path.controlPoints().size() != 3 || path.controlPoints()[2].position.z != 2.0) {
        return fail("path control points", __LINE__);
    }
    return 0;
}

int check_source(const xq::XQProject& project)
{
    auto payload = payload_of<xq::XQSourcePayload>(project, kSourceNode);
    if (!payload) {
        return fail("source payload restored", __LINE__);
    }
    if (payload->domainType() != xq::XQDomainType::Image
        || payload->sourcePath() != "images/OSMSC0090.vti") {
        return fail("source path", __LINE__);
    }
    return 0;
}

// AC3/9: full entity round-trip across all payload kinds + node->asset binding.
int test_full_payload_roundtrip()
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / "full.xqproj";

    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open project", __LINE__);
    }
    build_all(&project);

    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save project", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("load project", __LINE__);
    }
    if (!result.diagnostics.empty()) {
        return fail("payload round-trip emits no diagnostics", __LINE__);
    }

    int rc = 0;
    if ((rc = check_seg_mask(result.project)) != 0) return rc;
    if ((rc = check_surface(result.project)) != 0) return rc;
    if ((rc = check_mesh(result.project)) != 0) return rc;
    if ((rc = check_sim_case(result.project)) != 0) return rc;
    if ((rc = check_flow_result(result.project)) != 0) return rc;
    if ((rc = check_ai_analysis(result.project)) != 0) return rc;
    if ((rc = check_path(result.project)) != 0) return rc;
    if ((rc = check_source(result.project)) != 0) return rc;

    // Each payload-bearing node now carries an asset id (writer auto-derived).
    const xq::XQDataNode* segNode = result.project.scene().find(kSegNode);
    if (segNode == nullptr || !segNode->hasAssetId()) {
        return fail("seg node bound to derived asset", __LINE__);
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

// AC4: the main document carries no per-point / per-tet / per-voxel rows; the
// large arrays live only in blob files referenced by blob lines.
int test_large_arrays_not_in_main_document()
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / "arrays.xqproj";

    xq::XQProject project;
    project.open();
    build_seg_mask(&project);
    build_surface(&project);
    build_mesh(&project);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save project for AC4", __LINE__);
    }

    std::ifstream input(path.c_str());
    std::string line;
    bool sawBlobLine = false;
    while (std::getline(input, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        // No raw geometry rows in the main document (these are the writer's blob
        // payloads, which must never be inlined).
        if (line.rfind("  pt ", 0) == 0 || line.rfind("pt ", 0) == 0
            || line.rfind("  tri ", 0) == 0 || line.rfind("tri ", 0) == 0
            || line.rfind("  tet ", 0) == 0 || line.rfind("tet ", 0) == 0
            || line.rfind("  voxel ", 0) == 0) {
            std::filesystem::remove_all(temp_dir());
            return fail("main document contains an inlined large-array row", __LINE__);
        }
        if (line.rfind("  blob ", 0) == 0) {
            sawBlobLine = true;
        }
    }
    input.close();
    if (!sawBlobLine) {
        std::filesystem::remove_all(temp_dir());
        return fail("main document references at least one blob", __LINE__);
    }

    // The blob files exist under <stem>.assets/blobs/...
    const std::filesystem::path assetsDir = temp_dir() / "arrays.assets" / "blobs";
    bool sawBlobFile = false;
    if (std::filesystem::exists(assetsDir)) {
        for (std::filesystem::recursive_directory_iterator it(assetsDir), end; it != end; ++it) {
            if (it->is_regular_file()) {
                sawBlobFile = true;
                break;
            }
        }
    }
    if (!sawBlobFile) {
        std::filesystem::remove_all(temp_dir());
        return fail("blob files exist under <stem>.assets/blobs", __LINE__);
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

// AC5: a missing / truncated / byteCount-mismatched / SHA-broken blob fails the
// whole load with the matching ASSET_BLOB_* error and never returns a partial
// project.
int test_corrupt_blob_fails_load()
{
    struct Case {
        const char* name;
        const char* expectedCode;
    };

    // (a) delete a blob.
    {
        std::filesystem::remove_all(temp_dir());
        std::filesystem::create_directories(temp_dir());
        const std::filesystem::path path = temp_dir() / "corrupt.xqproj";
        xq::XQProject project;
        project.open();
        build_surface(&project);
        if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
            return fail("save for delete-blob case", __LINE__);
        }
        const std::filesystem::path blobs = temp_dir() / "corrupt.assets" / "blobs";
        std::filesystem::path first;
        for (std::filesystem::recursive_directory_iterator it(blobs), end; it != end; ++it) {
            if (it->is_regular_file()) {
                first = it->path();
                break;
            }
        }
        if (first.empty()) {
            return fail("find a blob to delete", __LINE__);
        }
        std::filesystem::remove(first);

        xq::XQProjectReadResult result = {};
        const xq::XQProjectReader::Status status =
            xq::XQProjectReader::load(path.string(), &result);
        if (status == xq::XQProjectReader::Status::Ok) {
            return fail("deleted blob fails the load", __LINE__);
        }
        bool sawMissing = false;
        for (std::vector<xq::Diagnostic>::const_iterator it = result.diagnostics.begin();
             it != result.diagnostics.end();
             ++it) {
            if (it->code() == "ASSET_BLOB_MISSING") {
                sawMissing = true;
            }
        }
        if (!sawMissing) {
            return fail("deleted blob reports ASSET_BLOB_MISSING", __LINE__);
        }
    }

    // (b) truncate a blob by one byte (file shorter than byteCount).
    // (c) main-doc byteCount mismatch is covered structurally by ByteCount; we
    //     exercise corruption that flips the recomputed SHA via (d).
    // (d) flip one byte -> checksum mismatch.
    {
        std::filesystem::remove_all(temp_dir());
        std::filesystem::create_directories(temp_dir());
        const std::filesystem::path path = temp_dir() / "corrupt2.xqproj";
        xq::XQProject project;
        project.open();
        build_surface(&project);
        if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
            return fail("save for flip-byte case", __LINE__);
        }
        const std::filesystem::path blobs = temp_dir() / "corrupt2.assets" / "blobs";
        std::filesystem::path first;
        for (std::filesystem::recursive_directory_iterator it(blobs), end; it != end; ++it) {
            if (it->is_regular_file()) {
                first = it->path();
                break;
            }
        }
        if (first.empty()) {
            return fail("find a blob to flip", __LINE__);
        }
        // Flip the first byte but keep the same length (size stays equal to
        // byteCount, so this is a checksum mismatch, not a byteCount mismatch).
        std::vector<char> bytes;
        {
            std::ifstream in(first.c_str(), std::ios::binary);
            bytes.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        if (bytes.empty()) {
            return fail("blob is non-empty", __LINE__);
        }
        bytes[0] = static_cast<char>(bytes[0] ^ 0xFF);
        {
            std::ofstream out(first.c_str(), std::ios::binary | std::ios::trunc);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }

        xq::XQProjectReadResult result = {};
        const xq::XQProjectReader::Status status =
            xq::XQProjectReader::load(path.string(), &result);
        if (status == xq::XQProjectReader::Status::Ok) {
            return fail("flipped blob fails the load", __LINE__);
        }
        bool sawChecksum = false;
        for (std::vector<xq::Diagnostic>::const_iterator it = result.diagnostics.begin();
             it != result.diagnostics.end();
             ++it) {
            if (it->code() == "ASSET_BLOB_CHECKSUM_MISMATCH") {
                sawChecksum = true;
            }
        }
        if (!sawChecksum) {
            return fail("flipped blob reports ASSET_BLOB_CHECKSUM_MISMATCH", __LINE__);
        }
    }

    // (b) truncate: drop the last byte -> file shorter than byteCount.
    {
        std::filesystem::remove_all(temp_dir());
        std::filesystem::create_directories(temp_dir());
        const std::filesystem::path path = temp_dir() / "corrupt3.xqproj";
        xq::XQProject project;
        project.open();
        build_surface(&project);
        if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
            return fail("save for truncate case", __LINE__);
        }
        const std::filesystem::path blobs = temp_dir() / "corrupt3.assets" / "blobs";
        std::filesystem::path first;
        for (std::filesystem::recursive_directory_iterator it(blobs), end; it != end; ++it) {
            if (it->is_regular_file()) {
                first = it->path();
                break;
            }
        }
        if (first.empty()) {
            return fail("find a blob to truncate", __LINE__);
        }
        std::vector<char> bytes;
        {
            std::ifstream in(first.c_str(), std::ios::binary);
            bytes.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        if (bytes.size() < 2) {
            return fail("blob long enough to truncate", __LINE__);
        }
        bytes.pop_back();
        {
            std::ofstream out(first.c_str(), std::ios::binary | std::ios::trunc);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }

        xq::XQProjectReadResult result = {};
        const xq::XQProjectReader::Status status =
            xq::XQProjectReader::load(path.string(), &result);
        if (status == xq::XQProjectReader::Status::Ok) {
            return fail("truncated blob fails the load", __LINE__);
        }
        bool sawSizeError = false;
        for (std::vector<xq::Diagnostic>::const_iterator it = result.diagnostics.begin();
             it != result.diagnostics.end();
             ++it) {
            if (it->code() == "ASSET_BLOB_BYTECOUNT_MISMATCH"
                || it->code() == "ASSET_BLOB_TRUNCATED") {
                sawSizeError = true;
            }
        }
        if (!sawSizeError) {
            return fail("truncated blob reports a size error", __LINE__);
        }
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

int test_blob_metadata_schema_fails_load()
{
    // Wrong role schema: points must be F64x3. Changing components must fail
    // before any eager or lazy path can accept the project.
    {
        std::filesystem::path path;
        int result = save_all_project("bad_points_schema", &path);
        if (result != 0) {
            return result;
        }
        if (!set_blob_token(path, "points", 8, "2")) {
            return fail("mutate points components", __LINE__);
        }
        result = expect_load_fails(path, false, 0, "wrong points components fails eager load");
        if (result != 0) {
            return result;
        }
        result = expect_load_fails(path, true, 0, "wrong points components fails lazy load");
        if (result != 0) {
            return result;
        }
    }

    // Parallel role count: faceId count must equal tris count. Keep faceId's own
    // byteCount logically consistent so this proves cross-role validation.
    {
        std::filesystem::path path;
        int result = save_all_project("bad_faceid_count", &path);
        if (result != 0) {
            return result;
        }
        if (!set_blob_token(path, "faceId", 3, "4")
            || !set_blob_token(path, "faceId", 9, "1")) {
            return fail("mutate faceId count", __LINE__);
        }
        result = expect_load_fails(path, false, 0, "faceId/tris count mismatch fails load");
        if (result != 0) {
            return result;
        }
    }

    // Segmentation voxels must match maskDims. Keep the blob line self-consistent
    // (U8x1, byteCount=elementCount) so rebuild_seg_mask catches the semantic
    // mismatch and reports InvalidMetadata.
    {
        std::filesystem::path path;
        int result = save_all_project("bad_voxel_count", &path);
        if (result != 0) {
            return result;
        }
        if (!set_blob_token(path, "voxels", 3, "31")
            || !set_blob_token(path, "voxels", 9, "31")) {
            return fail("mutate voxels count", __LINE__);
        }
        result = expect_load_fails(path, false, "ASSET_BLOB_INVALID_METADATA",
                                   "voxels/maskDims count mismatch reports invalid metadata");
        if (result != 0) {
            return result;
        }
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

// AC6: an external image asset (registered metadata) carries no voxel blob and
// round-trips its locator/geometry without any blob file.
int test_external_image_no_blob()
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / "image.xqproj";

    xq::XQProject project;
    project.open();
    xq::AssetRegistry& registry = project.assetRegistry();
    const xq::AssetId imageAsset =
        registry.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::AssetRecord* image = registry.find(imageAsset);
    image->displayName = "CT";
    image->sourceAbsPath = "C:/data/ct.vti";
    image->sourceRelPath = "images/ct.vti";
    image->hasGeometry = true;
    image->geometry.dimensions[0] = 128;
    image->geometry.dimensions[1] = 128;
    image->geometry.dimensions[2] = 60;
    image->geometry.spacing[0] = 0.5;
    image->geometry.spacing[1] = 0.5;
    image->geometry.spacing[2] = 0.7;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            image->geometry.direction[r][c] = (r == c) ? 1.0 : 0.0;
        }
    }
    image->geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    // A node bound to the external image asset, carrying only a source payload.
    project.scene().insert(xq::XQDataNode(xq::NodeId(2001), "image.volume", "Volume"));
    project.scene().find(xq::NodeId(2001))->setAssetId(imageAsset);

    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save external image project", __LINE__);
    }

    // No blob files were produced (an external image has no voxel blob).
    const std::filesystem::path blobs = temp_dir() / "image.assets" / "blobs";
    if (std::filesystem::exists(blobs)) {
        for (std::filesystem::recursive_directory_iterator it(blobs), end; it != end; ++it) {
            if (it->is_regular_file()) {
                std::filesystem::remove_all(temp_dir());
                return fail("external image produced no voxel blob", __LINE__);
            }
        }
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("load external image project", __LINE__);
    }
    const xq::AssetRecord* restored = result.project.assetRegistry().find(imageAsset);
    if (restored == nullptr || restored->category != xq::AssetCategory::ExternalSource
        || restored->kind != xq::AssetKind::Image) {
        return fail("external image asset restored", __LINE__);
    }
    if (restored->sourceAbsPath != "C:/data/ct.vti" || restored->sourceRelPath != "images/ct.vti"
        || !restored->hasGeometry || restored->geometry.dimensions[2] != 60
        || restored->geometry.spacing[2] != 0.7) {
        return fail("external image metadata round-trip", __LINE__);
    }
    if (!restored->blobs.empty()) {
        return fail("external image carries no blobs", __LINE__);
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

// AC-sidecar: after saving a project with geometry, every geometry blob file
// must have a matching .merkle sidecar alongside it (so MappedGeometrySource
// can use SegmentedMerkle mode). Sidecar presence is the observable result of
// the writer's new sidecar-emission path.
int test_geometry_sidecars_written()
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / "sidecar.xqproj";

    xq::XQProject project;
    project.open();
    build_surface(&project);
    build_mesh(&project);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save project for sidecar test", __LINE__);
    }

    // Every .bin geometry blob under the assets dir should have a .merkle next
    // to it. Walk the blobs subtree and check.
    const std::filesystem::path blobsDir = temp_dir() / "sidecar.assets" / "blobs";
    if (!std::filesystem::exists(blobsDir)) {
        return fail("sidecar.assets/blobs exists", __LINE__);
    }

    int blobsFound = 0;
    int sidecarsMissing = 0;
    for (std::filesystem::recursive_directory_iterator it(blobsDir), end; it != end; ++it) {
        if (!it->is_regular_file()) {
            continue;
        }
        const std::string name = it->path().filename().string();
        // The voxels blob (segmentation mask) does not get a geometry sidecar;
        // only geometry blobs do. We can distinguish by checking whether the
        // corresponding .merkle file exists for each .bin.
        if (name.size() < 4 || name.substr(name.size() - 4) != ".bin") {
            continue;
        }
        ++blobsFound;
        const std::filesystem::path merkle = it->path().string() + ".merkle";
        if (!std::filesystem::exists(merkle)) {
            ++sidecarsMissing;
        }
    }
    if (blobsFound == 0) {
        return fail("at least one blob file found", __LINE__);
    }
    // All geometry blobs (points/tris/faceId/volPoints/tets) must have sidecars.
    // The voxels blob from build_seg_mask (which build_all includes) does not get
    // a sidecar; here we save only surface + mesh so all blobs are geometry ones.
    if (sidecarsMissing > 0) {
        std::fprintf(stderr, "FAIL: %d blob(s) have no .merkle sidecar\n", sidecarsMissing);
        return 1;
    }

    std::filesystem::remove_all(temp_dir());
    return 0;
}

// A hostile path payload with a tiny sampleSpacing against a large arc length
// would drive resample into an unbounded sample loop (death-loop / OOM). The
// reader must reject such a load (ParseError) in bounded time, not hang.
int test_hostile_path_spacing_fails_load()
{
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / "hostile_path_spacing.xqproj";

    // build_path's control points span arc length 2.0; a tiny spacing here asks
    // for billions of samples. Save a valid project, then rewrite only the
    // sampleSpacing token so the payload text stays otherwise well-formed.
    xq::XQProject project;
    project.open();
    build_path(&project);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save project for hostile spacing", __LINE__);
    }
    if (!set_path_sample_spacing(path, "0.000000001")) {
        return fail("inject hostile sampleSpacing token", __LINE__);
    }

    // Must fail (resample returns non-Ok -> rebuild_path TextError -> ParseError)
    // and return promptly; if it hangs, the ctest timeout catches the regression.
    return expect_load_fails(path, false, 0, "hostile path spacing fails load");
}

} // namespace

int main()
{
    int result = test_full_payload_roundtrip();
    if (result != 0) {
        return result;
    }
    result = test_large_arrays_not_in_main_document();
    if (result != 0) {
        return result;
    }
    result = test_corrupt_blob_fails_load();
    if (result != 0) {
        return result;
    }
    result = test_blob_metadata_schema_fails_load();
    if (result != 0) {
        return result;
    }
    result = test_external_image_no_blob();
    if (result != 0) {
        return result;
    }
    result = test_geometry_sidecars_written();
    if (result != 0) {
        return result;
    }
    result = test_hostile_path_spacing_fails_load();
    if (result != 0) {
        return result;
    }
    return 0;
}
