#include "xq_ModelQuality.h"

#include <vtkFeatureEdges.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkMassProperties.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkNew.h>
#include <vtkTriangle.h>

#include <algorithm>
#include <cmath>

xq_ModelQualityReport xq_ModelQuality::Evaluate(vtkPolyData* polyData)
{
    xq_ModelQualityReport report;

    if (!polyData)
    {
        report.errors.push_back("Null polydata input.");
        return report;
    }

    report.numberOfPoints = static_cast<int>(polyData->GetNumberOfPoints());
    report.numberOfCells = static_cast<int>(polyData->GetNumberOfCells());

    if (report.numberOfPoints == 0)
    {
        report.errors.push_back("Model has no points.");
        return report;
    }

    for (vtkIdType cellId = 0; cellId < polyData->GetNumberOfCells(); ++cellId)
    {
        vtkCell* cell = polyData->GetCell(cellId);
        if (!cell || cell->GetNumberOfPoints() < 3)
        {
            ++report.degenerateCellCount;
            continue;
        }

        double p0[3];
        cell->GetPoints()->GetPoint(0, p0);
        double area = 0.0;
        for (vtkIdType i = 1; i + 1 < cell->GetNumberOfPoints(); ++i)
        {
            double p1[3], p2[3];
            cell->GetPoints()->GetPoint(i, p1);
            cell->GetPoints()->GetPoint(i + 1, p2);
            area += vtkTriangle::TriangleArea(p0, p1, p2);
        }
        if (area <= 1e-12 || std::isnan(area))
            ++report.degenerateCellCount;
    }

    // Boundary and non-manifold edges
    vtkNew<vtkFeatureEdges> featureEdges;
    featureEdges->SetInputData(polyData);
    featureEdges->BoundaryEdgesOn();
    featureEdges->FeatureEdgesOff();
    featureEdges->NonManifoldEdgesOn();
    featureEdges->ManifoldEdgesOff();
    featureEdges->Update();

    auto* edgeOutput = featureEdges->GetOutput();
    if (edgeOutput)
    {
        report.boundaryEdges = static_cast<int>(edgeOutput->GetNumberOfCells());

        // Non-manifold: rerun with only non-manifold
        vtkNew<vtkFeatureEdges> nme;
        nme->SetInputData(polyData);
        nme->BoundaryEdgesOff();
        nme->NonManifoldEdgesOn();
        nme->ManifoldEdgesOff();
        nme->FeatureEdgesOff();
        nme->Update();
        report.nonManifoldEdges = static_cast<int>(nme->GetOutput()->GetNumberOfCells());
    }

    // Connected components
    vtkNew<vtkPolyDataConnectivityFilter> connectivity;
    connectivity->SetInputData(polyData);
    connectivity->SetExtractionModeToAllRegions();
    connectivity->Update();
    report.connectedComponents =
        connectivity->GetNumberOfExtractedRegions();

    // FaceIds check
    auto* faceIds = polyData->GetCellData()->GetArray("FaceIds");
    report.hasFaceIds = (faceIds != nullptr);
    if (report.hasFaceIds)
    {
        int maxFaceId = -1;
        for (vtkIdType i = 0; i < faceIds->GetNumberOfTuples(); ++i)
            maxFaceId = std::max(maxFaceId, static_cast<int>(faceIds->GetTuple1(i)));
        report.faceCount = maxFaceId + 1;
    }

    // Determine overall status
    report.ok = true;
    if (report.boundaryEdges > 0)
        report.warnings.push_back(
            "Model has boundary edges (" + std::to_string(report.boundaryEdges) +
            "). Surface may not be closed.");
    if (report.nonManifoldEdges > 0)
    {
        report.ok = false;
        report.errors.push_back(
            "Model has non-manifold edges (" +
            std::to_string(report.nonManifoldEdges) + ").");
    }
    if (report.degenerateCellCount > 0)
    {
        report.ok = false;
        report.errors.push_back(
            "Model has degenerate cells (" +
            std::to_string(report.degenerateCellCount) + ").");
    }
    if (report.connectedComponents > 1)
        report.warnings.push_back(
            "Model has " + std::to_string(report.connectedComponents) +
            " connected components.");
    if (!report.hasFaceIds)
        report.warnings.push_back(
            "No FaceIds array found. Face-based operations may not work.");

    return report;
}
