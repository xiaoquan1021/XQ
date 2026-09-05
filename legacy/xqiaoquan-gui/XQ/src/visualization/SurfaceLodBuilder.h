#ifndef XQ_VISUALIZATION_SURFACE_LOD_BUILDER_H
#define XQ_VISUALIZATION_SURFACE_LOD_BUILDER_H

#include "core/source/ReadSpan.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <future>
#include <vector>

namespace xq {

// LOD level construction for a triangle surface (M9b-B). Given the full-res
// vtkPolyData (level 0) plus its per-triangle faceId span, produces decimated
// levels. Because vtkQuadricDecimation does NOT carry cell data (verified
// against VTK 9.3 headers), the medium level is built by splitting the surface
// into per-faceId-region submeshes, decimating each independently, and merging
// -- so every output triangle still belongs to exactly one original faceId
// region and the cell->faceId map can be rebuilt. The far level uses
// vtkQuadricClustering (changes topology, drops faceId) for pure distant display.
//
// All VTK lives here (visualization); io/core never see vtk*.

// Output cellId -> faceId map for one level. valid == false means the level
// dropped faceId (far level): no per-cell faceId, mapper goes flat-shaded.
struct CellFaceIdMap {
    std::vector<int> cellToFaceId; // length == level triangle count when valid
    bool valid = false;
};

struct LodLevel {
    vtkSmartPointer<vtkPolyData> poly;
    CellFaceIdMap faceMap;
    long long pointCount = 0;
    long long triangleCount = 0;
};

// Three levels: [0] full, [1] medium (faceId preserved), [2] far (flat).
struct LodLevels {
    LodLevel level[3];
    bool ok = false;
};

class SurfaceLodBuilder {
public:
    // Build all three levels synchronously (deterministic; used by headless
    // tests and as the body of any async wrapper). `full` is the level-0
    // vtkPolyData (its cell scalars are the faceId float array); `faceIds` is the
    // parallel per-triangle faceId span (same order as full's cells). A region
    // whose triangle count is below `minRegionTris` is kept un-decimated (R3:
    // avoid degenerate decimation of tiny regions).
    static LodLevels buildSync(vtkPolyData* full, const ReadSpan<int>& faceIds,
                               double mediumReduction = 0.75,
                               int farDivisions = 16,
                               std::size_t minRegionTris = 8);

    // Background-thread variant (decimation of a 20M-triangle mesh would stall
    // the main thread; the parent task forbids that). The full surface is deep-
    // copied and the faceId span is copied up front, so the returned future owns
    // its inputs and is safe even if the caller's lease/source is released right
    // after this returns. The future yields the same LodLevels as buildSync. VTK
    // objects are not thread-safe, so the worker only PRODUCES vtkPolyData on its
    // own deep copy; the caller must assemble mappers/actors on the main thread.
    static std::future<LodLevels> buildAsync(vtkPolyData* full,
                                             const ReadSpan<int>& faceIds,
                                             double mediumReduction = 0.75,
                                             int farDivisions = 16,
                                             std::size_t minRegionTris = 8);
};

} // namespace xq

#endif // XQ_VISUALIZATION_SURFACE_LOD_BUILDER_H
