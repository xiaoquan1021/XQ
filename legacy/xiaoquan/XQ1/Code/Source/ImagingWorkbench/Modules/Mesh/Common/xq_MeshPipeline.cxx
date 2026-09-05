#include "xq_MeshPipeline.h"

#include "xq_MitkGrid.h"
#include "xq_Model.h"
#include "xq_MeshGenerator.h"
#include "xq_MeshQuality.h"

#include <xq_VascularGeometry.h>

#include <sstream>

namespace
{

auto makeError(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Error, std::move(message)};
}

auto makeWarning(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Warning, std::move(message)};
}

std::string BuildCommandHistory(const xq_MeshGenerationRequest& request)
{
    std::ostringstream oss;
    oss << "requestedBackend=TetGen"
        << ";actualBackend=vtk_delaunay3d_fallback"
        << ";globalEdgeSize=" << request.globalEdgeSize
        << ";preserveSurface=" << (request.preserveSurface ? 1 : 0)
        << ";optimize=" << (request.optimize ? 1 : 0)
        << ";minDihedral=" << request.minDihedral
        << ";maxEdgeSize=" << request.maxEdgeSize
        << ";boundaryLayerLayers=" << request.boundaryLayerLayers
        << ";boundaryLayerFirstHeight=" << request.boundaryLayerFirstHeight
        << ";boundaryLayerGrowthRate=" << request.boundaryLayerGrowthRate
        << ";localFaceSizes=" << request.localFaceSizes.size()
        << ";refinementRegions=" << request.refinementRegions.size();
    return oss.str();
}

std::string SerializeLocalFaceSizes(const std::map<int, double>& localFaceSizes)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto& [faceId, edgeSize] : localFaceSizes)
    {
        if (!first)
            oss << ';';
        first = false;
        oss << faceId << ':' << edgeSize;
    }
    return oss.str();
}

const char* RegionTypeName(xq_RefinementRegion::Type type)
{
    switch (type)
    {
    case xq_RefinementRegion::Type::Sphere: return "Sphere";
    case xq_RefinementRegion::Type::Box: return "Box";
    case xq_RefinementRegion::Type::Cylinder: return "Cylinder";
    }
    return "Sphere";
}

std::string SerializeRefinementRegions(const std::vector<xq_RefinementRegion>& regions)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto& region : regions)
    {
        if (!first)
            oss << ';';
        first = false;
        oss << RegionTypeName(region.type) << ','
            << region.center[0] << ',' << region.center[1] << ',' << region.center[2] << ','
            << region.radiusOrSize[0] << ',' << region.radiusOrSize[1] << ','
            << region.radiusOrSize[2] << ',' << region.edgeSize;
    }
    return oss.str();
}

} // namespace

xq_MeshGenerationResult xq_MeshPipelineService::CreateVolumeMesh(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode::Pointer& modelNode,
    const xq_MeshGenerationRequest& request)
{
    xq_MeshGenerationResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }

    if (modelNode.IsNull() ||
        !xq::pipeline::HasStage(modelNode, xq::pipeline::Stage::Model))
    {
        result.diagnostics.push_back(makeError(
            "Mesh stage requires an upstream Model node (xq.pipeline.stage=model)."));
        return result;
    }

    auto* model = dynamic_cast<xq_Model*>(modelNode->GetData());
    auto* modelElement = model ? model->GetModelElement(0) : nullptr;
    if (!modelElement || !modelElement->GetWholeVtkPolyData() ||
        modelElement->GetWholeVtkPolyData()->GetNumberOfPoints() == 0)
    {
        result.diagnostics.push_back(makeError("Model node does not contain usable geometry."));
        return result;
    }
    bool modelQaOk = true;
    if (modelNode->GetBoolProperty("xq.model.qa.ok", modelQaOk) && !modelQaOk)
    {
        result.diagnostics.push_back(makeError(
            "Mesh generation blocked: upstream model QA failed."));
        return result;
    }

    // Delegate generation to the XQMeshGenerator interface. The current
    // native backend is VTK Delaunay3D and is recorded explicitly below.
    auto generator = CreateMeshGenerator("tetgen");

    xq_MeshGenerator::Params params;
    params.globalEdgeSize         = request.globalEdgeSize;
    params.localFaceSizes         = request.localFaceSizes;
    params.refinementRegions      = request.refinementRegions;
    params.preserveSurface        = request.preserveSurface;
    params.optimize               = request.optimize;
    params.minDihedral            = request.minDihedral;
    params.maxEdgeSize            = request.maxEdgeSize;
    params.boundaryLayer          = request.boundaryLayerLayers > 0;
    params.boundaryLayerCount     = request.boundaryLayerLayers;
    // XQ fix: first-height was previously dropped — now propagated.
    params.boundaryLayerFirstHeight = request.boundaryLayerFirstHeight;
    params.boundaryLayerGrowthRate  = request.boundaryLayerGrowthRate;

    auto generated = generator->Generate(modelElement, params);
    if (!generated.ok || !generated.grid)
    {
        result.diagnostics.push_back(makeError(
            generated.diagnostic.empty()
                ? "Mesh generation failed."
                : generated.diagnostic));
        return result;
    }
    if (!generated.diagnostic.empty())
        result.diagnostics.push_back(makeWarning(generated.diagnostic));

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(generated.grid.release(), 0);

    // Evaluate mesh quality and attach as node properties (P7.2)
    auto* gridData = mitkGrid->GetMesh(0);
    auto qualityReport = xq_MeshQuality::Evaluate(
        gridData ? gridData->GetVolumeMesh() : nullptr);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetData(mitkGrid);
    meshNode->SetName(request.meshName.empty() ? modelNode->GetName() + "_mesh" : request.meshName);
    meshNode->SetStringProperty("xq.mesh.type", "VTK Delaunay3D fallback");
    meshNode->SetStringProperty("xq.mesh.requested_backend", "tetgen");
    meshNode->SetStringProperty("xq.mesh.actual_backend", std::string(generator->Name()).c_str());
    meshNode->SetBoolProperty("xq.mesh.backend.fallback", true);
    meshNode->SetStringProperty("xq.mesh.command_history", BuildCommandHistory(request).c_str());
    meshNode->SetDoubleProperty("xq.mesh.globalEdgeSize", request.globalEdgeSize);
    meshNode->SetDoubleProperty("xq.mesh.bl.firstHeight", request.boundaryLayerFirstHeight);
    meshNode->SetIntProperty("xq.mesh.bl.layers", request.boundaryLayerLayers);
    meshNode->SetDoubleProperty("xq.mesh.bl.growthRate", request.boundaryLayerGrowthRate);
    meshNode->SetIntProperty("xq.mesh.local_face_sizes", static_cast<int>(request.localFaceSizes.size()));
    meshNode->SetIntProperty("xq.mesh.refinement_regions", static_cast<int>(request.refinementRegions.size()));
    meshNode->SetStringProperty(
        "xq.mesh.local_face_sizes.values",
        SerializeLocalFaceSizes(request.localFaceSizes).c_str());
    meshNode->SetStringProperty(
        "xq.mesh.refinement_regions.values",
        SerializeRefinementRegions(request.refinementRegions).c_str());
    meshNode->SetBoolProperty("xq.mesh.preserve_surface", request.preserveSurface);
    meshNode->SetBoolProperty("xq.mesh.optimize", request.optimize);
    meshNode->SetDoubleProperty("xq.mesh.min_dihedral", request.minDihedral);
    meshNode->SetDoubleProperty("xq.mesh.max_edge_size", request.maxEdgeSize);
    meshNode->SetBoolProperty("xq.mesh.local_face_sizes.applied", false);
    meshNode->SetBoolProperty("xq.mesh.refinement_regions.applied", false);
    meshNode->SetStringProperty(
        "xq.mesh.capability.diagnostic",
        "Requested TetGen behavior is not available in this XQ-native build. "
        "The actual backend is VTK Delaunay3D fallback; requested local sizing, "
        "refinement regions, and boundary layers are recorded but not fully applied.");
    if (!request.localFaceSizes.empty())
    {
        result.diagnostics.push_back(makeWarning(
            "Local face-size controls are recorded on the mesh node, but the "
            "current VTK Delaunay3D fallback does not fully apply face-local sizing."));
    }
    if (!request.refinementRegions.empty())
    {
        result.diagnostics.push_back(makeWarning(
            "Refinement regions are recorded on the mesh node, but the current "
            "VTK Delaunay3D fallback does not apply region-specific sizing."));
    }
    if (request.boundaryLayerLayers > 0)
    {
        result.diagnostics.push_back(makeWarning(
            "Boundary-layer parameters are recorded on the mesh node, but the "
            "current VTK Delaunay3D fallback does not generate true boundary layers."));
    }

    meshNode->SetBoolProperty("xq.mesh.qa.ok", qualityReport.ok);
    meshNode->SetIntProperty("xq.mesh.cells", qualityReport.numberOfCells);
    meshNode->SetIntProperty("xq.mesh.points", qualityReport.numberOfPoints);
    meshNode->SetIntProperty("xq.mesh.negative_volume_count", qualityReport.negativeVolumeCount);
    meshNode->SetDoubleProperty("xq.mesh.min_volume", qualityReport.minVolume);
    meshNode->SetDoubleProperty("xq.mesh.max_volume", qualityReport.maxVolume);
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(
        meshNode, xq::pipeline::kSourceModelProperty, modelNode->GetName());
    xq::pipeline::SetStringProperty(
        meshNode, xq::pipeline::kAlgorithmProperty, std::string(generator->Name()));

    auto meshFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::VolumeMesh, modelNode.GetPointer());
    if (meshFolder.IsNotNull())
        dataStorage->Add(meshNode, meshFolder);
    else
        dataStorage->Add(meshNode, modelNode);

    result.ok = true;
    result.node = meshNode;
    result.gridData = mitkGrid;
    return result;
}
