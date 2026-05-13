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
    return AdaptWithResult().ok;
}

xq_AdaptResult xq_TetGenAdaptor::AdaptWithResult()
{
    xq_AdaptResult result;
    if (!m_InputMesh)
    {
        result.diagnostic = "Input mesh is null.";
        return result;
    }
    if (m_ErrorMetricName.empty())
    {
        result.diagnostic = "Missing error metric name.";
        PopulateStats(m_InputMesh, m_InputMesh, result);
        return result;
    }

    auto* volumeMesh = m_InputMesh->GetVolumeMesh();
    if (!ValidateVolumeMesh(volumeMesh))
    {
        result.diagnostic = "Input mesh has no valid volume mesh.";
        PopulateStats(m_InputMesh, m_InputMesh, result);
        return result;
    }

    const int initialCells = std::max(1, m_InputMesh->GetNumberOfElements());
    PopulateStats(m_InputMesh, m_InputMesh, result);

    for (int iter = 0; iter < m_MaxIterations; ++iter)
    {
        result.iterationCount = iter;
        auto* errorArray = FindErrorArray(volumeMesh, m_ErrorMetricName);
        if (!errorArray)
        {
            result.diagnostic = "Error metric array not found: " + m_ErrorMetricName;
            PopulateStats(m_InputMesh, m_InputMesh, result);
            return result;
        }

        result.peakError = ComputePeakError(errorArray);
        if (result.peakError <= m_TargetError)
        {
            result.ok = true;
            result.iterationCount = iter;
            PopulateStats(m_InputMesh, m_InputMesh, result);
            return result;
        }

        if (!m_InputMesh->AdaptMesh(m_ErrorMetricName))
        {
            result.diagnostic =
                "AdaptMesh failed for metric: " + m_ErrorMetricName +
                ". Current XQ adaptive meshing is a basic in-process loop, "
                "not a full solution-driven TetGen/MMG remeshing workflow.";
            result.iterationCount = iter + 1;
            PopulateStats(m_InputMesh, m_InputMesh, result);
            return result;
        }

        const int currentCells = std::max(0, m_InputMesh->GetNumberOfElements());
        if (currentCells > static_cast<int>(std::ceil(initialCells * m_MaxRefinementRatio)))
        {
            result.diagnostic = "Refinement ratio exceeded.";
            result.iterationCount = iter + 1;
            PopulateStats(m_InputMesh, m_InputMesh, result);
            return result;
        }

        volumeMesh = m_InputMesh->GetVolumeMesh();
        if (!ValidateVolumeMesh(volumeMesh))
        {
            result.diagnostic = "Adapted mesh has no valid volume mesh.";
            result.iterationCount = iter + 1;
            PopulateStats(m_InputMesh, m_InputMesh, result);
            return result;
        }
    }

    result.diagnostic =
        "Maximum iterations reached before target error. Current XQ adaptive "
        "meshing is a basic in-process loop, not a full solution-driven "
        "TetGen/MMG remeshing workflow.";
    PopulateStats(m_InputMesh, m_InputMesh, result);
    return result;
}

void xq_TetGenAdaptor::SetMaxRefinementRatio(double ratio) { m_MaxRefinementRatio = ratio; }
double xq_TetGenAdaptor::GetMaxRefinementRatio() const { return m_MaxRefinementRatio; }

void xq_TetGenAdaptor::SetMaxIterations(int iterations) { m_MaxIterations = iterations; }
int xq_TetGenAdaptor::GetMaxIterations() const { return m_MaxIterations; }
