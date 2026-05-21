#include "xq_MitkSeg3D.h"
#include "xq_MitkSeg3DOperation.h"

#include <mitkOperation.h>

xq_MitkSeg3D::xq_MitkSeg3D()
{
    // XQ fix: without a valid TimeGeometry, MITK's VtkPropRenderer culls
    // this node's mapper before GenerateDataForRenderer runs — see
    // xq_VesselCenterline / xq_ProfileGroup for the same rationale.
    Superclass::InitializeTimeGeometry(1);
}

xq_MitkSeg3D::xq_MitkSeg3D(const xq_MitkSeg3D& other)
    : mitk::BaseData(other)
    , m_Method(other.m_Method)
    , m_SeedPoints(other.m_SeedPoints)
    , m_UpperThreshold(other.m_UpperThreshold)
    , m_LowerThreshold(other.m_LowerThreshold)
{
    if (other.m_PolyData != nullptr) {
        m_PolyData = vtkSmartPointer<vtkPolyData>::New();
        m_PolyData->DeepCopy(other.m_PolyData);
    }
}

void xq_MitkSeg3D::SetSurfaceMesh(vtkSmartPointer<vtkPolyData> pd)
{
    m_PolyData = pd;
    Modified();
}

vtkSmartPointer<vtkPolyData> xq_MitkSeg3D::GetSurfaceMesh() const
{
    return m_PolyData;
}

void xq_MitkSeg3D::SetMethod(Seg3DMethod method)
{
    m_Method = method;
    Modified();
}

xq_MitkSeg3D::Seg3DMethod xq_MitkSeg3D::GetMethod() const
{
    return m_Method;
}

std::string_view xq_MitkSeg3D::GetMethodString() const
{
    auto idx = static_cast<std::size_t>(m_Method);
    if (idx < kMethodNames.size()) {
        return kMethodNames[idx];
    }
    return "Unknown";
}

void xq_MitkSeg3D::AddSeedPoint(const mitk::Point3D& pt)
{
    m_SeedPoints.push_back(pt);
    Modified();
}

void xq_MitkSeg3D::ClearSeedPoints()
{
    m_SeedPoints.clear();
    Modified();
}

std::vector<mitk::Point3D> xq_MitkSeg3D::GetSeedPoints() const
{
    return m_SeedPoints;
}

void xq_MitkSeg3D::SetUpperThreshold(double val)
{
    m_UpperThreshold = val;
    Modified();
}

void xq_MitkSeg3D::SetLowerThreshold(double val)
{
    m_LowerThreshold = val;
    Modified();
}

double xq_MitkSeg3D::GetUpperThreshold() const
{
    return m_UpperThreshold;
}

double xq_MitkSeg3D::GetLowerThreshold() const
{
    return m_LowerThreshold;
}

void xq_MitkSeg3D::SetRequestedRegionToLargestPossibleRegion()
{
}

bool xq_MitkSeg3D::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_MitkSeg3D::VerifyRequestedRegion()
{
    return true;
}

void xq_MitkSeg3D::SetRequestedRegion(const itk::DataObject* /*data*/)
{
}

void xq_MitkSeg3D::UpdateOutputInformation()
{
    if (auto source = GetSource()) {
        source->UpdateOutputInformation();
    }

    if (auto* timeGeo = GetTimeGeometry()) {
        timeGeo->Update();
    }
}

void xq_MitkSeg3D::ExecuteOperation(mitk::Operation* operation)
{
    if (operation == nullptr) {
        return;
    }

    auto* seg3DOp = dynamic_cast<xq_MitkSeg3DOperation*>(operation);
    if (seg3DOp == nullptr) {
        return;
    }

    switch (operation->GetOperationType())
    {
    case OpSETSEG3DRESULT:
        SetSurfaceMesh(seg3DOp->GetPolyData());
        break;
    case OpADDSEEDPOINT:
        AddSeedPoint(seg3DOp->GetSeedPoint());
        break;
    case OpCLEARSEEDPOINTS:
        ClearSeedPoints();
        break;
    case OpSETMETHOD:
        SetMethod(static_cast<Seg3DMethod>(seg3DOp->GetMethodId()));
        break;
    }
}
