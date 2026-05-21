#include "xq_MeshQuality.h"

#include <vtkUnstructuredGrid.h>
#include <vtkCell.h>
#include <vtkPoints.h>
#include <vtkTetra.h>
#include <vtkNew.h>

#include <algorithm>
#include <cmath>

xq_MeshQualityReport xq_MeshQuality::Evaluate(vtkUnstructuredGrid* grid)
{
    xq_MeshQualityReport report;

    if (!grid)
    {
        report.errors.push_back("Null grid input.");
        return report;
    }

    report.numberOfPoints = static_cast<int>(grid->GetNumberOfPoints());
    report.numberOfCells = static_cast<int>(grid->GetNumberOfCells());

    if (report.numberOfCells == 0)
    {
        report.errors.push_back("Mesh has no cells.");
        return report;
    }

    report.minVolume = 1e30;
    report.maxVolume = -1e30;
    bool hasTetra = false;

    for (vtkIdType i = 0; i < grid->GetNumberOfCells(); ++i)
    {
        vtkCell* cell = grid->GetCell(i);
        if (!cell) continue;

        if (cell->GetCellType() == VTK_TETRA && cell->GetNumberOfPoints() >= 4)
        {
            hasTetra = true;
            double p0[3], p1[3], p2[3], p3[3];
            cell->GetPoints()->GetPoint(0, p0);
            cell->GetPoints()->GetPoint(1, p1);
            cell->GetPoints()->GetPoint(2, p2);
            cell->GetPoints()->GetPoint(3, p3);
            double v = std::abs(vtkTetra::ComputeVolume(p0, p1, p2, p3));
            report.minVolume = std::min(report.minVolume, v);
            report.maxVolume = std::max(report.maxVolume, v);
            if (v <= 0.0 || std::isnan(v))
                ++report.negativeVolumeCount;
        }
    }

    if (!hasTetra)
    {
        report.minVolume = 0.0;
        report.maxVolume = 0.0;
    }

    // Compute edge-length statistics (all cell edges)
    report.minEdgeLength = 1e30;
    report.maxEdgeLength = -1e30;
    double sumEdgeLength = 0.0;
    vtkIdType totalEdges = 0;

    for (vtkIdType i = 0; i < grid->GetNumberOfCells(); ++i)
    {
        vtkCell* cell = grid->GetCell(i);
        if (!cell) continue;
        int nEdges = cell->GetNumberOfEdges();
        for (int e = 0; e < nEdges; ++e)
        {
            vtkCell* edge = cell->GetEdge(e);
            if (!edge || edge->GetNumberOfPoints() < 2) continue;
            double p0[3], p1[3];
            grid->GetPoint(edge->GetPointId(0), p0);
            grid->GetPoint(edge->GetPointId(1), p1);
            double dx = p1[0] - p0[0];
            double dy = p1[1] - p0[1];
            double dz = p1[2] - p0[2];
            double len = std::sqrt(dx*dx + dy*dy + dz*dz);
            sumEdgeLength += len;
            ++totalEdges;
            if (len < report.minEdgeLength) report.minEdgeLength = len;
            if (len > report.maxEdgeLength) report.maxEdgeLength = len;
        }
    }

    if (totalEdges > 0)
        report.meanEdgeLength = sumEdgeLength / static_cast<double>(totalEdges);
    else
        report.minEdgeLength = report.maxEdgeLength = 0.0;

    report.ok = true;
    if (report.negativeVolumeCount > 0)
    {
        report.ok = false;
        report.errors.push_back(
            "Mesh contains " + std::to_string(report.negativeVolumeCount) +
            " inverted/zero-volume elements.");
    }

    if (report.numberOfPoints < 10)
        report.warnings.push_back("Mesh has very few points (<10).");

    return report;
}
