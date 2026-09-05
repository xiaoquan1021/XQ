#include "vtkvmtkPolyDataCenterlines.h"

#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDecimatePro.h>
#include <vtkFeatureEdges.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPointLocator.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#include <vtkXMLPolyDataReader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef XQ_AUDIT_REAL_SURFACE
#error XQ_AUDIT_REAL_SURFACE must name the audited real vascular surface
#endif

namespace {

using Point = std::array<double, 3>;

std::map<int, Point> capCentroids(vtkPolyData* surface)
{
    std::map<int, Point> sums;
    std::map<int, std::set<vtkIdType>> capPointIds;
    vtkDataArray* capIds = surface->GetCellData()->GetArray("CapID");
    if (capIds == nullptr) {
        return sums;
    }

    for (vtkIdType cellId = 0; cellId < surface->GetNumberOfCells(); ++cellId) {
        const int capId = static_cast<int>(std::llround(capIds->GetComponent(cellId, 0)));
        if (capId <= 0) {
            continue;
        }
        vtkCell* cell = surface->GetCell(cellId);
        for (vtkIdType local = 0; local < cell->GetNumberOfPoints(); ++local) {
            capPointIds[capId].insert(cell->GetPointId(local));
        }
    }

    for (const auto& entry : capPointIds) {
        Point centroid = {0.0, 0.0, 0.0};
        for (vtkIdType pointId : entry.second) {
            double point[3] = {};
            surface->GetPoint(pointId, point);
            for (int axis = 0; axis < 3; ++axis) {
                centroid[axis] += point[axis];
            }
        }
        for (double& value : centroid) {
            value /= static_cast<double>(entry.second.size());
        }
        sums.emplace(entry.first, centroid);
    }
    return sums;
}

double distance(const double* lhs, const Point& rhs)
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

int main()
{
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(XQ_AUDIT_REAL_SURFACE);
    reader->Update();
    vtkPolyData* original = reader->GetOutput();
    if (original == nullptr || original->GetNumberOfPoints() < 1000
        || original->GetNumberOfPolys() < 1000) {
        std::cerr << "real vascular surface did not load\n";
        return EXIT_FAILURE;
    }

    const auto centroids = capCentroids(original);
    if (centroids.size() < 2) {
        std::cerr << "real vascular surface has fewer than two labeled caps\n";
        return EXIT_FAILURE;
    }

    vtkNew<vtkTriangleFilter> triangulate;
    triangulate->SetInputData(original);
    vtkNew<vtkDecimatePro> decimate;
    decimate->SetInputConnection(triangulate->GetOutputPort());
    decimate->SetTargetReduction(0.92);
    decimate->PreserveTopologyOn();
    decimate->BoundaryVertexDeletionOff();
    decimate->SplittingOff();
    decimate->Update();
    vtkPolyData* surface = decimate->GetOutput();
    if (surface == nullptr || surface->GetNumberOfPoints() < 500) {
        std::cerr << "real vascular surface decimation was degenerate\n";
        return EXIT_FAILURE;
    }

    vtkNew<vtkFeatureEdges> boundaryEdges;
    boundaryEdges->SetInputData(surface);
    boundaryEdges->BoundaryEdgesOn();
    boundaryEdges->FeatureEdgesOff();
    boundaryEdges->ManifoldEdgesOff();
    boundaryEdges->NonManifoldEdgesOff();
    boundaryEdges->Update();
    if (boundaryEdges->GetOutput()->GetNumberOfCells() != 0) {
        std::cerr << "real vascular surface is not closed after decimation\n";
        return EXIT_FAILURE;
    }

    vtkNew<vtkPointLocator> locator;
    locator->SetDataSet(surface);
    locator->BuildLocator();

    auto cap = centroids.begin();
    const Point sourceCentroid = cap->second;
    ++cap;
    const Point targetCentroid = cap->second;
    vtkNew<vtkIdList> sourceIds;
    vtkNew<vtkIdList> targetIds;
    sourceIds->InsertNextId(locator->FindClosestPoint(sourceCentroid.data()));
    targetIds->InsertNextId(locator->FindClosestPoint(targetCentroid.data()));
    if (sourceIds->GetId(0) == targetIds->GetId(0)) {
        std::cerr << "cap seeds collapsed to one surface point\n";
        return EXIT_FAILURE;
    }

    vtkNew<vtkvmtkPolyDataCenterlines> centerlines;
    centerlines->SetInputData(surface);
    centerlines->SetSourceSeedIds(sourceIds);
    centerlines->SetTargetSeedIds(targetIds);
    centerlines->SetRadiusArrayName("MaximumInscribedSphereRadius");
    centerlines->SetCostFunction("1/R");
    centerlines->SetAppendEndPointsToCenterlines(1);
    centerlines->SetCenterlineResampling(1);
    centerlines->SetResamplingStepLength(0.5);
    centerlines->Update();

    vtkPolyData* output = centerlines->GetOutput();
    vtkDataArray* radius = output == nullptr
        ? nullptr
        : output->GetPointData()->GetArray("MaximumInscribedSphereRadius");
    if (output == nullptr || output->GetNumberOfLines() == 0
        || output->GetNumberOfPoints() < 2 || radius == nullptr) {
        std::cerr << "vtkvmtk did not produce a radius-bearing centerline\n";
        return EXIT_FAILURE;
    }

    double radiusRange[2] = {};
    radius->GetRange(radiusRange);
    if (!std::isfinite(radiusRange[0]) || !std::isfinite(radiusRange[1])
        || radiusRange[0] <= 0.0) {
        std::cerr << "vtkvmtk centerline radius is invalid\n";
        return EXIT_FAILURE;
    }

    double minimumSourceDistance = std::numeric_limits<double>::max();
    double minimumTargetDistance = std::numeric_limits<double>::max();
    for (vtkIdType pointId = 0; pointId < output->GetNumberOfPoints(); ++pointId) {
        double point[3] = {};
        output->GetPoint(pointId, point);
        minimumSourceDistance = std::min(minimumSourceDistance, distance(point, sourceCentroid));
        minimumTargetDistance = std::min(minimumTargetDistance, distance(point, targetCentroid));
    }
    const double surfaceLength = surface->GetLength();
    const double endpointTolerance = std::max(1.0, surfaceLength * 0.03);
    if (minimumSourceDistance > endpointTolerance || minimumTargetDistance > endpointTolerance) {
        std::cerr << "centerline does not reach both real cap regions"
                  << " source_distance=" << minimumSourceDistance
                  << " target_distance=" << minimumTargetDistance
                  << " tolerance=" << endpointTolerance << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "PASS vtkvmtk_centerline_probe"
              << " input_points=" << original->GetNumberOfPoints()
              << " probe_points=" << surface->GetNumberOfPoints()
              << " centerline_points=" << output->GetNumberOfPoints()
              << " centerline_lines=" << output->GetNumberOfLines()
              << " radius_range=" << radiusRange[0] << ',' << radiusRange[1]
              << " cap_distance=" << minimumSourceDistance << ',' << minimumTargetDistance << '\n';
    return EXIT_SUCCESS;
}
