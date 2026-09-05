#include "visualization/SurfaceLodBuilder.h"

#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkQuadricClustering.h>
#include <vtkQuadricDecimation.h>
#include <vtkTriangle.h>

#include <future>
#include <map>
#include <utility>
#include <vector>

namespace xq {
namespace {

// Build a standalone triangle vtkPolyData from a subset of `full`'s cells (the
// triangles whose indices are in `cellIds`). Points are compacted to only those
// the subset references, with a local remap, so each region is an independent
// decimable mesh. Cell data is NOT copied (the region's faceId is uniform and
// tracked separately).
vtkSmartPointer<vtkPolyData> extractRegion(vtkPolyData* full,
                                           const std::vector<vtkIdType>& cellIds)
{
    vtkPoints* srcPts = full->GetPoints();
    std::map<vtkIdType, vtkIdType> remap; // global point id -> local
    vtkNew<vtkPoints> pts;
    pts->SetDataTypeToDouble();
    vtkNew<vtkCellArray> tris;

    vtkNew<vtkIdList> cellPts;
    for (vtkIdType c : cellIds) {
        full->GetCellPoints(c, cellPts);
        if (cellPts->GetNumberOfIds() != 3) {
            continue;
        }
        vtkIdType local[3];
        for (int k = 0; k < 3; ++k) {
            const vtkIdType g = cellPts->GetId(k);
            auto it = remap.find(g);
            if (it == remap.end()) {
                double p[3];
                srcPts->GetPoint(g, p);
                const vtkIdType nl = pts->InsertNextPoint(p);
                it = remap.emplace(g, nl).first;
            }
            local[k] = it->second;
        }
        tris->InsertNextCell(3);
        tris->InsertCellPoint(local[0]);
        tris->InsertCellPoint(local[1]);
        tris->InsertCellPoint(local[2]);
    }

    vtkSmartPointer<vtkPolyData> region = vtkSmartPointer<vtkPolyData>::New();
    region->SetPoints(pts);
    region->SetPolys(tris);
    return region;
}

// Decimate a region; if it is too small, return it unchanged (R3).
vtkSmartPointer<vtkPolyData> decimateRegion(vtkPolyData* region, double reduction,
                                            std::size_t minRegionTris)
{
    if (static_cast<std::size_t>(region->GetNumberOfCells()) < minRegionTris
        || reduction <= 0.0) {
        return region;
    }
    vtkNew<vtkQuadricDecimation> deci;
    deci->SetInputData(region);
    deci->SetTargetReduction(reduction);
    deci->Update();
    vtkSmartPointer<vtkPolyData> out = vtkSmartPointer<vtkPolyData>::New();
    out->ShallowCopy(deci->GetOutput());
    return out;
}

} // namespace

LodLevels SurfaceLodBuilder::buildSync(vtkPolyData* full, const ReadSpan<int>& faceIds,
                                       double mediumReduction, int farDivisions,
                                       std::size_t minRegionTris)
{
    LodLevels levels;
    if (full == nullptr || full->GetNumberOfCells() == 0) {
        return levels;
    }

    // --- level 0: full (identity cell->faceId) ---
    levels.level[0].poly = full;
    levels.level[0].pointCount = full->GetNumberOfPoints();
    levels.level[0].triangleCount = full->GetNumberOfCells();
    levels.level[0].faceMap.valid = true;
    levels.level[0].faceMap.cellToFaceId.assign(faceIds.begin(), faceIds.end());

    // --- level 1: medium, per-faceId-region decimation (faceId preserved) ---
    // Group cell indices by faceId (region order = ascending faceId for
    // determinism). Each region decimated independently, then appended; the
    // merged cell->faceId is rebuilt from the appended region order/sizes.
    std::map<int, std::vector<vtkIdType>> regions;
    const vtkIdType nCells = full->GetNumberOfCells();
    for (vtkIdType c = 0; c < nCells; ++c) {
        const int fid = (static_cast<std::size_t>(c) < faceIds.size())
                            ? faceIds[static_cast<std::size_t>(c)]
                            : 0;
        regions[fid].push_back(c);
    }

    vtkNew<vtkAppendPolyData> append;
    std::vector<int> mediumFaceIds;
    bool anyRegion = false;
    for (const auto& kv : regions) {
        vtkSmartPointer<vtkPolyData> region = extractRegion(full, kv.second);
        vtkSmartPointer<vtkPolyData> deci =
            decimateRegion(region, mediumReduction, minRegionTris);
        const vtkIdType rc = deci->GetNumberOfCells();
        if (rc == 0) {
            continue;
        }
        append->AddInputData(deci);
        mediumFaceIds.insert(mediumFaceIds.end(), static_cast<std::size_t>(rc), kv.first);
        anyRegion = true;
    }

    if (anyRegion) {
        append->Update();
        vtkSmartPointer<vtkPolyData> merged = vtkSmartPointer<vtkPolyData>::New();
        merged->ShallowCopy(append->GetOutput());
        // Attach the rebuilt faceId cell scalars so the mapper can LUT-colour it.
        vtkNew<vtkFloatArray> faceScalars;
        faceScalars->SetName("faceId");
        faceScalars->SetNumberOfComponents(1);
        faceScalars->SetNumberOfTuples(static_cast<vtkIdType>(mediumFaceIds.size()));
        for (std::size_t i = 0; i < mediumFaceIds.size(); ++i) {
            faceScalars->SetValue(static_cast<vtkIdType>(i),
                                  static_cast<float>(mediumFaceIds[i]));
        }
        merged->GetCellData()->SetScalars(faceScalars);

        levels.level[1].poly = merged;
        levels.level[1].pointCount = merged->GetNumberOfPoints();
        levels.level[1].triangleCount = merged->GetNumberOfCells();
        levels.level[1].faceMap.valid = true;
        levels.level[1].faceMap.cellToFaceId = std::move(mediumFaceIds);

        // Honesty guard: per-region extraction duplicates points shared across
        // faceId regions, so when regions are highly fragmented (many small
        // scattered regions) the "medium" level can end up with MORE points than
        // full -- it is then not a reduction at all. Never present such a level
        // as an LOD: fall back to full so uploadedPointCount stays <= full.
        if (levels.level[1].pointCount >= levels.level[0].pointCount) {
            levels.level[1] = levels.level[0];
        }
    } else {
        // Fall back to full if nothing decimated (e.g. all regions tiny).
        levels.level[1] = levels.level[0];
    }

    // --- level 2: far, vtkQuadricClustering (topology changes, no faceId) ---
    vtkNew<vtkQuadricClustering> cluster;
    cluster->SetInputData(full);
    cluster->SetNumberOfDivisions(farDivisions, farDivisions, farDivisions);
    cluster->CopyCellDataOff();
    cluster->Update();
    vtkSmartPointer<vtkPolyData> far = vtkSmartPointer<vtkPolyData>::New();
    far->ShallowCopy(cluster->GetOutput());
    levels.level[2].poly = far;
    levels.level[2].pointCount = far->GetNumberOfPoints();
    levels.level[2].triangleCount = far->GetNumberOfCells();
    levels.level[2].faceMap.valid = false; // far drops faceId

    levels.ok = true;
    return levels;
}

std::future<LodLevels> SurfaceLodBuilder::buildAsync(vtkPolyData* full,
                                                     const ReadSpan<int>& faceIds,
                                                     double mediumReduction,
                                                     int farDivisions,
                                                     std::size_t minRegionTris)
{
    // Own the inputs before going async: deep-copy the surface and copy the
    // faceId span into a std::vector. VTK objects are not thread-safe and the
    // caller's lease/source may be released the instant this returns, so the
    // worker must not touch the caller's `full` or `faceIds` storage.
    vtkSmartPointer<vtkPolyData> fullCopy;
    if (full != nullptr) {
        fullCopy = vtkSmartPointer<vtkPolyData>::New();
        fullCopy->DeepCopy(full);
    }
    std::vector<int> faceCopy(faceIds.begin(), faceIds.end());

    return std::async(std::launch::async,
                      [fullCopy, faceCopy = std::move(faceCopy), mediumReduction,
                       farDivisions, minRegionTris]() -> LodLevels {
                          ReadSpan<int> span(faceCopy.data(), faceCopy.size());
                          return buildSync(fullCopy, span, mediumReduction,
                                           farDivisions, minRegionTris);
                      });
}

} // namespace xq
