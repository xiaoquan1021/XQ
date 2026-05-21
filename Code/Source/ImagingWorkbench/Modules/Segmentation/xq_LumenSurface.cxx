#include "xq_LumenSurface.h"

#include <vtkMassProperties.h>
#include <vtkTriangleFilter.h>

xq_LumenSurface::xq_LumenSurface()
{
    // XQ fix: without a valid TimeGeometry, MITK's VtkPropRenderer culls
    // this node's mapper before GenerateDataForRenderer runs — see
    // xq_VesselCenterline / xq_ProfileGroup for the same rationale.
    Superclass::InitializeTimeGeometry(1);
}

void xq_LumenSurface::SetSurfaceMesh(vtkSmartPointer<vtkPolyData> polyData, unsigned int timeStep)
{
    m_MeshSeries[timeStep] = polyData;
    Modified();
}

vtkSmartPointer<vtkPolyData> xq_LumenSurface::GetSurfaceMesh(unsigned int timeStep) const
{
    const auto it = m_MeshSeries.find(timeStep);
    if (it == m_MeshSeries.cend())
        return nullptr;
    return it->second;
}

auto xq_LumenSurface::computeMassProperties(vtkPolyData* pd) -> vtkSmartPointer<vtkMassProperties>
{
    auto triFilter = vtkSmartPointer<vtkTriangleFilter>::New();
    triFilter->SetInputData(pd);
    triFilter->Update();

    auto massProps = vtkSmartPointer<vtkMassProperties>::New();
    massProps->SetInputConnection(triFilter->GetOutputPort());
    massProps->Update();

    return massProps;
}

double xq_LumenSurface::GetSurfaceArea(unsigned int timeStep) const
{
    auto pd = GetSurfaceMesh(timeStep);
    if (!pd || pd->GetNumberOfPolys() == 0)
        return 0.0;

    return computeMassProperties(pd)->GetSurfaceArea();
}

double xq_LumenSurface::GetVolume(unsigned int timeStep) const
{
    auto pd = GetSurfaceMesh(timeStep);
    if (!pd || pd->GetNumberOfPolys() == 0)
        return 0.0;

    return computeMassProperties(pd)->GetVolume();
}

void xq_LumenSurface::SetRequestedRegionToLargestPossibleRegion()
{
}

bool xq_LumenSurface::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_LumenSurface::VerifyRequestedRegion()
{
    return true;
}

void xq_LumenSurface::SetRequestedRegion(const itk::DataObject* /*data*/)
{
}

void xq_LumenSurface::UpdateOutputInformation()
{
    if (GetSource())
        GetSource()->UpdateOutputInformation();

    if (GetTimeGeometry())
        GetTimeGeometry()->Update();
}

void xq_LumenSurface::ClearData()
{
    m_MeshSeries.clear();
}

void xq_LumenSurface::InitializeEmpty()
{
    ClearData();
    Superclass::InitializeEmpty();
}
