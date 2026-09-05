#ifndef XQ_VISUALIZATION_SCENE_RENDERER_H
#define XQ_VISUALIZATION_SCENE_RENDERER_H

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class XQTriangleSurfaceGeometryHandle;
class XQTetVolumeMeshHandle;
class XQPathPayload;
class XQFlowResultPayload;
class XQSegmentationMaskPayload;
class IGeometrySource;

// Per-add summary, mirroring XQImageViewer's ImageRenderResult contract: an
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

// A single multi-actor scene renderer: holds one vtkRenderer and a list of
// actors, and assembles a different actor per main-line product (image slice /
// path / surface model / volume mesh / flow result / segmentation mask).
//
// Public header is VTK-free (pimpl), the same boundary discipline XQImageViewer
// uses: all vtk* types live only in the .cpp Impl. The main window selects a
// scene node and calls clear() + the matching add*(); XQRenderWidget (same lib)
// pulls the internal vtkRenderer through the opaque vtkRendererHandle() to mount
// it on an interactive render window.
class XQSceneRenderer {
public:
    XQSceneRenderer();
    ~XQSceneRenderer();

    XQSceneRenderer(const XQSceneRenderer&) = delete;
    XQSceneRenderer& operator=(const XQSceneRenderer&) = delete;

    // Removes all assembled actors (resets the scene to empty).
    void clear();

    // Renders an axis-aligned slice of an image volume. `buffer` carries the
    // real scalars (XQImageVolume's own ImageBufferHandle is counts-only). When
    // buffer is non-null its scalars are copied into the vtkImageData; when it
    // is null the slice degrades to a neutral mid-intensity fill from the
    // volume's intensity range (no synthetic gradient). axis is 0/1/2 (x/y/z);
    // slice is the index along that axis (clamped into range).
    RenderStats addImageSlice(const XQImageVolume& img,
                              const XQMemoryImageBufferHandle* buffer,
                              int axis,
                              int slice);

    RenderStats addPath(const XQPathPayload& path);

    // Triangle surface, coloured by per-triangle faceId. Normals are
    // auto-oriented (M3 winding is not globally consistent), see the .cpp.
    // lod (default off) enables LOD/decimation upload (M9b-B); off == M9a path.
    RenderStats addSurface(const XQTriangleSurfaceGeometryHandle& surf,
                           const LodOptions& lod = {});

    // Progressive surface upload (M9b-C): consumes a geometry source directly,
    // splits its triangles into chunks (spec.maxCellsPerChunk), and builds one
    // actor per chunk so the first chunk is visible before the whole upload
    // finishes. onChunk (optional) fires after each chunk with cumulative stats.
    // pointCount stays the source total; actorCount is the scene actor count
    // after the upload; chunkCount/completedChunkCount track progress. Does not
    // implicitly clear() the scene (same as the other add* calls).
    RenderStats addSurfaceProgressive(const IGeometrySource& source,
                                      const ChunkUploadSpec& spec = {},
                                      const ChunkProgressFn& onChunk = {});

    // Tet volume mesh rendered as its surface with edges (wireframe-ish).
    // Boundary normals are auto-oriented for the same reason as addSurface.
    RenderStats addVolumeMesh(const XQTetVolumeMeshHandle& mesh);

    // Progressive volume-mesh upload (M9b-C): chunked counterpart of
    // addVolumeMesh, splitting tetrahedra into per-chunk wireframe actors.
    RenderStats addVolumeMeshProgressive(const IGeometrySource& source,
                                         const ChunkUploadSpec& spec = {},
                                         const ChunkProgressFn& onChunk = {});

    // Flow result rendered minimally: one polyline laid out along segmentId,
    // coloured by a per-segment scalar (last-time pressure). The flow result
    // carries no geometry of its own, so this is a schematic, not a 3D layout.
    RenderStats addFlowResult(const XQFlowResultPayload& flow);

    // Segmentation mask rendered as foreground voxel points, positioned through
    // the mask's own geometry. Needs no image buffer.
    RenderStats addSegmentationMask(const XQSegmentationMaskPayload& mask);

    // Renders the current scene offscreen to a tightly packed RGBA buffer
    // (width*height*4 bytes). No window required; safe under QT_QPA_PLATFORM=
    // offscreen. ok is false on a bad size or a render error.
    RenderStats renderOffscreenToRgba(int width,
                                      int height,
                                      std::vector<unsigned char>* outRgba);

    // Opaque handle to the internal vtkRenderer, for XQRenderWidget (same lib)
    // to mount on its interactive render window. Returns void* on purpose: the
    // header stays VTK-free; the widget .cpp static_casts it back to
    // vtkRenderer*. Same-sink internal contract, not a public API.
    void* vtkRendererHandle() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xq

#endif // XQ_VISUALIZATION_SCENE_RENDERER_H
