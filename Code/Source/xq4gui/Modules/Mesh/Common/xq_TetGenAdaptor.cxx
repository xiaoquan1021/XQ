#include "xq_TetGenAdaptor.h"
#include "xq_Grid.h"

#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{

vtkDataArray* FindErrorArray(vtkUnstructuredGrid* mesh, const std::string& metricName)
{
    const std::function<vtkDataArray*(vtkUnstructuredGrid*)> sources[] = {
        [&](vtkUnstructuredGrid* m) { return m->GetCellData()->GetArray(metricName.c_str()); },
        [&](vtkUnstructuredGrid* m) { return m->GetPointData()->GetArray(metricName.c_str()); },
    };

    for (const auto& src : sources)
    {
        if (auto* arr = src(mesh))
            return arr;
    }
    return nullptr;
}

double ComputePeakError(vtkDataArray* errorArray)
{
    double peak = 0.0;
    const vtkIdType numTuples = errorArray->GetNumberOfTuples();
    for (vtkIdType i = 0; i < numTuples; ++i)
        peak = std::max(peak, std::abs(errorArray->GetTuple1(i)));
    return peak;
}

bool ValidateVolumeMesh(vtkUnstructuredGrid* mesh)
{
    return mesh && mesh->GetNumberOfCells() > 0;
}

} // anonymous namespace

bool xq_TetGenAdaptor::Adapt()
{
    if (!m_InputMesh || m_ErrorMetricName.empty())
        return false;

    auto* volumeMesh = m_InputMesh->GetVolumeMesh();
    if (!ValidateVolumeMesh(volumeMesh))
        return false;

    for (int iter = 0; iter < m_MaxIterations; ++iter)
    {
        auto* errorArray = FindErrorArray(volumeMesh, m_ErrorMetricName);
        if (!errorArray)
            return false;

        if (ComputePeakError(errorArray) <= m_TargetError)
            return true;

        if (!m_InputMesh->AdaptMesh(m_ErrorMetricName))
            return false;

        volumeMesh = m_InputMesh->GetVolumeMesh();
        if (!ValidateVolumeMesh(volumeMesh))
            return false;
    }

    return true;
}

void xq_TetGenAdaptor::SetMaxRefinementRatio(double ratio) { m_MaxRefinementRatio = ratio; }
double xq_TetGenAdaptor::GetMaxRefinementRatio() const { return m_MaxRefinementRatio; }

void xq_TetGenAdaptor::SetMaxIterations(int iterations) { m_MaxIterations = iterations; }
int xq_TetGenAdaptor::GetMaxIterations() const { return m_MaxIterations; }
