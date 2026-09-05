#include "xq_MitkGrid.h"
#include "xq_Grid.h"
#include "xq_TetGenGrid.h"
#include "xq_MitkGridOperation.h"

#include <mitkGeometry3D.h>
#include <mitkSlicedGeometry3D.h>
#include <mitkProportionalTimeGeometry.h>
#include <mitkOperation.h>

#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace
{

constexpr unsigned int kInitialTimeSteps = 1;

bool HasValidVolumeMesh(const xq_Grid* grid)
{
    if (!grid) return false;
    auto* ugrid = grid->GetVolumeMesh();
    return ugrid && ugrid->GetNumberOfCells() > 0;
}

void UpdateGeometryBounds(mitk::TimeGeometry* timeGeometry,
                          const std::vector<std::unique_ptr<xq_Grid>>& meshList)
{
    if (!timeGeometry) return;

    for (unsigned int t = 0; t < meshList.size(); ++t)
    {
        if (!meshList[t]) continue;

        auto* ugrid = meshList[t]->GetVolumeMesh();
        if (!ugrid || ugrid->GetNumberOfPoints() == 0) continue;

        auto geometry = timeGeometry->GetGeometryForTimeStep(t);
        if (geometry.IsNull()) continue;

        double bounds[6];
        ugrid->GetBounds(bounds);
        mitk::BoundingBox::BoundsArrayType mitkBounds;
        std::copy(std::begin(bounds), std::end(bounds), mitkBounds.Begin());
        geometry->SetBounds(mitkBounds);
    }
}

} // anonymous namespace

xq_MitkGrid::xq_MitkGrid()
    : m_MeshList(kInitialTimeSteps)
{
    this->InitializeTimeGeometry(kInitialTimeSteps);
}

xq_MitkGrid::xq_MitkGrid(const xq_MitkGrid& other)
    : mitk::BaseData(other)
    , m_MeshList(other.m_MeshList.size())
{
    for (size_t i = 0; i < other.m_MeshList.size(); ++i)
    {
        if (!other.m_MeshList[i]) continue;

        auto copy = std::make_unique<xq_TetGenGrid>();
        auto* srcVol = other.m_MeshList[i]->GetVolumeMesh();
        if (srcVol && srcVol->GetNumberOfCells() > 0)
            copy->GetVolumeMesh()->DeepCopy(srcVol);

        auto* srcSurf = other.m_MeshList[i]->GetSurfaceMesh();
        if (srcSurf && srcSurf->GetNumberOfPoints() > 0)
            copy->GetSurfaceMesh()->DeepCopy(srcSurf);

        copy->SetMeshParams(other.m_MeshList[i]->GetMeshParams());
        copy->SetModelElement(other.m_MeshList[i]->GetModelElement());
        m_MeshList[i] = std::move(copy);
    }
}

xq_MitkGrid::~xq_MitkGrid()
{
    m_MeshList.clear();
}

void xq_MitkGrid::UpdateOutputInformation()
{
    if (this->GetSource())
        this->GetSource()->UpdateOutputInformation();

    UpdateGeometryBounds(this->GetTimeGeometry(), m_MeshList);
}

void xq_MitkGrid::SetRequestedRegionToLargestPossibleRegion() {}

bool xq_MitkGrid::RequestedRegionIsOutsideOfTheBufferedRegion() { return false; }

bool xq_MitkGrid::VerifyRequestedRegion() { return true; }

void xq_MitkGrid::SetRequestedRegion(const itk::DataObject* /*data*/) {}

void xq_MitkGrid::Expand(unsigned int timeSteps)
{
    if (timeSteps <= m_MeshList.size()) return;
    m_MeshList.resize(timeSteps);
    Superclass::Expand(timeSteps);
}

void xq_MitkGrid::ExecuteOperation(mitk::Operation* operation)
{
    auto* meshOp = dynamic_cast<xq_MitkGridOperation*>(operation);
    if (!meshOp) return;

    const auto t = meshOp->GetTimeStep();

    using OpHandler = std::function<bool(xq_MitkGrid&, xq_MitkGridOperation*, unsigned int)>;
    static const std::unordered_map<int, OpHandler> dispatch = {
        {xq_MitkGridOperation::OpSETMESH, [](xq_MitkGrid& self, xq_MitkGridOperation* op, unsigned int ts) {
            return self.ApplySetMesh(op->GetMesh(), ts);
        }},
        {xq_MitkGridOperation::OpGENERATEMESH, [](xq_MitkGrid& self, xq_MitkGridOperation*, unsigned int ts) {
            return self.ApplyGenerateMesh(ts);
        }},
        {xq_MitkGridOperation::OpADAPTMESH, [](xq_MitkGrid& self, xq_MitkGridOperation*, unsigned int ts) {
            return self.ApplyAdaptMesh(ts);
        }},
    };

    auto it = dispatch.find(operation->GetOperationType());
    if (it != dispatch.end() && it->second(*this, meshOp, t))
        this->Modified();
}

bool xq_MitkGrid::ApplySetMesh(xq_Grid* mesh, unsigned int t)
{
    if (t >= m_MeshList.size()) return false;
    m_MeshList[t].reset(mesh);
    return true;
}

bool xq_MitkGrid::ApplyGenerateMesh(unsigned int t)
{
    if (t >= m_MeshList.size() || !m_MeshList[t]) return false;
    m_MeshList[t]->GenerateMesh();
    return true;
}

bool xq_MitkGrid::ApplyAdaptMesh(unsigned int t)
{
    if (t >= m_MeshList.size() || !m_MeshList[t]) return false;
    m_MeshList[t]->AdaptMesh("ErrorMetric");
    return true;
}

bool xq_MitkGrid::IsEmptyTimeStep(unsigned int t) const
{
    return (t >= m_MeshList.size()) || !HasValidVolumeMesh(m_MeshList[t].get());
}

xq_Grid* xq_MitkGrid::GetMesh(unsigned int t) const
{
    return (t < m_MeshList.size()) ? m_MeshList[t].get() : nullptr;
}

void xq_MitkGrid::SetMesh(xq_Grid* mesh, unsigned int t)
{
    if (t >= m_MeshList.size())
        m_MeshList.resize(t + 1);

    m_MeshList[t].reset(mesh);
    this->Modified();
}

xq_VascularGeometry* xq_MitkGrid::GetModelElement() const
{
    return (!m_MeshList.empty() && m_MeshList[0]) ? m_MeshList[0]->GetModelElement() : nullptr;
}

void xq_MitkGrid::ClearData()
{
    m_MeshList.clear();
}

void xq_MitkGrid::InitializeEmpty()
{
    m_MeshList.clear();
    m_MeshList.resize(kInitialTimeSteps);
    Superclass::InitializeTimeGeometry(kInitialTimeSteps);
    this->Modified();
}
