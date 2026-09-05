#include "xq_MeshGenerator.h"

#include "xq_TetGenGrid.h"
#include "xq_VascularGeometry.h"

xq_MeshGenerator::Result
xq_TetGenMeshGenerator::Generate(
    xq_VascularGeometry* modelElement, const Params& params)
{
    Result out;
    if (!modelElement || !modelElement->GetWholeVtkPolyData() ||
        modelElement->GetWholeVtkPolyData()->GetNumberOfPoints() == 0)
    {
        out.diagnostic = "VTK Delaunay3D fallback: model element has no usable surface.";
        return out;
    }

    auto grid = std::make_unique<xq_TetGenGrid>();
    grid->SetModelElement(modelElement);

    MeshParams mp;
    mp.globalEdgeSize = params.globalEdgeSize;
    mp.localEdgeSizes = params.localFaceSizes;
    grid->SetMeshParams(mp);

    grid->SetQualityRatio(params.qualityRatio);
    grid->SetVolumeConstraint(params.volumeConstraint);
    grid->SetBoundaryLayerMesh(params.boundaryLayer && params.boundaryLayerCount > 0);
    grid->SetBoundaryLayerCount(params.boundaryLayerCount);
    grid->SetBoundaryLayerGrowthRate(params.boundaryLayerGrowthRate);
    // XQ fix: first-height is now propagated.
    grid->SetBoundaryLayerFirstHeight(params.boundaryLayerFirstHeight);

    if (!grid->GenerateMesh())
    {
        out.diagnostic = "VTK Delaunay3D fallback: GenerateMesh failed.";
        return out;
    }

    // Preserve the model surface as the mesh's surface view so face tagging
    // and BC plumbing downstream has direct access to the original FaceIds.
    auto modelSurface = modelElement->GetWholeVtkPolyData();
    if (modelSurface && modelSurface->GetNumberOfCells() > 0)
        grid->SetSurfaceMesh(modelSurface);

    out.ok = true;
    out.grid = std::move(grid);
    return out;
}

std::unique_ptr<xq_MeshGenerator>
CreateMeshGenerator(std::string_view /*name*/)
{
    // Only the XQ-native VTK Delaunay3D fallback is available today. Do not
    // imply that full TetGen/Netgen/MMG behavior ran.
    return std::make_unique<xq_TetGenMeshGenerator>();
}
