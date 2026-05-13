#include "xq_MeshPipeline.h"

#include "xq_MitkGrid.h"
#include "xq_Model.h"
#include "xq_MeshGenerator.h"
#include "xq_MeshQuality.h"

#include <xq_VascularGeometry.h>

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

    // Delegate generation to the XQMeshGenerator interface (TetGen today).
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
    meshNode->SetStringProperty("xq.mesh.type", "TetGen");
    meshNode->SetDoubleProperty("xq.mesh.globalEdgeSize", request.globalEdgeSize);
    meshNode->SetDoubleProperty("xq.mesh.bl.firstHeight", request.boundaryLayerFirstHeight);
    meshNode->SetIntProperty("xq.mesh.bl.layers", request.boundaryLayerLayers);
    meshNode->SetDoubleProperty("xq.mesh.bl.growthRate", request.boundaryLayerGrowthRate);
    meshNode->SetIntProperty("xq.mesh.local_face_sizes", static_cast<int>(request.localFaceSizes.size()));
    meshNode->SetIntProperty("xq.mesh.refinement_regions", static_cast<int>(request.refinementRegions.size()));
    meshNode->SetBoolProperty("xq.mesh.optimize", request.optimize);

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
