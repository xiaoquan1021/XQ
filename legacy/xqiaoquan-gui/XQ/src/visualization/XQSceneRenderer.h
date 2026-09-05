#ifndef XQ_VISUALIZATION_SCENE_RENDERER_H
#define XQ_VISUALIZATION_SCENE_RENDERER_H

#include "visualization/XQRenderTypes.h"

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

// A single multi-actor scene renderer: holds one vtkRenderer and a list of
// actors, and assembles a different actor per main-line product (image slice /
// path / surface model / volume mesh / flow result / segmentation mask).
//
// TEST-ONLY: this is the offscreen-to-RGBA contract harness behind
// test_scene_renderer / test_scene_renderer_progressive / test_surface_lod. The
// product render entry point is XQRenderScene (the multi-view resident scene).
// Do not reference XQSceneRenderer from product code.
//
// Public header is VTK-free (pimpl): all vtk* types live only in the .cpp Impl.
// A caller assembles a scene by calling clear() + the matching add*(), then
// pulls the internal vtkRenderer through the opaque vtkRendererHandle() to mount
// it on a render window.
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

    // Opaque handle to the internal vtkRenderer, for a caller to mount on a
    // render window. Returns void* on purpose: the
    // header stays VTK-free; the widget .cpp static_casts it back to
    // vtkRenderer*. Same-sink internal contract, not a public API.
    void* vtkRendererHandle() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xq

#endif // XQ_VISUALIZATION_SCENE_RENDERER_H
