#include "xq_Grid.h"

// --- GridConfigBuilder ---

GridConfigBuilder& GridConfigBuilder::withGlobalEdgeSize(double size)
{
    m_Params.globalEdgeSize = size;
    return *this;
}

GridConfigBuilder& GridConfigBuilder::withMaxEdgeSize(double size)
{
    m_Params.globalMaxEdgeSize = size;
    return *this;
}

GridConfigBuilder& GridConfigBuilder::withLocalEdgeSize(int faceId, double size)
{
    m_Params.localEdgeSizes[faceId] = size;
    return *this;
}

GridConfigBuilder& GridConfigBuilder::surfaceOnly(bool flag)
{
    m_Params.surfaceMeshOnly = flag;
    return *this;
}

GridConfigBuilder& GridConfigBuilder::withOptimizationPasses(int passes)
{
    m_Params.optimizationPasses = passes;
    return *this;
}

MeshParams GridConfigBuilder::build() const
{
    return m_Params;
}

// --- xq_Grid ---

xq_Grid::xq_Grid()
    : m_VolumeMesh(vtkSmartPointer<vtkUnstructuredGrid>::New())
    , m_SurfaceMesh(vtkSmartPointer<vtkPolyData>::New())
    , m_Type("Base")
{
}

void xq_Grid::SetModelElement(xq_VascularGeometry* elem)
{
    m_ModelElement = elem;
}

xq_VascularGeometry* xq_Grid::GetModelElement() const
{
    return m_ModelElement;
}

void xq_Grid::SetMeshParams(const MeshParams& params)
{
    m_Params = params;
}

const MeshParams& xq_Grid::GetMeshParams() const
{
    return m_Params;
}

vtkUnstructuredGrid* xq_Grid::GetVolumeMesh() const
{
    return m_VolumeMesh;
}

vtkPolyData* xq_Grid::GetSurfaceMesh() const
{
    return m_SurfaceMesh;
}

void xq_Grid::SetVolumeMesh(vtkSmartPointer<vtkUnstructuredGrid> mesh)
{
    if (!mesh)
        return;

    m_VolumeMesh->DeepCopy(mesh);
}

void xq_Grid::SetSurfaceMesh(vtkSmartPointer<vtkPolyData> mesh)
{
    if (!mesh)
        return;

    m_SurfaceMesh->DeepCopy(mesh);
}

std::string xq_Grid::GetType() const
{
    return m_Type;
}

int xq_Grid::GetNumberOfNodes() const
{
    return m_VolumeMesh->GetNumberOfPoints();
}

int xq_Grid::GetNumberOfElements() const
{
    return m_VolumeMesh->GetNumberOfCells();
}
