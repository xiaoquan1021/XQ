#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "app/WorkflowCapabilities.h"
#include "app/XQWorkflowSession.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQScaleSlot.h"
#include "core/XQSimulationCasePayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "core/command/XQSceneCommands.h"
#include "io/blob/Sha256.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "services/image/DicomImportService.h"
#include "services/image/ImageResourceResolver.h"
#include "services/resource/GeometryResourceManager.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/VesselProfileController.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef XQ_SHELL_A_DICOM_ROOT
#error "XQ_SHELL_A_DICOM_ROOT must be defined"
#endif

#ifndef XQ_SHELL_A_HISTORY_PROJECT
#error "XQ_SHELL_A_HISTORY_PROJECT must be defined"
#endif

#ifndef XQ_BUILD_NINJA_PATH
#error "XQ_BUILD_NINJA_PATH must be defined"
#endif

namespace {

namespace fs = std::filesystem;

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            return fail(#condition, __LINE__);                              \
        }                                                                    \
    } while (false)

const xq::XQDataNode* firstNode(const xq::XQProject& project,
                                xq::XQDomainType domain)
{
    const xq::XQDataNode* found = nullptr;
    project.scene().visit_nodes([&found, domain](const xq::XQDataNode& node) {
        if (found == nullptr && node.domainType() == domain) {
            found = &node;
        }
    });
    return found;
}

std::size_t nodeCount(const xq::XQProject& project)
{
    std::size_t count = 0;
    project.scene().visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

bool allNodesAreOrgan(const xq::XQProject& project)
{
    bool valid = true;
    project.scene().visit_nodes([&valid](const xq::XQDataNode& node) {
        valid = valid && node.hasScaleSlot()
            && node.scaleSlot() == xq::ScaleSlot::Organ;
    });
    return valid;
}

struct TempTree {
    fs::path path;

    TempTree()
    {
        path = fs::temp_directory_path()
            / (std::string("xq-shell-a-noflow-")
               + std::to_string(
                   std::chrono::high_resolution_clock::now()
                       .time_since_epoch().count()));
    }

    ~TempTree()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

bool hashSource(const xq::IVoxelSource& source,
                std::string* digest,
                std::size_t* bytes)
{
    if (digest == nullptr || bytes == nullptr) {
        return false;
    }
    xq::VoxelLease whole = source.acquire_whole();
    if (!whole.view().valid || whole.view().bytes.empty()) {
        return false;
    }
    *bytes = whole.view().bytes.size();
    *digest = xq::Sha256::hashHex(
        whole.view().bytes.data(), whole.view().bytes.size());
    return true;
}

bool buildPath(const xq::XQImageVolume& image,
               std::vector<xq::PathControlPoint>* points)
{
    if (points == nullptr || !image.hasGeometry()) {
        return false;
    }
    const xq::ImageGeometry& geometry = image.geometry();
    int axis = 0;
    double largestExtent = -1.0;
    for (int candidate = 0; candidate < 3; ++candidate) {
        const double extent =
            static_cast<double>(geometry.dimensions[candidate] - 1)
            * std::abs(geometry.spacing[candidate]);
        if (extent > largestExtent) {
            largestExtent = extent;
            axis = candidate;
        }
    }
    if (!(std::abs(geometry.spacing[axis]) > 0.0)) {
        return false;
    }
    double startVoxel[3] = {
        (geometry.dimensions[0] - 1) * 0.5,
        (geometry.dimensions[1] - 1) * 0.5,
        (geometry.dimensions[2] - 1) * 0.5,
    };
    startVoxel[axis] = 0.0;
    double endVoxel[3] = {
        startVoxel[0], startVoxel[1], startVoxel[2]};
    endVoxel[axis] = 60.0 / std::abs(geometry.spacing[axis]);
    double startWorld[3] = {};
    double endWorld[3] = {};
    if (image.voxelToWorld(startVoxel, startWorld)
            != xq::XQImageVolume::TransformStatus::Ok
        || image.voxelToWorld(endVoxel, endWorld)
            != xq::XQImageVolume::TransformStatus::Ok) {
        return false;
    }
    points->clear();
    points->push_back({{startWorld[0], startWorld[1], startWorld[2]}});
    points->push_back({{endWorld[0], endWorld[1], endWorld[2]}});
    return true;
}

xq::XQContour makeContour(const xq::XQPath& path,
                          xq::ContourId id,
                          double arc,
                          double halfWidth)
{
    xq::PathFrame frame = {};
    path.frameAtArcLength(arc, &frame);
    xq::XQContour contour;
    contour.contourId = id;
    contour.pathArcLength = arc;
    contour.frame.origin = frame.position;
    contour.frame.normal = frame.tangent;
    contour.frame.xAxis = frame.normal;
    contour.frame.yAxis = frame.binormal;
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

int verifyExecutionSourcesExcluded()
{
    std::ifstream input(XQ_BUILD_NINJA_PATH, std::ios::binary);
    CHECK(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string rules = buffer.str();
    const char* forbidden[] = {
        "BoundaryConditionService.cpp",
        "FlowSolver1D.cpp",
        "FlowInputAssembler.cpp",
        "FlowGeometrySmokeService.cpp",
        "FlowController.cpp",
        "FlowSmokeController.cpp",
    };
    for (const char* source : forbidden) {
        CHECK(rules.find(source) == std::string::npos);
    }
    CHECK(rules.find("CenterlineBController.cpp") != std::string::npos);
    CHECK(rules.find("ItkCenterlineSkeletonizer3D.cpp") != std::string::npos);
    return 0;
}

int verifyHistoricalProject()
{
    xq::XQProjectReadResult loaded;
    CHECK(xq::XQProjectReader::load(XQ_SHELL_A_HISTORY_PROJECT, &loaded)
          == xq::XQProjectReader::Status::Ok);
    CHECK(nodeCount(loaded.project) == 5);
    CHECK(allNodesAreOrgan(loaded.project));

    const xq::XQDataNode* profileNode =
        firstNode(loaded.project, xq::XQDomainType::VesselProfile);
    const xq::XQDataNode* caseNode =
        firstNode(loaded.project, xq::XQDomainType::SimulationCase);
    const xq::XQDataNode* resultNode =
        firstNode(loaded.project, xq::XQDomainType::FlowResult);
    CHECK(profileNode != nullptr && caseNode != nullptr && resultNode != nullptr);
    const auto profile = std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(
        profileNode->payload());
    const auto simulationCase = std::dynamic_pointer_cast<
        xq::XQSimulationCasePayload>(caseNode->payload());
    const auto flow = std::dynamic_pointer_cast<xq::XQFlowResultPayload>(
        resultNode->payload());
    CHECK(profile != nullptr && simulationCase != nullptr && flow != nullptr);
    CHECK(xq::VesselProfileValidator::validate(profile->profile()).ok());
    CHECK(simulationCase->simulationCase().hasFlowSmokeProvenance());
    CHECK(flow->result().hasFlowSmokeProvenance());
    CHECK(flow->result().flowSmokeProvenance().protocol.id
          == "engineering-smoke-v1");
    CHECK(flow->result().isConsistent());
    CHECK(flow->result().converged());
    CHECK(flow->result().segments().size() == 10);
    CHECK(flow->result().times().size() == 200);

    xq::XQCommandStack stack;
    xq::XQWorkflowSession session;
    session.attach(&loaded.project, &stack);
    CHECK(!xq::WorkflowCapabilities::flowCompiledIn());
    CHECK(!session.hasFlowCapability());
    CHECK(session.flowController() == nullptr);
    CHECK(session.flowSmokeController() == nullptr);
    CHECK(session.pathController() != nullptr);
    CHECK(session.vesselProfileController() != nullptr);
    CHECK(session.centerlineBController() != nullptr);
    CHECK(session.aiController() != nullptr);

    TempTree temp;
    std::error_code error;
    CHECK(fs::create_directories(temp.path, error) && !error);
    const fs::path resaved = temp.path / "history-resaved.xqproj";
    CHECK(xq::XQProjectWriter::save(loaded.project, resaved.string())
          == xq::XQProjectWriter::Status::Ok);
    xq::XQProjectReadResult reopened;
    CHECK(xq::XQProjectReader::load(resaved.string(), &reopened)
          == xq::XQProjectReader::Status::Ok);
    const xq::XQDataNode* reopenedResultNode =
        firstNode(reopened.project, xq::XQDomainType::FlowResult);
    const auto reopenedFlow = reopenedResultNode == nullptr
        ? std::shared_ptr<xq::XQFlowResultPayload>()
        : std::dynamic_pointer_cast<xq::XQFlowResultPayload>(
            reopenedResultNode->payload());
    CHECK(reopenedFlow != nullptr);
    CHECK(reopenedFlow->result().isConsistent());
    CHECK(reopenedFlow->result().times() == flow->result().times());
    CHECK(reopenedFlow->result().flowQ() == flow->result().flowQ());
    CHECK(reopenedFlow->result().pressureP() == flow->result().pressureP());
    CHECK(reopenedFlow->result().areaA() == flow->result().areaA());
    return 0;
}

int verifyDicomProfileWithoutFlow()
{
    const fs::path source =
        fs::path(XQ_SHELL_A_DICOM_ROOT) / "regular-oblique";
    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery = reader.discover(source.string());
    CHECK(discovery.ok() && discovery.series.size() == 1);

    TempTree temp;
    std::error_code error;
    CHECK(fs::create_directories(temp.path, error) && !error);
    const fs::path projectPath = temp.path / "noflow-profile.xqproj";
    const xq::NodeId imageId(66001);
    const xq::NodeId pathId(66002);
    const xq::NodeId contourId(66003);
    const xq::NodeId profileId(66004);
    const xq::AssetId imageAsset(76001);
    std::string expectedDigest;
    std::size_t expectedBytes = 0;

    {
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
        xq::DicomImportRequest request;
        request.sourceDirectory = source.string();
        request.seriesInstanceUid =
            discovery.series.front().identity.seriesInstanceUid;
        request.nodeId = imageId;
        request.assetId = imageAsset;
        xq::DicomImportResult prepared =
            xq::DicomImportService::prepare(reader, request);
        CHECK(prepared.ok());
        CHECK(hashSource(
            *prepared.residentSource, &expectedDigest, &expectedBytes));
        CHECK(stack.push(std::make_unique<xq::ProjectNodeBatchCommand>(
            &project, std::move(prepared.batchSpec.value()),
            "Import no-Flow DICOM")));

        const xq::XQDataNode* imageNode = project.scene().find(imageId);
        const auto imagePayload = imageNode == nullptr
            ? std::shared_ptr<xq::XQImageVolumePayload>()
            : std::dynamic_pointer_cast<xq::XQImageVolumePayload>(
                imageNode->payload());
        CHECK(imagePayload != nullptr);
        std::vector<xq::PathControlPoint> controlPoints;
        CHECK(buildPath(imagePayload->volume(), &controlPoints));
        xq::PathController pathController(&project.scene(), &stack);
        xq::PathController::AddPathIntent pathIntent;
        pathIntent.newPathId = pathId;
        pathIntent.name = "No-Flow path";
        pathIntent.sourceImageNode = imageId;
        pathIntent.controlPoints = controlPoints;
        pathIntent.spacing = 1.0;
        CHECK(pathController.addPath(pathIntent)
              == xq::PathController::Status::Ok);
        const xq::XQDataNode* pathNode = project.scene().find(pathId);
        const auto pathPayload = pathNode == nullptr
            ? std::shared_ptr<xq::XQPathPayload>()
            : std::dynamic_pointer_cast<xq::XQPathPayload>(pathNode->payload());
        CHECK(pathPayload != nullptr);
        CHECK(pathNode->scaleSlot() == xq::ScaleSlot::Organ);

        xq::XQContourGroup group;
        group.setId(contourId);
        group.setSourcePathNode(pathId);
        group.addContour(makeContour(
            pathPayload->path(), xq::ContourId(67001), 5.0, 5.0));
        group.addContour(makeContour(
            pathPayload->path(), xq::ContourId(67002), 30.0, 6.0));
        group.addContour(makeContour(
            pathPayload->path(), xq::ContourId(67003), 55.0, 7.0));
        xq::XQDataNode contourNode(
            contourId, xq::XQDomainType::ContourGroup,
            "No-Flow contours",
            std::make_shared<xq::XQContourGroupPayload>(std::move(group)));
        contourNode.setScaleSlot(xq::ScaleSlot::Organ);
        CHECK(stack.push(
            std::make_unique<xq::AddNodeWithSourceRelationCommand>(
                &project.scene(), std::move(contourNode), pathId)));

        xq::VesselProfileController profileController(&project, &stack);
        xq::VesselProfileController::ContourIntent intent;
        intent.output.newProfileId = profileId;
        intent.output.name = "No-Flow VesselProfile";
        intent.pathNode = pathId;
        intent.contourGroupNode = contourId;
        intent.frameOfReferenceId =
            imagePayload->volume().dicomIdentity().frameOfReferenceUid;
        CHECK(profileController.assembleFromContours(intent)
              == xq::VesselProfileController::Status::Ok);
        CHECK(project.scene().find(profileId)->scaleSlot()
              == xq::ScaleSlot::Organ);
        CHECK(xq::XQProjectWriter::save(project, projectPath.string())
              == xq::XQProjectWriter::Status::Ok);
    }

    xq::XQProjectReadResult reopened;
    CHECK(xq::XQProjectReader::load(projectPath.string(), &reopened)
          == xq::XQProjectReader::Status::Ok);
    CHECK(reopened.project.scene().find(imageId) != nullptr);
    CHECK(reopened.project.scene().find(pathId) != nullptr);
    CHECK(reopened.project.scene().find(contourId) != nullptr);
    CHECK(reopened.project.scene().find(profileId) != nullptr);
    CHECK(reopened.project.scene().find(profileId)->scaleSlot()
          == xq::ScaleSlot::Organ);
    const auto imagePayload = std::dynamic_pointer_cast<xq::XQImageVolumePayload>(
        reopened.project.scene().find(imageId)->payload());
    CHECK(imagePayload != nullptr);
    xq::GeometryResourceManager manager(
        &reopened.project.assetRegistry(), temp.path.string());
    xq::GdcmItkDicomSeriesReader lazyReader;
    const xq::ImageResourceResolveResult resolved =
        xq::ImageResourceResolver::acquire(
            imageAsset, imagePayload->volume(), projectPath.string(), manager,
            reopened.project.assetRegistry(), lazyReader);
    CHECK(resolved.ok());
    std::string actualDigest;
    std::size_t actualBytes = 0;
    CHECK(hashSource(resolved.source.source(), &actualDigest, &actualBytes));
    CHECK(actualBytes == expectedBytes);
    CHECK(actualDigest == expectedDigest);
    return 0;
}

} // namespace

int main()
{
    int result = verifyExecutionSourcesExcluded();
    if (result != 0) {
        return result;
    }
    result = verifyHistoricalProject();
    if (result != 0) {
        return result;
    }
    result = verifyDicomProfileWithoutFlow();
    if (result != 0) {
        return result;
    }
    std::printf(
        "OK: Flow-OFF excludes execution, preserves production history, "
        "and keeps DICOM -> Path -> ContourGroup -> VesselProfile available\n");
    return 0;
}
