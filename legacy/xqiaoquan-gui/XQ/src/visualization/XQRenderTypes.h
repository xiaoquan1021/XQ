#ifndef XQ_VISUALIZATION_RENDER_TYPES_H
#define XQ_VISUALIZATION_RENDER_TYPES_H

#include <cstddef>
#include <functional>

namespace xq {

// Per-add summary: an
// explicit boolean status plus the actor / point counts the call assembled, so
// headless tests can assert "this add produced one actor with N points" without
// touching any VTK type.
struct RenderStats {
    bool ok = false;
    int actorCount = 0;
    long long pointCount = 0;        // source point count (unchanged contract)
    long long uploadedPointCount = 0; // M9b-B: points actually uploaded to VTK;
                                      // == pointCount when LOD is off / level 0
    int chunkCount = 0;          // M9b-C: total chunks planned (0 for non-progressive adds)
    int completedChunkCount = 0; // M9b-C: chunks successfully uploaded so far
};

// LOD / decimation controls for a surface add (M9b-B). Off by default so the
// existing addSurface call path is unchanged. fixedLevel >= 0 forces a level
// (0 = full, 1 = medium with faceId, 2 = far flat-shaded); deterministic
// headless selection. budgetTriangles drives CPU-side level selection when
// fixedLevel < 0.
struct LodOptions {
    bool enabled = false;
    int fixedLevel = -1;
    long long budgetTriangles = 0;
    // Interactive (windowed) assembly: build all levels and mount them on a
    // vtkLODActor whose DesiredUpdateRate (driven by the QVTK interactor) picks
    // still/motion levels. Headless/offscreen paths leave this off and use the
    // deterministic fixedLevel/budget selection instead (vtkLODActor cannot be
    // measured headless -- no interactor drives AllocatedRenderTime).
    bool interactive = false;
};

// Progressive / chunked upload controls (M9b-C). A large geometry is split into
// contiguous cell-index ranges and each chunk built into its own actor, so the
// first chunk becomes visible long before the whole geometry finishes uploading
// (VTK has no efficient single-vtkPolyData append). lod carries the LOD options
// applied per chunk actor (M9b-B); off by default keeps plain actors.
struct ChunkUploadSpec {
    std::size_t maxCellsPerChunk = 1u << 20; // ~1M cells/chunk default
    LodOptions lod = {};
};

// Per-chunk progress callback: invoked after each chunk's actor is added, with
// a cumulative RenderStats (completedChunkCount grows 1..chunkCount). The first
// invocation already has >= 1 actor in the scene, so the caller may render to
// show partial results before the upload completes.
using ChunkProgressFn = std::function<void(const RenderStats&)>;

} // namespace xq

#endif // XQ_VISUALIZATION_RENDER_TYPES_H
