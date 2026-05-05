#include "xq_SolidModeler.h"

#include <vtkAppendPolyData.h>
#include <vtkBooleanOperationPolyDataFilter.h>
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkFeatureEdges.h>
#include <vtkFillHolesFilter.h>
#include <vtkIdList.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkStripper.h>
#include <vtkTriangleFilter.h>

#include <utility>
#include <vector>

namespace
{

vtkSmartPointer<vtkPolyData> triangulate(vtkSmartPointer<vtkPolyData> input)
{
    auto tri = vtkSmartPointer<vtkTriangleFilter>::New();
    tri->SetInputData(input);
    tri->Update();
    auto clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputConnection(tri->GetOutputPort());
    clean->Update();
    return clean->GetOutput();
}

// Build a single cap by fan triangulation from the loop centroid.
// Inserts new points/cells directly into `outPoints` / `outPolys`.
// Returns number of triangle cells added.
int TriangulateLoopInto(
    vtkPoints* srcPoints, vtkIdList* loop,
    vtkPoints* outPoints, vtkCellArray* outPolys)
{
    const vtkIdType n = loop->GetNumberOfIds();
    if (n < 3)
        return 0;

    double centroid[3] = {0.0, 0.0, 0.0};
    std::vector<vtkIdType> newIds;
    newIds.reserve(n + 1);

    for (vtkIdType i = 0; i < n; ++i)
    {
        double p[3];
        srcPoints->GetPoint(loop->GetId(i), p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
        newIds.push_back(outPoints->InsertNextPoint(p));
    }
    centroid[0] /= static_cast<double>(n);
    centroid[1] /= static_cast<double>(n);
    centroid[2] /= static_cast<double>(n);
    const vtkIdType cId = outPoints->InsertNextPoint(centroid);

    int cells = 0;
    for (vtkIdType i = 0; i < n; ++i)
    {
        vtkIdType tri[3] = { cId, newIds[i], newIds[(i + 1) % n] };
        outPolys->InsertNextCell(3, tri);
        ++cells;
    }
    return cells;
}

} // namespace

xq_SolidModeler::BuildResult
xq_VtkSolidModeler::Build(const BuildRequest& request)
{
    BuildResult result;

    if (request.loftedWalls.empty())
    {
        result.diagnostic = "VtkSolidModeler: no lofted walls supplied.";
        return result;
    }

    // 1) Concatenate the per-group lofts.
    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    int nonEmpty = 0;
    for (const auto& w : request.loftedWalls)
    {
        if (w && w->GetNumberOfPoints() > 0)
        {
            append->AddInputData(w);
            ++nonEmpty;
        }
    }
    if (nonEmpty == 0)
    {
        result.diagnostic = "VtkSolidModeler: all lofted walls are empty.";
        return result;
    }
    append->Update();

    // 2) Clean + triangulate.
    auto clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputConnection(append->GetOutputPort());
    clean->Update();
    auto wall = triangulate(clean->GetOutput());
    const int wallCellCount = static_cast<int>(wall->GetNumberOfCells());

    // 3) Extract boundary loops and cap each one as its own face.
    auto combined = vtkSmartPointer<vtkPolyData>::New();
    auto combinedPoints = vtkSmartPointer<vtkPoints>::New();
    auto combinedPolys = vtkSmartPointer<vtkCellArray>::New();

    // Copy the wall first (preserve cell order 0..wallCellCount-1).
    combinedPoints->DeepCopy(wall->GetPoints());
    auto* wallPolys = wall->GetPolys();
    wallPolys->InitTraversal();
    vtkIdType npts = 0;
    const vtkIdType* pts = nullptr;
    while (wallPolys->GetNextCell(npts, pts))
        combinedPolys->InsertNextCell(npts, pts);

    std::vector<int> perCellFaceId(wallCellCount, 0);
    int capCount = 0;

    if (request.capEnds)
    {
        auto edges = vtkSmartPointer<vtkFeatureEdges>::New();
        edges->SetInputData(wall);
        edges->BoundaryEdgesOn();
        edges->FeatureEdgesOff();
        edges->ManifoldEdgesOff();
        edges->NonManifoldEdgesOff();
        edges->Update();

        auto strip = vtkSmartPointer<vtkStripper>::New();
        strip->SetInputConnection(edges->GetOutputPort());
        strip->JoinContiguousSegmentsOn();
        strip->Update();

        auto loops = strip->GetOutput();
        auto* loopLines = loops->GetLines();
        loopLines->InitTraversal();

        // XQ fix: each closed boundary loop becomes ITS OWN face id, rather
        // than splitting a single flat "cap cell count / capCount" band
        // (which was the SV-derived bug in xq_ModelPipeline).
        while (loopLines->GetNextCell(npts, pts))
        {
            if (npts < 3)
                continue;
            auto loopIds = vtkSmartPointer<vtkIdList>::New();
            for (vtkIdType i = 0; i < npts; ++i)
                loopIds->InsertNextId(pts[i]);

            // Append this cap's points into the combined point set with an
            // offset so we don't collide with wall point ids.
            const vtkIdType pointOffset = combinedPoints->GetNumberOfPoints();
            auto localPolys = vtkSmartPointer<vtkCellArray>::New();
            auto tmpPoints = vtkSmartPointer<vtkPoints>::New();
            const int added = TriangulateLoopInto(
                loops->GetPoints(), loopIds, tmpPoints, localPolys);
            if (added == 0)
                continue;

            // Merge tmp points into combined.
            for (vtkIdType k = 0; k < tmpPoints->GetNumberOfPoints(); ++k)
            {
                double p[3];
                tmpPoints->GetPoint(k, p);
                combinedPoints->InsertNextPoint(p);
            }
            // Merge tmp cells with pointOffset + record face id.
            localPolys->InitTraversal();
            vtkIdType lpn = 0;
            const vtkIdType* lptr = nullptr;
            const int faceId = 1 + capCount;
            while (localPolys->GetNextCell(lpn, lptr))
            {
                std::vector<vtkIdType> shifted(lpn);
                for (vtkIdType k = 0; k < lpn; ++k)
                    shifted[k] = lptr[k] + pointOffset;
                combinedPolys->InsertNextCell(lpn, shifted.data());
                perCellFaceId.push_back(faceId);
            }
            ++capCount;
        }
    }

    combined->SetPoints(combinedPoints);
    combined->SetPolys(combinedPolys);
    auto finalMesh = triangulate(combined);

    result.ok = true;
    result.surface = finalMesh;
    result.faceIdsPerCell = std::move(perCellFaceId);
    result.wallCellCount = wallCellCount;
    result.capCount = capCount;

    // Blend: optional smoothing pass, scope-limited so it doesn't eat the
    // cap planarity. Kept deliberately conservative.
    if (request.blendRadius > 0.0)
    {
        auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
        smoother->SetInputData(finalMesh);
        smoother->SetNumberOfIterations(5);
        smoother->SetRelaxationFactor(std::min(0.1, request.blendRadius));
        smoother->FeatureEdgeSmoothingOff();
        smoother->BoundarySmoothingOff();
        smoother->Update();
        result.surface = smoother->GetOutput();
    }

    return result;
}

xq_SolidModeler::BuildResult
xq_OccSolidModeler::Build(const BuildRequest& request)
{
    // TODO(xq-occ): implement BRep lofting via xq_OCCTGeometry + BRepAlgoAPI_Fuse
    // for booleans and BRepFilletAPI_MakeFillet for Blend. Until wired, delegate
    // to VTK so the pipeline still produces a usable Model.
    xq_VtkSolidModeler fallback;
    auto r = fallback.Build(request);
    if (r.ok)
        r.diagnostic = "OCCT modeler not yet linked; returned VTK fallback surface.";
    return r;
}

std::unique_ptr<xq_SolidModeler> CreateSolidModeler(std::string_view name)
{
    if (name == "occt")
        return std::make_unique<xq_OccSolidModeler>();
    return std::make_unique<xq_VtkSolidModeler>();
}
