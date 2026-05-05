#include "xq_ModelPipeline.h"

#include "xq_Model.h"
#include "xq_ModelQuality.h"
#include "xq_PolyGeometry.h"
#include "xq_GeometryUtils.h"
#include "xq_SolidModeler.h"

#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>
#include <xq_ContourGroup.h>
#include <xq_ContourGroupMigration.h>
#include <xq_SegmentationUtils.h>
#include <xq_VesselCenterline.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using PlacementFrames = std::vector<xq_ProfilePlacementFrame>;

auto makeError(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Error, std::move(message)};
}

auto makeWarning(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Warning, std::move(message)};
}

auto buildPlacementFrames(xq_VesselCenterline* centerline) -> PlacementFrames
{
    PlacementFrames frames;
    if (!centerline)
        return frames;

    auto* segment = centerline->GetSegment();
    if (!segment)
        return frames;

    if (segment->GetTraceVertexCount() == 0 && segment->GetAnchorCount() >= 2)
        segment->Interpolate();

    const auto traceVertices = segment->GetTraceVertices();
    frames.reserve(traceVertices.size());
    for (const auto& traceVertex : traceVertices)
    {
        xq_ProfilePlacementFrame frame;
        frame.pathPosIndex = traceVertex.id;
        frame.position = traceVertex.pos;
        frame.tangent = traceVertex.tangent;
        frame.rotation = traceVertex.rotation;
        frames.push_back(frame);
    }

    return frames;
}

auto findPathByName(mitk::DataStorage* dataStorage, const std::string& pathName)
    -> xq_VesselCenterline*
{
    if (!dataStorage || pathName.empty())
        return nullptr;

    const auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        const bool isPath = xq::pipeline::IsPathNode(node);
        if (isPath && node->GetName() == pathName)
            return dynamic_cast<xq_VesselCenterline*>(node->GetData());
    }

    return nullptr;
}

auto resolvePlacementFramesForPath(mitk::DataStorage* dataStorage, const std::string& pathName)
    -> PlacementFrames
{
    return buildPlacementFrames(findPathByName(dataStorage, pathName));
}

// Assign per-cell FaceIds array + face infos from the modeler result.
// XQ fix: each boundary loop (cap) now owns its own face id, instead of the
// broken "cellsPerCap = total / capCount" uniform slicing.
auto assignFaceMetadata(
    xq_PolyGeometry* geometry,
    const std::vector<int>& perCellFaceId,
    int capCount) -> void
{
    if (!geometry || perCellFaceId.empty())
        return;

    auto faceIds = vtkSmartPointer<vtkIntArray>::New();
    faceIds->SetName("FaceIds");
    faceIds->SetNumberOfComponents(1);
    faceIds->SetNumberOfTuples(static_cast<vtkIdType>(perCellFaceId.size()));
    for (size_t i = 0; i < perCellFaceId.size(); ++i)
        faceIds->SetValue(static_cast<vtkIdType>(i), perCellFaceId[i]);
    geometry->AssignFaceIds(faceIds);

    FaceInfo wallFace;
    wallFace.id = 0;
    wallFace.name = "wall";
    wallFace.type = "wall";
    wallFace.color = {0.85f, 0.85f, 0.88f};
    geometry->SetFaceInfo(0, wallFace);

    for (int capIndex = 0; capIndex < capCount; ++capIndex)
    {
        FaceInfo capFace;
        capFace.id = 1 + capIndex;
        capFace.name = "cap_" + std::to_string(capIndex + 1);
        capFace.type = "cap";
        capFace.color = {0.55f, 0.75f, 0.95f};
        geometry->SetFaceInfo(capFace.id, capFace);
    }
}

} // namespace

xq_CreateModelResult xq_ModelPipelineService::CreateModel(
    mitk::DataStorage* dataStorage,
    const xq_CreateModelRequest& request)
{
    xq_CreateModelResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }

    if (request.modelName.empty())
    {
        result.diagnostics.push_back(makeError("Model name is empty."));
        return result;
    }

    auto profileNodes = xq::pipeline::GetNodesByStage(
        dataStorage, xq::pipeline::Stage::ContourGroup);
    auto contourNodes = dataStorage->GetSubset(
        mitk::NodePredicateDataType::New("xq_ContourGroup"));

    std::set<std::string> canonicalPathNames;
    std::vector<std::string> sourceNames;
    std::vector<std::string> sourcePaths;
    std::vector<xq_ProfileGroup::Pointer> migratedGroups;
    std::vector<vtkSmartPointer<vtkPolyData>> loftedWalls;

    // Helper: decide whether a path name passes the request filter.
    auto acceptPath = [&](const std::string& pathName) -> bool
    {
        if (request.pathFilter.empty())
            return true;
        return pathName == request.pathFilter;
    };

    for (const auto& profileNode : profileNodes)
    {
        auto* profileGroup = dynamic_cast<xq_ProfileGroup*>(profileNode->GetData());
        if (!profileGroup)
            continue;

        const auto pathName = profileGroup->GetAttribute("path_name");
        if (!acceptPath(pathName))
            continue;

        const auto loftReason =
            xq_SegmentationUtils::GetLoftReadinessBlockingReason(profileGroup);
        const auto phaseReason =
            xq_SegmentationUtils::GetModelingPhaseBoundaryBlockingReason(profileGroup);
        if (!loftReason.empty())
        {
            result.diagnostics.push_back(makeError(
                "ProfileGroup '" + profileNode->GetName() + "' is not loft-ready: " + loftReason));
            continue;
        }
        if (!phaseReason.empty())
        {
            result.diagnostics.push_back(makeError(
                "ProfileGroup '" + profileNode->GetName() + "' is not ready for modeling: " + phaseReason));
            continue;
        }

        auto loftedMesh = profileGroup->GetLoftedMesh(0);
        if (!loftedMesh || loftedMesh->GetNumberOfPoints() == 0)
            continue;

        loftedWalls.push_back(loftedMesh);
        sourceNames.push_back(profileNode->GetName());
        if (!pathName.empty())
        {
            canonicalPathNames.insert(pathName);
            sourcePaths.push_back(pathName);
        }
    }

    for (auto it = contourNodes->Begin(); it != contourNodes->End(); ++it)
    {
        auto* contourGroup = dynamic_cast<xq_ContourGroup*>(it->Value()->GetData());
        if (!contourGroup)
            continue;

        const auto pathName = contourGroup->GetPathName();
        if (!acceptPath(pathName))
            continue;
        // Skip groups already covered by a canonical ProfileGroup on the
        // same path to avoid duplicated walls in the model.
        if (!pathName.empty() && canonicalPathNames.count(pathName) != 0)
            continue;

        const auto frames = resolvePlacementFramesForPath(dataStorage, pathName);
        auto migrated = (!pathName.empty() && !frames.empty())
            ? xq_ContourGroupMigration::ToProfileGroup(contourGroup, frames)
            : xq_ContourGroupMigration::ToProfileGroup(contourGroup);
        if (migrated.IsNull() || migrated->GetProfileCount() == 0)
            continue;

        auto mesh = migrated->GetLoftedMesh(0);
        if (!mesh || mesh->GetNumberOfPoints() == 0)
        {
            mesh = xq_SegmentationUtils::LoftProfileGroup(migrated);
            if (mesh && mesh->GetNumberOfPoints() > 0)
                migrated->SetLoftedMesh(mesh, 0);
        }

        if (!mesh || mesh->GetNumberOfPoints() == 0)
            continue;

        loftedWalls.push_back(mesh);
        migratedGroups.push_back(migrated);
        sourceNames.push_back(it->Value()->GetName());
        if (!pathName.empty())
            sourcePaths.push_back(pathName);
    }

    if (loftedWalls.empty())
    {
        if (result.diagnostics.empty())
        {
            result.diagnostics.push_back(makeError(
                request.pathFilter.empty()
                    ? "No usable contour/profile group loft surfaces were found in DataStorage."
                    : "No contour groups were found with xq.source.path == '" +
                        request.pathFilter + "'."));
        }
        return result;
    }

    // Delegate capping/booleans/blend to the solid modeler (Bridge).
    auto modeler = CreateSolidModeler(request.engine);
    xq_SolidModeler::BuildRequest buildReq;
    buildReq.loftedWalls  = std::move(loftedWalls);
    buildReq.capEnds      = true;
    buildReq.booleanUnion = true;
    buildReq.blendRadius  = request.blendRadius;

    auto built = modeler->Build(buildReq);
    if (!built.ok || !built.surface)
    {
        result.diagnostics.push_back(makeError(
            built.diagnostic.empty()
                ? "Solid modeler failed to build the surface."
                : built.diagnostic));
        return result;
    }
    if (!built.diagnostic.empty())
        result.diagnostics.push_back(makeWarning(built.diagnostic));

    auto normalsMesh = xq::geometry::computeNormals(built.surface);

    auto modelData = xq_Model::New();
    modelData->SetType(request.modelType);

    auto geometry = std::make_unique<xq_PolyGeometry>();
    geometry->SetWholeVtkPolyData(normalsMesh ? normalsMesh : built.surface);
    assignFaceMetadata(geometry.get(), built.faceIdsPerCell, built.capCount);
    modelData->SetModelElement(std::move(geometry), 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetData(modelData);
    modelNode->SetName(request.modelName);
    modelNode->SetStringProperty("xq.model.type", request.modelType.c_str());
    modelNode->SetIntProperty("xq.model.sampling", request.numSampling);
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(
        modelNode, xq::pipeline::kAlgorithmProperty, std::string(modeler->Name()));

    xq::pipeline::SetStringProperty(
        modelNode, xq::pipeline::kSourceContourGroupsProperty,
        xq::pipeline::JoinSourceList(sourceNames));

    // Record the unique source path(s) for downstream stages.
    std::vector<std::string> uniquePaths;
    {
        std::set<std::string> seen;
        for (const auto& p : sourcePaths)
            if (!p.empty() && seen.insert(p).second)
                uniquePaths.push_back(p);
    }
    if (!uniquePaths.empty())
        xq::pipeline::SetStringProperty(
            modelNode, xq::pipeline::kSourcePathProperty,
            xq::pipeline::JoinSourceList(uniquePaths));

    // Run model quality assurance
    auto* modelElement = modelData->GetModelElement(0);
    if (modelElement)
    {
        auto qa = xq_ModelQuality::Evaluate(modelElement->GetWholeVtkPolyData());
        modelNode->SetBoolProperty("xq.model.qa.ok", qa.ok);
        modelNode->SetIntProperty("xq.model.qa.boundary_edges", qa.boundaryEdges);
        modelNode->SetIntProperty("xq.model.qa.non_manifold_edges", qa.nonManifoldEdges);
        modelNode->SetIntProperty("xq.model.qa.connected_components", qa.connectedComponents);
        modelNode->SetBoolProperty("xq.model.qa.has_face_ids", qa.hasFaceIds);
        modelNode->SetIntProperty("xq.model.face_count", qa.faceCount);

        if (!qa.ok)
        {
            result.diagnostics.push_back(makeWarning(
                "Model QA: non-manifold edges detected (count=" +
                std::to_string(qa.nonManifoldEdges) + ")"));
        }
    }

    // Prefer the Models category folder; otherwise root level.
    // Use the first source contour group as project-context reference.
    mitk::DataNode::Pointer ctxNode = nullptr;
    if (!sourceNames.empty())
        ctxNode = xq::pipeline::FindNodeByNameAndStage(
            dataStorage, sourceNames.front(), xq::pipeline::Stage::ContourGroup);
    auto modelFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::Model, ctxNode.GetPointer());
    if (modelFolder.IsNotNull())
        dataStorage->Add(modelNode, modelFolder);
    else
        dataStorage->Add(modelNode);

    result.ok = true;
    result.node = modelNode;
    result.surface = normalsMesh ? normalsMesh : built.surface;
    return result;
}
