#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQSimulationCasePayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/asset/AssetRecord.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "core/command/XQSceneCommands.h"
#include "io/blob/Sha256.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "services/flow/FlowInputAssembler.h"
#include "services/flow/FlowSolver1D.h"
#include "services/image/DicomImportService.h"
#include "services/image/DicomServiceSupport.h"
#include "services/image/ImageResourceResolver.h"
#include "services/path/PathService.h"
#include "services/resource/GeometryResourceManager.h"
#include "ui/controllers/FlowSmokeController.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/VesselProfileController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef XQ_SHELL_A_DICOM_ROOT
#error "XQ_SHELL_A_DICOM_ROOT must be defined"
#endif

namespace {

namespace fs = std::filesystem;

constexpr double kTolerance = 1.0e-8;

struct ShellIds {
    xq::NodeId image{61001};
    xq::NodeId path{61002};
    xq::NodeId contour{61003};
    xq::NodeId profile{61004};
    xq::NodeId simulationCase{61005};
    xq::NodeId flowResult{61006};

    xq::AssetId imageAsset{71001};
    xq::AssetId profileAsset{71004};
    xq::AssetId caseAsset{71005};
    xq::AssetId resultAsset{71006};
};

const ShellIds kIds;

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

bool closeEnough(double left, double right)
{
    const double scale =
        (std::max)(1.0, (std::max)(std::abs(left), std::abs(right)));
    return std::abs(left - right) <= kTolerance * scale;
}

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t sceneRelationCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

bool hasSceneRelation(const xq::XQScene& scene,
                      const xq::NodeId& source,
                      const xq::NodeId& derived)
{
    bool found = false;
    scene.visit_derived_relations(
        [&found, &source, &derived](const xq::NodeId& currentSource,
                                    const xq::NodeId& currentDerived) {
            found = found
                || (currentSource == source && currentDerived == derived);
        });
    return found;
}

struct ProjectCounts {
    std::size_t nodes = 0;
    std::size_t sceneRelations = 0;
    std::size_t assets = 0;
    std::size_t assetRelations = 0;
    std::size_t undo = 0;
    std::size_t redo = 0;
};

ProjectCounts counts(const xq::XQProject& project,
                     const xq::XQCommandStack& stack)
{
    ProjectCounts value;
    value.nodes = nodeCount(project.scene());
    value.sceneRelations = sceneRelationCount(project.scene());
    value.assets = project.assetRegistry().assetCount();
    value.assetRelations = project.assetRegistry().relationCount();
    value.undo = stack.undo_count();
    value.redo = stack.redo_count();
    return value;
}

bool sameCounts(const ProjectCounts& left, const ProjectCounts& right)
{
    return left.nodes == right.nodes
        && left.sceneRelations == right.sceneRelations
        && left.assets == right.assets
        && left.assetRelations == right.assetRelations
        && left.undo == right.undo
        && left.redo == right.redo;
}

struct TempTree {
    fs::path path;

    explicit TempTree(const char* label)
    {
        path = fs::temp_directory_path()
            / (std::string(label) + "-"
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

std::string relativeSourcePath(const fs::path& source,
                               const fs::path& projectFile)
{
    std::error_code error;
    const fs::path relative = fs::relative(
        source, projectFile.parent_path(), error);
    if (error || relative.empty() || relative.is_absolute()
        || relative.has_root_name() || relative.has_root_directory()) {
        return std::string();
    }
    return relative.generic_string();
}

xq::AssetRecord derivedAsset(const xq::AssetId& id,
                             xq::AssetKind kind,
                             const char* fingerprint)
{
    xq::AssetRecord asset;
    asset.id = id;
    asset.category = xq::AssetCategory::Derived;
    asset.kind = kind;
    asset.contentFingerprint = fingerprint;
    asset.displayName = "Shell A E2E";
    return asset;
}

const xq::XQImageVolumePayload* imagePayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.image);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQImageVolumePayload*>(node->payload().get());
}

const xq::XQPathPayload* pathPayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.path);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQPathPayload*>(node->payload().get());
}

const xq::XQContourGroupPayload* contourPayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.contour);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQContourGroupPayload*>(node->payload().get());
}

const xq::XQVesselProfilePayload* profilePayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.profile);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQVesselProfilePayload*>(node->payload().get());
}

const xq::XQSimulationCasePayload* casePayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.simulationCase);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQSimulationCasePayload*>(node->payload().get());
}

const xq::XQFlowResultPayload* resultPayload(const xq::XQProject& project)
{
    const xq::XQDataNode* node = project.scene().find(kIds.flowResult);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQFlowResultPayload*>(node->payload().get());
}

bool hashVoxels(const xq::IVoxelSource& source,
                std::string* digest,
                std::size_t* byteCount)
{
    if (digest == nullptr || byteCount == nullptr) {
        return false;
    }
    xq::VoxelLease lease = source.acquire_whole();
    if (!lease.view().valid || lease.view().bytes.empty()) {
        return false;
    }
    *byteCount = lease.view().bytes.size();
    *digest = xq::Sha256::hashHex(
        lease.view().bytes.data(), lease.view().bytes.size());
    return !digest->empty();
}

bool selectSingleSeries(const fs::path& root,
                        xq::DicomSeriesDescriptor* descriptor)
{
    if (descriptor == nullptr) {
        return false;
    }
    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery =
        reader.discover(root.string());
    if (!discovery.ok() || discovery.series.size() != 1
        || discovery.series.front().identity.seriesInstanceUid.empty()
        || discovery.series.front().sliceCount < 2) {
        return false;
    }
    *descriptor = discovery.series.front();
    return true;
}

bool buildSixtyMillimeterPath(const xq::XQImageVolume& image,
                              bool requireInsideVolume,
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
    if (!(std::abs(geometry.spacing[axis]) > 0.0)
        || (requireInsideVolume && largestExtent < 60.0)) {
        return false;
    }

    double startVoxel[3] = {
        (geometry.dimensions[0] - 1) * 0.5,
        (geometry.dimensions[1] - 1) * 0.5,
        (geometry.dimensions[2] - 1) * 0.5,
    };
    double endVoxel[3] = {
        startVoxel[0], startVoxel[1], startVoxel[2]};
    const double delta = 60.0 / std::abs(geometry.spacing[axis]);
    if (largestExtent >= 60.0) {
        startVoxel[axis] =
            (static_cast<double>(geometry.dimensions[axis] - 1) - delta) * 0.5;
    } else {
        // The committed CI fixture is deliberately tiny. It still proves the
        // patient-LPS transform and full production chain; the optional
        // authorized real-data target below additionally requires the complete
        // 60 mm segment to lie inside the decoded volume.
        startVoxel[axis] = 0.0;
    }
    endVoxel[axis] = startVoxel[axis] + delta;

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
    return closeEnough(
        xq::distance(points->front().position, points->back().position), 60.0);
}

xq::XQContour makeContour(const xq::XQPath& path,
                          xq::ContourId id,
                          double arcLength,
                          double halfWidth)
{
    xq::PathFrame pathFrame = {};
    path.frameAtArcLength(arcLength, &pathFrame);
    xq::XQContour contour;
    contour.contourId = id;
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

xq::XQContourGroup makeContourGroup(const xq::XQPath& path, bool valid)
{
    xq::XQContourGroup group;
    group.setId(kIds.contour);
    group.setSourcePathNode(kIds.path);
    xq::XQContour first =
        makeContour(path, xq::ContourId(62001), 5.0, 5.0);
    if (!valid) {
        first.closed = false;
    }
    group.addContour(first);
    group.addContour(makeContour(path, xq::ContourId(62002), 30.0, 6.0));
    group.addContour(makeContour(path, xq::ContourId(62003), 55.0, 7.0));
    return group;
}

bool sameInputStamp(const xq::DerivationInputStamp& left,
                    const xq::DerivationInputStamp& right)
{
    return left.nodeId == right.nodeId
        && left.contentRevision == right.contentRevision
        && left.assetId == right.assetId
        && left.assetFingerprint == right.assetFingerprint;
}

bool sameProfile(const xq::VesselProfileV1& left,
                 const xq::VesselProfileV1& right)
{
    if (left.contractVersion != right.contractVersion
        || left.coordinateSystem != right.coordinateSystem
        || left.lengthUnit != right.lengthUnit
        || left.areaUnit != right.areaUnit
        || left.frameOfReferenceId != right.frameOfReferenceId
        || left.sourcePathNode != right.sourcePathNode
        || left.sourceEvidenceNodes != right.sourceEvidenceNodes
        || left.externalEvidenceId != right.externalEvidenceId
        || left.externalEvidenceFingerprint != right.externalEvidenceFingerprint
        || left.derivationStamp.algorithmId != right.derivationStamp.algorithmId
        || left.derivationStamp.algorithmVersion
            != right.derivationStamp.algorithmVersion
        || left.derivationStamp.parameterSummary
            != right.derivationStamp.parameterSummary
        || left.derivationStamp.randomSeed != right.derivationStamp.randomSeed
        || left.derivationStamp.inputs.size()
            != right.derivationStamp.inputs.size()
        || left.samples.size() != right.samples.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.derivationStamp.inputs.size(); ++i) {
        if (!sameInputStamp(
                left.derivationStamp.inputs[i], right.derivationStamp.inputs[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < left.samples.size(); ++i) {
        const xq::VesselProfileSample& a = left.samples[i];
        const xq::VesselProfileSample& b = right.samples[i];
        if (a.sampleId != b.sampleId
            || !closeEnough(a.arcLengthMm, b.arcLengthMm)
            || !closeEnough(a.positionMm.x, b.positionMm.x)
            || !closeEnough(a.positionMm.y, b.positionMm.y)
            || !closeEnough(a.positionMm.z, b.positionMm.z)
            || !closeEnough(a.unitTangent.x, b.unitTangent.x)
            || !closeEnough(a.unitTangent.y, b.unitTangent.y)
            || !closeEnough(a.unitTangent.z, b.unitTangent.z)
            || !closeEnough(a.areaMm2, b.areaMm2)
            || a.evidenceKind != b.evidenceKind
            || a.quality != b.quality
            || a.sourceEvidenceNode != b.sourceEvidenceNode) {
            return false;
        }
    }
    return true;
}

bool sameMatrix(const std::vector<std::vector<double>>& left,
                const std::vector<std::vector<double>>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t row = 0; row < left.size(); ++row) {
        if (left[row].size() != right[row].size()) {
            return false;
        }
        for (std::size_t column = 0; column < left[row].size(); ++column) {
            if (!closeEnough(left[row][column], right[row][column])) {
                return false;
            }
        }
    }
    return true;
}

bool sameFlowResult(const xq::XQFlowResult& left,
                    const xq::XQFlowResult& right)
{
    if (left.hasSourceCaseNode() != right.hasSourceCaseNode()
        || (left.hasSourceCaseNode()
            && left.sourceCaseNode() != right.sourceCaseNode())
        || left.times().size() != right.times().size()
        || left.segments().size() != right.segments().size()
        || left.converged() != right.converged()
        || !closeEnough(left.maxCfl(), right.maxCfl())
        || left.hasFlowSmokeProvenance() != right.hasFlowSmokeProvenance()) {
        return false;
    }
    for (std::size_t i = 0; i < left.times().size(); ++i) {
        if (!closeEnough(left.times()[i], right.times()[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < left.segments().size(); ++i) {
        const xq::FlowSegment& a = left.segments()[i];
        const xq::FlowSegment& b = right.segments()[i];
        if (a.segmentId != b.segmentId
            || !closeEnough(a.arcLengthStart, b.arcLengthStart)
            || !closeEnough(a.arcLengthEnd, b.arcLengthEnd)
            || a.faceId != b.faceId) {
            return false;
        }
    }
    if (!sameMatrix(left.flowQ(), right.flowQ())
        || !sameMatrix(left.pressureP(), right.pressureP())
        || !sameMatrix(left.areaA(), right.areaA())) {
        return false;
    }
    if (left.hasFlowSmokeProvenance()) {
        const xq::FlowSmokeResultProvenance& a =
            left.flowSmokeProvenance();
        const xq::FlowSmokeResultProvenance& b =
            right.flowSmokeProvenance();
        if (a.protocol.id != b.protocol.id
            || a.protocol.version != b.protocol.version
            || a.protocol.label != b.protocol.label
            || a.protocol.stationCount != b.protocol.stationCount
            || !closeEnough(a.protocol.dtSeconds, b.protocol.dtSeconds)
            || a.protocol.numTimeSteps != b.protocol.numTimeSteps
            || a.protocol.numCycles != b.protocol.numCycles
            || a.solverId != b.solverId
            || a.solverVersion != b.solverVersion
            || a.sourceVesselProfileNode != b.sourceVesselProfileNode
            || a.sourceVesselProfileRevision
                != b.sourceVesselProfileRevision) {
            return false;
        }
    }
    return true;
}

bool verifyScaleSlots(const xq::XQProject& project)
{
    const xq::NodeId ids[] = {
        kIds.image, kIds.path, kIds.contour, kIds.profile,
        kIds.simulationCase, kIds.flowResult};
    for (const xq::NodeId& id : ids) {
        const xq::XQDataNode* node = project.scene().find(id);
        if (node == nullptr || !node->hasScaleSlot()
            || node->scaleSlot() != xq::ScaleSlot::Organ) {
            return false;
        }
    }
    return true;
}

bool verifySceneLineage(const xq::XQProject& project)
{
    return hasSceneRelation(project.scene(), kIds.image, kIds.path)
        && hasSceneRelation(project.scene(), kIds.path, kIds.contour)
        && hasSceneRelation(project.scene(), kIds.path, kIds.profile)
        && hasSceneRelation(project.scene(), kIds.contour, kIds.profile)
        && hasSceneRelation(
            project.scene(), kIds.profile, kIds.simulationCase)
        && hasSceneRelation(project.scene(), kIds.profile, kIds.flowResult)
        && hasSceneRelation(
            project.scene(), kIds.simulationCase, kIds.flowResult);
}

int verifySemanticStaleAndUndo(xq::XQProject* project,
                               xq::XQCommandStack* stack)
{
    CHECK(project != nullptr && stack != nullptr);
    const xq::XQDataNode* pathNode = project->scene().find(kIds.path);
    const xq::XQPathPayload* originalPath = pathPayload(*project);
    CHECK(pathNode != nullptr && originalPath != nullptr);
    const xq::ContentRevision pathRevision = pathNode->contentRevision();
    std::vector<xq::PathControlPoint> controls =
        originalPath->path().controlPoints();
    CHECK(controls.size() >= 2);
    controls.back().position.x += 0.5;
    xq::PathService::Result pathEdit =
        xq::PathService::moveControlPointCommand(
            &project->scene(), *pathNode, controls.size() - 1,
            controls.back().position, originalPath->path().sampleSpacing());
    CHECK(pathEdit.ok() && pathEdit.command != nullptr);
    CHECK(stack->push(std::move(pathEdit.command)));
    CHECK(project->scene().find(kIds.path)->contentRevision()
          == pathRevision + 1);
    CHECK(project->scene().is_stale(kIds.contour));
    CHECK(project->scene().is_stale(kIds.profile));
    CHECK(project->scene().is_stale(kIds.simulationCase));
    CHECK(project->scene().is_stale(kIds.flowResult));
    CHECK(stack->undo());
    CHECK(project->scene().find(kIds.path)->contentRevision() == pathRevision);
    CHECK(!project->scene().is_stale(kIds.contour));
    CHECK(!project->scene().is_stale(kIds.profile));
    CHECK(!project->scene().is_stale(kIds.simulationCase));
    CHECK(!project->scene().is_stale(kIds.flowResult));

    const xq::XQDataNode* contourNode = project->scene().find(kIds.contour);
    const xq::XQContourGroupPayload* originalContour = contourPayload(*project);
    CHECK(contourNode != nullptr && originalContour != nullptr);
    const xq::ContentRevision contourRevision = contourNode->contentRevision();
    xq::XQContourGroup edited = originalContour->group();
    std::vector<xq::XQContour> contours = edited.contours();
    CHECK(contours.size() == 3);
    contours[1].points[0] = xq::add(
        contours[1].points[0], xq::scale(contours[1].frame.xAxis, 0.25));
    xq::XQContourGroup rebuilt;
    rebuilt.setId(edited.id());
    rebuilt.setSourcePathNode(edited.sourcePathNode());
    for (const xq::XQContour& contour : contours) {
        rebuilt.addContour(contour);
    }
    CHECK(stack->push(std::make_unique<xq::SemanticReplacePayloadCommand>(
        &project->scene(), kIds.contour, xq::XQDomainType::ContourGroup,
        std::make_shared<xq::XQContourGroupPayload>(std::move(rebuilt)),
        "Edit Shell A contour evidence")));
    CHECK(project->scene().find(kIds.contour)->contentRevision()
          == contourRevision + 1);
    CHECK(project->scene().is_stale(kIds.profile));
    CHECK(project->scene().is_stale(kIds.simulationCase));
    CHECK(project->scene().is_stale(kIds.flowResult));
    CHECK(stack->undo());
    CHECK(project->scene().find(kIds.contour)->contentRevision()
          == contourRevision);
    CHECK(!project->scene().is_stale(kIds.profile));
    CHECK(!project->scene().is_stale(kIds.simulationCase));
    CHECK(!project->scene().is_stale(kIds.flowResult));
    return 0;
}

struct SuccessSnapshot {
    xq::XQImageVolume image;
    xq::XQPath path;
    xq::XQContourGroup contours;
    xq::VesselProfileV1 profile;
    xq::XQSimulationCase simulationCase;
    xq::XQFlowResult flow;
    xq::DicomSeriesIdentity dicom;
    xq::ImageGeometry geometry{};
    std::string fingerprint;
    std::string voxelDigest;
    std::size_t voxelBytes = 0;
};

int runSuccessChain(const fs::path& sourceDirectory,
                    bool requirePathInsideVolume,
                    bool runStaleChecks,
                    SuccessSnapshot* snapshotOut)
{
    CHECK(snapshotOut != nullptr);
    TempTree temp("xq-shell-a-e2e");
    std::error_code error;
    CHECK(fs::create_directories(temp.path, error) && !error);
    const fs::path projectPath = temp.path / "shell-a.xqproj";

    xq::DicomSeriesDescriptor descriptor;
    CHECK(selectSingleSeries(sourceDirectory, &descriptor));

    SuccessSnapshot expected;
    {
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);

        xq::GdcmItkDicomSeriesReader reader;
        xq::DicomImportRequest request;
        request.sourceDirectory = sourceDirectory.string();
        request.sourceRelPath = relativeSourcePath(sourceDirectory, projectPath);
        request.seriesInstanceUid = descriptor.identity.seriesInstanceUid;
        request.nodeId = kIds.image;
        request.assetId = kIds.imageAsset;

        xq::DicomImportResult prepared =
            xq::DicomImportService::prepare(reader, request);
        CHECK(prepared.ok());
        CHECK(nodeCount(project.scene()) == 0);
        CHECK(project.assetRegistry().assetCount() == 0);

        const auto preparedImage = std::dynamic_pointer_cast<
            xq::XQImageVolumePayload>(prepared.batchSpec->node.payload());
        CHECK(preparedImage != nullptr);
        expected.image = preparedImage->volume();
        expected.dicom = preparedImage->volume().dicomIdentity();
        expected.geometry = preparedImage->volume().geometry();
        expected.fingerprint =
            prepared.batchSpec->assetToRegister->contentFingerprint;
        CHECK(hashVoxels(
            *prepared.residentSource, &expected.voxelDigest,
            &expected.voxelBytes));

        xq::GeometryResourceManager manager(
            &project.assetRegistry(), temp.path.string());
        CHECK(!manager.installResidentVoxelSource(
            kIds.imageAsset, prepared.residentSource).valid());
        CHECK(stack.push(std::make_unique<xq::ProjectNodeBatchCommand>(
            &project, std::move(prepared.batchSpec.value()),
            "Import Shell A DICOM")));
        CHECK(manager.installResidentVoxelSource(
            kIds.imageAsset, prepared.residentSource).valid());
        prepared.residentSource.reset();

        std::vector<xq::PathControlPoint> controlPoints;
        CHECK(buildSixtyMillimeterPath(
            expected.image, requirePathInsideVolume, &controlPoints));
        xq::PathController pathController(&project.scene(), &stack);
        xq::PathController::AddPathIntent pathIntent;
        pathIntent.newPathId = kIds.path;
        pathIntent.name = "Shell A measured path";
        pathIntent.sourceImageNode = kIds.image;
        pathIntent.controlPoints = controlPoints;
        pathIntent.spacing = 1.0;
        CHECK(pathController.addPath(pathIntent)
              == xq::PathController::Status::Ok);

        const xq::XQPathPayload* path = pathPayload(project);
        CHECK(path != nullptr);
        xq::XQDataNode contourNode(
            kIds.contour, xq::XQDomainType::ContourGroup,
            "Shell A measured contours",
            std::make_shared<xq::XQContourGroupPayload>(
                makeContourGroup(path->path(), true)));
        contourNode.setScaleSlot(xq::ScaleSlot::Organ);
        CHECK(stack.push(
            std::make_unique<xq::AddNodeWithSourceRelationCommand>(
                &project.scene(), std::move(contourNode), kIds.path,
                "Add Shell A contour evidence")));

        xq::VesselProfileController profileController(&project, &stack);
        xq::VesselProfileController::ContourIntent profileIntent;
        profileIntent.output.newProfileId = kIds.profile;
        profileIntent.output.name = "Shell A VesselProfile";
        profileIntent.output.newProfileAsset = derivedAsset(
            kIds.profileAsset, xq::AssetKind::VesselProfile,
            "sha256:shell-a-profile-v1");
        profileIntent.pathNode = kIds.path;
        profileIntent.contourGroupNode = kIds.contour;
        profileIntent.frameOfReferenceId =
            expected.dicom.frameOfReferenceUid;
        CHECK(profileController.assembleFromContours(profileIntent)
              == xq::VesselProfileController::Status::Ok);

        xq::FlowSmokeController flowController(&project, &stack);
        xq::FlowSmokeController::SmokeIntent smokeIntent;
        smokeIntent.sourceProfileNode = kIds.profile;
        smokeIntent.output.caseNode = kIds.simulationCase;
        smokeIntent.output.caseName = "L0 geometry smoke case";
        smokeIntent.output.resultNode = kIds.flowResult;
        smokeIntent.output.resultName = "L0 geometry smoke result";
        smokeIntent.output.caseAsset = derivedAsset(
            kIds.caseAsset, xq::AssetKind::SimulationCase,
            "sha256:shell-a-smoke-case-v1");
        smokeIntent.output.resultAsset = derivedAsset(
            kIds.resultAsset, xq::AssetKind::FlowResult,
            "sha256:shell-a-smoke-result-v1");
        CHECK(flowController.run(smokeIntent)
              == xq::FlowSmokeController::Status::Ok);

        CHECK(nodeCount(project.scene()) == 6);
        CHECK(sceneRelationCount(project.scene()) == 7);
        CHECK(verifyScaleSlots(project));
        CHECK(verifySceneLineage(project));
        CHECK(project.assetRegistry().hasRelation(
            kIds.profileAsset, kIds.caseAsset));
        CHECK(project.assetRegistry().hasRelation(
            kIds.profileAsset, kIds.resultAsset));
        CHECK(project.assetRegistry().hasRelation(
            kIds.caseAsset, kIds.resultAsset));

        const xq::XQVesselProfilePayload* profile = profilePayload(project);
        const xq::XQSimulationCasePayload* simulationCase = casePayload(project);
        const xq::XQFlowResultPayload* result = resultPayload(project);
        CHECK(profile != nullptr && simulationCase != nullptr && result != nullptr);
        CHECK(xq::VesselProfileValidator::validate(profile->profile()).ok());
        CHECK(profile->profile().frameOfReferenceId
              == expected.dicom.frameOfReferenceUid);
        CHECK(profile->profile().samples.size() == 3);
        CHECK(closeEnough(profile->profile().samples[0].arcLengthMm, 5.0));
        CHECK(closeEnough(profile->profile().samples[1].arcLengthMm, 30.0));
        CHECK(closeEnough(profile->profile().samples[2].arcLengthMm, 55.0));
        CHECK(closeEnough(profile->profile().samples[0].areaMm2, 100.0));
        CHECK(closeEnough(profile->profile().samples[1].areaMm2, 144.0));
        CHECK(closeEnough(profile->profile().samples[2].areaMm2, 196.0));
        CHECK(simulationCase->simulationCase().hasFlowSmokeProvenance());
        CHECK(result->result().hasFlowSmokeProvenance());
        CHECK(result->result().isConsistent());
        CHECK(result->result().converged());
        expected.profile = profile->profile();
        expected.path = path->path();
        expected.contours = contourPayload(project)->group();
        expected.simulationCase = simulationCase->simulationCase();
        expected.flow = result->result();

        if (runStaleChecks) {
            const int staleResult = verifySemanticStaleAndUndo(&project, &stack);
            if (staleResult != 0) {
                return staleResult;
            }
        }

        CHECK(xq::XQProjectWriter::save(project, projectPath.string())
              == xq::XQProjectWriter::Status::Ok);
    }

    xq::XQProjectReadResult reopened;
    CHECK(xq::XQProjectReader::load(projectPath.string(), &reopened)
          == xq::XQProjectReader::Status::Ok);
    CHECK(nodeCount(reopened.project.scene()) == 6);
    CHECK(sceneRelationCount(reopened.project.scene()) == 7);
    CHECK(verifyScaleSlots(reopened.project));
    CHECK(verifySceneLineage(reopened.project));

    const xq::XQImageVolumePayload* reopenedImage = imagePayload(reopened.project);
    const xq::XQVesselProfilePayload* reopenedProfile =
        profilePayload(reopened.project);
    const xq::XQSimulationCasePayload* reopenedCase = casePayload(reopened.project);
    const xq::XQFlowResultPayload* reopenedResult = resultPayload(reopened.project);
    const xq::AssetRecord* reopenedImageAsset =
        reopened.project.assetRegistry().find(kIds.imageAsset);
    CHECK(reopenedImage != nullptr && reopenedProfile != nullptr
          && reopenedCase != nullptr && reopenedResult != nullptr
          && reopenedImageAsset != nullptr);
    CHECK(reopenedImage->volume().bufferHandle() == nullptr);
    CHECK(xq::sameImageGeometry(
        reopenedImage->volume().geometry(), expected.geometry));
    CHECK(xq::sameDicomSeriesIdentity(
        reopenedImage->volume().dicomIdentity(), expected.dicom));
    CHECK(reopenedImageAsset->contentFingerprint == expected.fingerprint);
    CHECK(sameProfile(reopenedProfile->profile(), expected.profile));
    CHECK(sameFlowResult(reopenedResult->result(), expected.flow));
    CHECK(reopenedCase->simulationCase().flowSmokeProvenance().stationMap.size()
          == 11);

    xq::GeometryResourceManager reopenedManager(
        &reopened.project.assetRegistry(),
        (projectPath.parent_path()
         / (projectPath.stem().string() + ".assets")).string());
    xq::GdcmItkDicomSeriesReader reopenedReader;
    const xq::ImageResourceResolveResult resolved =
        xq::ImageResourceResolver::acquire(
            kIds.imageAsset, reopenedImage->volume(), projectPath.string(),
            reopenedManager, reopened.project.assetRegistry(), reopenedReader);
    CHECK(resolved.ok());
    std::string reopenedDigest;
    std::size_t reopenedBytes = 0;
    CHECK(hashVoxels(resolved.source.source(), &reopenedDigest, &reopenedBytes));
    CHECK(reopenedBytes == expected.voxelBytes);
    CHECK(reopenedDigest == expected.voxelDigest);

    *snapshotOut = std::move(expected);
    return 0;
}

int testBadDicomAndExplicitSeriesSelection(const fs::path& fixtureRoot)
{
    xq::XQProject project;
    xq::XQCommandStack stack;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    const ProjectCounts before = counts(project, stack);

    xq::GdcmItkDicomSeriesReader reader;
    xq::DicomImportRequest badRequest;
    badRequest.sourceDirectory = (fixtureRoot / "invalid").string();
    badRequest.seriesInstanceUid = "1.2.826.0.1.invalid";
    badRequest.nodeId = xq::NodeId(63001);
    badRequest.assetId = xq::AssetId(73001);
    const xq::DicomImportResult bad =
        xq::DicomImportService::prepare(reader, badRequest);
    CHECK(!bad.ok());
    CHECK(!bad.batchSpec.has_value());
    CHECK(bad.residentSource == nullptr);
    CHECK(sameCounts(before, counts(project, stack)));

    const fs::path multiSeries = fixtureRoot / "multi-series";
    const xq::DicomSeriesDiscoveryResult discovery =
        reader.discover(multiSeries.string());
    CHECK(discovery.ok());
    CHECK(discovery.series.size() >= 2);
    const xq::DicomSeriesReadResult unselected =
        reader.read(multiSeries.string(), std::string());
    CHECK(unselected.status == xq::DicomSeriesStatus::AmbiguousSeries);
    CHECK(!unselected.ok());
    CHECK(sameCounts(before, counts(project, stack)));
    return 0;
}

int testMissingExternalSource(const fs::path& fixtureRoot)
{
    TempTree temp("xq-shell-a-source-missing");
    std::error_code error;
    CHECK(fs::create_directories(temp.path, error) && !error);
    const fs::path copiedSource = temp.path / "dicom-source";
    fs::copy(fixtureRoot / "regular-oblique", copiedSource,
             fs::copy_options::recursive, error);
    CHECK(!error && fs::is_directory(copiedSource));
    const fs::path projectPath = temp.path / "missing-source.xqproj";

    xq::DicomSeriesDescriptor descriptor;
    CHECK(selectSingleSeries(copiedSource, &descriptor));
    {
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
        xq::GdcmItkDicomSeriesReader reader;
        xq::DicomImportRequest request;
        request.sourceDirectory = copiedSource.string();
        request.sourceRelPath = relativeSourcePath(copiedSource, projectPath);
        request.seriesInstanceUid = descriptor.identity.seriesInstanceUid;
        request.nodeId = kIds.image;
        request.assetId = kIds.imageAsset;
        xq::DicomImportResult prepared =
            xq::DicomImportService::prepare(reader, request);
        CHECK(prepared.ok());
        CHECK(stack.push(std::make_unique<xq::ProjectNodeBatchCommand>(
            &project, std::move(prepared.batchSpec.value()))));
        CHECK(xq::XQProjectWriter::save(project, projectPath.string())
              == xq::XQProjectWriter::Status::Ok);
    }
    fs::remove_all(copiedSource, error);
    CHECK(!error && !fs::exists(copiedSource));

    xq::XQProjectReadResult reopened;
    CHECK(xq::XQProjectReader::load(projectPath.string(), &reopened)
          == xq::XQProjectReader::Status::Ok);
    const xq::XQImageVolumePayload* image = imagePayload(reopened.project);
    CHECK(image != nullptr);
    const std::size_t nodesBefore = nodeCount(reopened.project.scene());
    const std::size_t assetsBefore = reopened.project.assetRegistry().assetCount();
    xq::GeometryResourceManager manager(
        &reopened.project.assetRegistry(), temp.path.string());
    xq::GdcmItkDicomSeriesReader reader;
    const xq::ImageResourceResolveResult resolved =
        xq::ImageResourceResolver::acquire(
            kIds.imageAsset, image->volume(), projectPath.string(), manager,
            reopened.project.assetRegistry(), reader);
    CHECK(resolved.status == xq::DicomSeriesStatus::SourceNotFound);
    CHECK(!resolved.ok());
    CHECK(nodeCount(reopened.project.scene()) == nodesBefore);
    CHECK(reopened.project.assetRegistry().assetCount() == assetsBefore);
    return 0;
}

int testBadProfileAndWrongUnits(const SuccessSnapshot& success)
{
    {
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);

        xq::XQPath path;
        path.setId(kIds.path);
        path.setControlPoints({{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 60.0}}});
        CHECK(path.resample(1.0) == xq::XQPath::ResampleStatus::Ok);
        xq::XQDataNode pathNode(
            kIds.path, xq::XQDomainType::Path, "bad-profile path",
            std::make_shared<xq::XQPathPayload>(path));
        pathNode.setScaleSlot(xq::ScaleSlot::Organ);
        xq::XQDataNode contourNode(
            kIds.contour, xq::XQDomainType::ContourGroup,
            "bad-profile contours",
            std::make_shared<xq::XQContourGroupPayload>(
                makeContourGroup(path, false)));
        contourNode.setScaleSlot(xq::ScaleSlot::Organ);
        CHECK(project.scene().insert(std::move(pathNode))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(project.scene().insert(std::move(contourNode))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(project.scene().link_derived(kIds.path, kIds.contour)
              == xq::XQScene::RelationResult::Linked);

        xq::VesselProfileController controller(&project, &stack);
        xq::VesselProfileController::ContourIntent intent;
        intent.output.newProfileId = kIds.profile;
        intent.output.name = "must not publish";
        intent.pathNode = kIds.path;
        intent.contourGroupNode = kIds.contour;
        intent.frameOfReferenceId = "1.2.826.shell-a.bad-profile";
        const ProjectCounts before = counts(project, stack);
        CHECK(controller.assembleFromContours(intent)
              == xq::VesselProfileController::Status::AssemblyFailed);
        CHECK(sameCounts(before, counts(project, stack)));
        CHECK(project.scene().find(kIds.profile) == nullptr);
    }

    {
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
        xq::XQDataNode sourcePath(
            kIds.path, xq::XQDomainType::Path, "wrong-unit source",
            std::make_shared<xq::XQPathPayload>(xq::XQPath()));
        sourcePath.setScaleSlot(xq::ScaleSlot::Organ);
        xq::VesselProfileV1 wrongUnits = success.profile;
        wrongUnits.lengthUnit = xq::VesselProfileLengthUnit::Unknown;
        xq::XQDataNode profileNode(
            kIds.profile, xq::XQDomainType::VesselProfile,
            "wrong-unit profile",
            std::make_shared<xq::XQVesselProfilePayload>(wrongUnits));
        profileNode.setScaleSlot(xq::ScaleSlot::Organ);
        CHECK(project.scene().insert(std::move(sourcePath))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(project.scene().insert(std::move(profileNode))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(project.scene().link_derived(kIds.path, kIds.profile)
              == xq::XQScene::RelationResult::Linked);

        xq::FlowSmokeController controller(&project, &stack);
        xq::FlowSmokeController::SmokeIntent intent;
        intent.sourceProfileNode = kIds.profile;
        intent.output.caseNode = kIds.simulationCase;
        intent.output.caseName = "must not publish case";
        intent.output.resultNode = kIds.flowResult;
        intent.output.resultName = "must not publish result";
        const ProjectCounts before = counts(project, stack);
        const xq::FlowSmokeController::PreparedCommand prepared =
            controller.prepare(intent);
        CHECK(!prepared.ok());
        CHECK(prepared.status == xq::FlowSmokeController::Status::ComputeFailed);
        CHECK(prepared.assemblyStatus
              == xq::FlowInputAssembler::Status::InvalidProfile);
        CHECK(sameCounts(before, counts(project, stack)));
        CHECK(project.scene().find(kIds.simulationCase) == nullptr);
        CHECK(project.scene().find(kIds.flowResult) == nullptr);
    }
    return 0;
}

int testProductionSolverFailure(const SuccessSnapshot& success)
{
    const xq::FlowInputAssembler::Result assembled =
        xq::FlowInputAssembler::assemble(
            success.profile, xq::ScaleSlot::Organ,
            xq::FlowSmokeProtocolV1::engineeringSmoke());
    CHECK(assembled.ok());
    xq::FlowSolver1D::SolverInput unstable = assembled.solverInput;
    unstable.dt = 1.0;
    const xq::FlowSolver1D::Result failed =
        xq::FlowSolver1D::solve(unstable);
    CHECK(failed.status == xq::FlowSolver1D::Status::CflViolation);
    CHECK(!failed.ok());
    return 0;
}

int writeHistoryFixture(const SuccessSnapshot& success,
                        const fs::path& outputPath)
{
    std::error_code error;
    if (!outputPath.parent_path().empty()) {
        fs::create_directories(outputPath.parent_path(), error);
        CHECK(!error);
    }
    fs::remove(outputPath, error);
    fs::remove_all(
        outputPath.parent_path()
            / (outputPath.stem().string() + ".assets"),
        error);

    xq::XQProject project;
    xq::XQCommandStack stack;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);

    xq::XQPath historyPath;
    historyPath.setId(kIds.path);
    historyPath.setInterpolation(success.path.interpolation());
    historyPath.setControlPoints(success.path.controlPoints());
    CHECK(historyPath.resample(success.path.sampleSpacing())
          == xq::XQPath::ResampleStatus::Ok);
    xq::XQDataNode pathNode(
        kIds.path, xq::XQDomainType::Path, "Shell A history path",
        std::make_shared<xq::XQPathPayload>(std::move(historyPath)));
    pathNode.setScaleSlot(xq::ScaleSlot::Organ);
    CHECK(stack.push(std::make_unique<xq::AddNodeCommand>(
        &project.scene(), std::move(pathNode), "Add history path")));

    xq::XQDataNode contourNode(
        kIds.contour, xq::XQDomainType::ContourGroup,
        "Shell A history contours",
        std::make_shared<xq::XQContourGroupPayload>(success.contours));
    contourNode.setScaleSlot(xq::ScaleSlot::Organ);
    CHECK(stack.push(
        std::make_unique<xq::AddNodeWithSourceRelationCommand>(
            &project.scene(), std::move(contourNode), kIds.path,
            "Add history contours")));

    xq::XQDataNode profileNode(
        kIds.profile, xq::XQDomainType::VesselProfile,
        "Shell A historical VesselProfile",
        std::make_shared<xq::XQVesselProfilePayload>(success.profile));
    profileNode.setScaleSlot(xq::ScaleSlot::Organ);
    xq::ProjectNodeBatchSpec profileSpec(std::move(profileNode));
    profileSpec.assetToRegister = derivedAsset(
        kIds.profileAsset, xq::AssetKind::VesselProfile,
        "sha256:shell-a-profile-v1");
    profileSpec.bindAsset = kIds.profileAsset;
    profileSpec.sources = success.profile.derivationStamp.inputs;
    CHECK(stack.push(std::make_unique<xq::ProjectNodeBatchCommand>(
        &project, std::move(profileSpec), "Add historical VesselProfile")));

    xq::XQDataNode caseNode(
        kIds.simulationCase, xq::XQDomainType::SimulationCase,
        "L0 geometry smoke case",
        std::make_shared<xq::XQSimulationCasePayload>(success.simulationCase));
    caseNode.setScaleSlot(xq::ScaleSlot::Organ);
    xq::ProjectNodeBatchSpec caseSpec(std::move(caseNode));
    caseSpec.assetToRegister = derivedAsset(
        kIds.caseAsset, xq::AssetKind::SimulationCase,
        "sha256:shell-a-smoke-case-v1");
    caseSpec.bindAsset = kIds.caseAsset;
    caseSpec.sources.push_back({
        kIds.profile, 0, kIds.profileAsset, "sha256:shell-a-profile-v1"});

    xq::XQDataNode resultNode(
        kIds.flowResult, xq::XQDomainType::FlowResult,
        "L0 geometry smoke result",
        std::make_shared<xq::XQFlowResultPayload>(success.flow));
    resultNode.setScaleSlot(xq::ScaleSlot::Organ);
    xq::ProjectNodeBatchSpec resultSpec(std::move(resultNode));
    resultSpec.assetToRegister = derivedAsset(
        kIds.resultAsset, xq::AssetKind::FlowResult,
        "sha256:shell-a-smoke-result-v1");
    resultSpec.bindAsset = kIds.resultAsset;
    resultSpec.sources.push_back({
        kIds.profile, 0, kIds.profileAsset, "sha256:shell-a-profile-v1"});
    resultSpec.sources.push_back({
        kIds.simulationCase, 0, kIds.caseAsset,
        "sha256:shell-a-smoke-case-v1"});
    std::vector<xq::ProjectNodeBatchSpec> bundle;
    bundle.push_back(std::move(caseSpec));
    bundle.push_back(std::move(resultSpec));
    CHECK(stack.push(std::make_unique<xq::ProjectNodeBundleCommand>(
        &project, std::move(bundle), "Add historical L0 smoke bundle")));

    CHECK(xq::XQProjectWriter::save(project, outputPath.string())
          == xq::XQProjectWriter::Status::Ok);
    return 0;
}

} // namespace

int main()
{
    const fs::path configuredRoot(XQ_SHELL_A_DICOM_ROOT);
    SuccessSnapshot success;
#if defined(XQ_SHELL_A_REAL_DATA)
    const int chainResult = runSuccessChain(
        configuredRoot, true, false, &success);
    if (chainResult != 0) {
        return chainResult;
    }
    const xq::ImageGeometry& geometry = success.geometry;
    const double minimumSpacing = (std::min)(
        geometry.spacing[0], (std::min)(geometry.spacing[1], geometry.spacing[2]));
    const double maximumSpacing = (std::max)(
        geometry.spacing[0], (std::max)(geometry.spacing[1], geometry.spacing[2]));
    std::printf(
        "OK: authorized Shell A real-data chain; dims=%dx%dx%d, "
        "spacing-range=%.6g..%.6g mm, voxel-bytes=%zu, digest-prefix=%.12s\n",
        geometry.dimensions[0], geometry.dimensions[1], geometry.dimensions[2],
        minimumSpacing, maximumSpacing, success.voxelBytes,
        success.voxelDigest.c_str());
    return 0;
#else
    const fs::path successSeries = configuredRoot / "regular-oblique";
    int result = runSuccessChain(successSeries, false, true, &success);
    if (result != 0) {
        return result;
    }
    result = testBadDicomAndExplicitSeriesSelection(configuredRoot);
    if (result != 0) {
        return result;
    }
    result = testMissingExternalSource(configuredRoot);
    if (result != 0) {
        return result;
    }
    result = testBadProfileAndWrongUnits(success);
    if (result != 0) {
        return result;
    }
    result = testProductionSolverFailure(success);
    if (result != 0) {
        return result;
    }
    if (const char* fixtureOutput =
            std::getenv("XQ_SHELL_A_HISTORY_FIXTURE_OUT")) {
        if (*fixtureOutput != '\0') {
            result = writeHistoryFixture(success, fs::path(fixtureOutput));
            if (result != 0) {
                return result;
            }
        }
    }
    std::printf(
        "OK: DICOM -> Path -> ContourGroup -> VesselProfile -> "
        "L0 smoke -> save/reopen/lazy voxel E2E\n");
    return 0;
#endif
}
