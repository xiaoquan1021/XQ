#include "xq_TetGenGrid.h"
#include "xq_VascularGeometry.h"

#include <vtkCleanPolyData.h>
#include <vtkDelaunay3D.h>
#include <vtkGeometryFilter.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkThreshold.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkTriangleFilter.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

xq_TetGenGrid::xq_TetGenGrid()
{
    m_Type = "TetGen";
}

bool xq_TetGenGrid::GenerateMesh()
{
    if (!m_ModelElement)
    {
        return false;
    }

    auto surfaceInput = m_ModelElement->GetWholeVtkPolyData();
    if (!surfaceInput || surfaceInput->GetNumberOfPoints() == 0)
    {
        return false;
    }

    // Clean input: remove duplicate points and triangulate so that
    // vtkDelaunay3D receives well-formed geometry even when the lofted
    // surface is thin or nearly coplanar.
    auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputData(surfaceInput);
    cleaner->Update();

    auto triangulator = vtkSmartPointer<vtkTriangleFilter>::New();
    triangulator->SetInputConnection(cleaner->GetOutputPort());
    triangulator->Update();

    auto cleaned = triangulator->GetOutput();
    if (!cleaned || cleaned->GetNumberOfPoints() < 4 || cleaned->GetNumberOfCells() == 0)
    {
        return false;
    }

    // BoundingTriangulationOff is safe; BoundingTriangulationOn crashes
    // VTK for degenerate inputs (thin cylinders, near-coplanar points).
    auto delaunay = vtkSmartPointer<vtkDelaunay3D>::New();
    delaunay->SetInputData(cleaned);
    delaunay->SetTolerance(m_Params.globalEdgeSize * 0.01);
    delaunay->SetAlpha(0.0);
    delaunay->BoundingTriangulationOff();
    delaunay->Update();

    auto* output = delaunay->GetOutput();
    if (!output || output->GetNumberOfCells() == 0)
    {
        return false;
    }

    TetGenMeshHandle resultHandle;
    auto resultGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    resultGrid->DeepCopy(output);
    resultHandle.reset(std::move(resultGrid));

    m_VolumeMesh->DeepCopy(resultHandle.get());

    auto geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geomFilter->SetInputData(m_VolumeMesh);
    geomFilter->Update();
    m_SurfaceMesh->DeepCopy(geomFilter->GetOutput());

    return true;
}

bool xq_TetGenGrid::AdaptMesh(std::string_view errorMetricArrayName)
{
    if (!m_VolumeMesh || m_VolumeMesh->GetNumberOfCells() == 0)
    {
        return false;
    }

    const std::string metricName(errorMetricArrayName);
    auto* errorArray = m_VolumeMesh->GetCellData()->GetArray(metricName.c_str());
    if (!errorArray)
    {
        errorArray = m_VolumeMesh->GetPointData()->GetArray(metricName.c_str());
    }
    if (!errorArray)
    {
        return false;
    }

    const auto numTuples = errorArray->GetNumberOfTuples();

    // Accumulate total absolute error via STL algorithm
    double totalError = 0.0;
    for (vtkIdType i = 0; i < numTuples; ++i)
    {
        totalError += std::abs(errorArray->GetTuple1(i));
    }
    const double meanError = totalError / static_cast<double>(numTuples);

    if (meanError < m_Params.globalEdgeSize * 0.01)
    {
        return true;
    }

    // Build per-cell target size field
    auto sizeField = vtkSmartPointer<vtkDoubleArray>::New();
    sizeField->SetName("TargetEdgeSize");
    sizeField->SetNumberOfTuples(numTuples);

    const double refinedSize = m_Params.globalEdgeSize * 0.5;
    for (vtkIdType i = 0; i < numTuples; ++i)
    {
        const double err = std::abs(errorArray->GetTuple1(i));
        sizeField->SetValue(i, (err > meanError) ? refinedSize : m_Params.globalEdgeSize);
    }

    m_VolumeMesh->GetCellData()->AddArray(sizeField);

    // Re-generate with halved edge size, then restore original
    if (!m_ModelElement)
    {
        return false;
    }

    const double originalEdgeSize = m_Params.globalEdgeSize;
    m_Params.globalEdgeSize *= 0.5;
    const bool result = GenerateMesh();
    m_Params.globalEdgeSize = originalEdgeSize;
    return result;
}

void xq_TetGenGrid::SetQualityRatio(double ratio) { m_QualityRatio = ratio; }
double xq_TetGenGrid::GetQualityRatio() const { return m_QualityRatio; }

void xq_TetGenGrid::SetVolumeConstraint(double constraint) { m_VolumeConstraint = constraint; }
double xq_TetGenGrid::GetVolumeConstraint() const { return m_VolumeConstraint; }

void xq_TetGenGrid::SetCoarsenMesh(bool coarsen) { m_CoarsenMesh = coarsen; }
bool xq_TetGenGrid::GetCoarsenMesh() const { return m_CoarsenMesh; }

void xq_TetGenGrid::SetBoundaryLayerMesh(bool enable) { m_BoundaryLayerMesh = enable; }
bool xq_TetGenGrid::GetBoundaryLayerMesh() const { return m_BoundaryLayerMesh; }

void xq_TetGenGrid::SetBoundaryLayerCount(int count) { m_BoundaryLayerCount = count; }
int xq_TetGenGrid::GetBoundaryLayerCount() const { return m_BoundaryLayerCount; }

void xq_TetGenGrid::SetBoundaryLayerGrowthRate(double rate) { m_BoundaryLayerGrowthRate = rate; }
double xq_TetGenGrid::GetBoundaryLayerGrowthRate() const { return m_BoundaryLayerGrowthRate; }

void xq_TetGenGrid::SetBoundaryLayerFirstHeight(double h) { m_BoundaryLayerFirstHeight = h; }
double xq_TetGenGrid::GetBoundaryLayerFirstHeight() const { return m_BoundaryLayerFirstHeight; }
