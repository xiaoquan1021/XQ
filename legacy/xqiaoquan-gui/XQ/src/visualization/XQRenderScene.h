#ifndef XQ_VISUALIZATION_RENDER_SCENE_H
#define XQ_VISUALIZATION_RENDER_SCENE_H
#include "visualization/XQRenderTypes.h"
#include "core/NodeId.h"
#include <array>
#include <cstddef>
#include <memory>
#include <vector>
namespace xq {
class XQImageVolume; class XQMemoryImageBufferHandle;
class XQDataNode; class IGeometrySource;

enum class ViewId { Axial, Sagittal, Coronal, Volume3D };
enum class GeoKind { Surface, TetMesh };

// Resident-GPU scene compositor: four persistent vtkRenderer (Axial / Sagittal /
// Coronal / Volume3D). The volume is uploaded once; slicing is a parameter
// change; nodes are resident actor groups toggled by visibility. Public header
// is VTK-free (pimpl); the widget layer mounts renderers through the opaque
// vtkRendererHandle. Pure VTK -- no Qt.
class XQRenderScene {
public:
    XQRenderScene(); ~XQRenderScene();
    XQRenderScene(const XQRenderScene&) = delete;
    XQRenderScene& operator=(const XQRenderScene&) = delete;

    // Volume: build a resident vtkImageData once (null buffer -> neutral midpoint
    // fill, same as the old build_image). The three 2D slice views each mount a
    // vtkImageSlice sharing that vtkImageData; the 3D view mounts three orthogonal
    // slice planes. A repeat call replaces the whole volume. Returns false on
    // invalid geometry.
    bool setVolume(const XQImageVolume& img, const XQMemoryImageBufferHandle* buffer);
    void clearVolume();
    bool hasVolume() const;

    // Slice (axis: 0=x/Sagittal, 1=y/Coronal, 2=z/Axial). Out-of-range is
    // clamped; updates that axis's 2D mapper + the
    // matching 3D plane + crosshairs. No-op when there is no volume.
    void setSliceIndex(int axis, int index);
    int  sliceIndex(int axis) const;   // -1 when there is no volume
    int  sliceCount(int axis) const;   // 0 when there is no volume

    // Window/level: shared vtkImageProperty (three slices + 3D planes all linked).
    // Initialized to the full intensityRange window on setVolume.
    void setWindowLevel(double window, double level);
    void windowLevel(double* window, double* level) const;

    // Crosshairs: two orthogonal line actors per slice view, positioned from the
    // current slice indices of the other two axes.
    void setCrosshairVisible(bool on);
    bool crosshairVisible() const;     // default true

    // Shows/hides the three grey-scale slice planes mounted in the 3D view (the 2D
    // slice views are unaffected). Default true. Survives setVolume (a volume
    // reload re-applies the current flag to the rebuilt planes).
    void setImagePlanesVisible3d(bool on);
    bool imagePlanesVisible3d() const;

    // Shows/hides the volume image everywhere: the three 2D slice images and the
    // 3D planes (the latter additionally gated by imagePlanesVisible3d). Crosshairs
    // are unaffected. Default true; survives setVolume.
    void setImageVisible(bool on);
    bool imageVisible() const;

    // Seed marker: a small resident sphere at the picked voxel's world position,
    // shown in all four views. setSeedMarker moves + shows it; clearSeedMarker
    // hides it; clearVolume hides it too. No volume -> no-op.
    void setSeedMarker(int i, int j, int k);
    void clearSeedMarker();
    bool seedMarkerVisible() const;   // probe

    // Path control-point markers: a resident glyph actor per view renders the
    // current draft control points (distinct colour from the seed sphere). Each 2D
    // slice view only shows the points near its current slice plane (a marker
    // appears on the slice it sits on); the 3D view shows all. setPathControlPoints
    // replaces the whole set from world coordinates (an empty vector clears/hides);
    // clearPathControlPoints empties it; clearVolume drops them.
    void setPathControlPoints(const std::vector<std::array<double, 3>>& worldPoints);
    void clearPathControlPoints();
    std::size_t pathControlPointCount() const;  // probe: total set size
    // Probe: how many control points currently pass the slice-distance filter for
    // the 2D slice view of `axis` (0=Sagittal,1=Coronal,2=Axial), i.e. how many
    // markers that view shows at the current slice. -1 for an invalid axis.
    int pathControlVisibleCount(int axis) const;

    // --- crosshair drag support (B2a) ---
    // World coordinate of the current slice plane of `axis` (the plane's constant
    // component). 0.0 when there is no volume.
    double sliceWorldCoord(int axis) const;
    // Inverse: world coordinate along `axis` -> nearest slice index, clamped into
    // range. -1 when there is no volume.
    int worldToSliceIndex(int axis, double world) const;
    // World point -> nearest voxel index (rounded, clamped into the volume).
    // False when there is no volume. The out i/j/k follow the image index axes.
    bool worldToVoxelIndex(const double world[3], int* i, int* j, int* k) const;
    // Probe: which axis the given crosshair line represents (see table in .cpp).
    static int crosshairLineAxis(int viewAxis, int line);
    // Probe: that line's colour (rgb 0-1). False when there is no volume/actor.
    bool crosshairLineColor(int viewAxis, int line, double rgb[3]) const;

    // Nodes: first sight builds the actor group (payload -> actor; B1a mounts into
    // Volume3D only); re-upsert removes the old group and rebuilds (the payload
    // may have changed). A non-renderable payload (Image / SimulationCase /
    // Unknown / empty payload) -> ok=false and leaves no trace. Visibility /
    // opacity / color are presentation attributes owned by this class.
    RenderStats upsertNode(const NodeId& id, const XQDataNode& node);
    // Progressive path: the caller (app layer) has already resolved an
    // IGeometrySource; kind selects surface / tet assembly.
    RenderStats upsertNodeProgressive(const NodeId& id, const IGeometrySource& src,
                                      GeoKind kind, const ChunkUploadSpec& spec = {},
                                      const ChunkProgressFn& onChunk = {});
    void removeNode(const NodeId& id);
    void clearNodes();
    void setNodeVisible(const NodeId& id, bool on);   // unknown id: no-op
    void setNodeOpacity(const NodeId& id, double a);
    void setNodeColor(const NodeId& id, double r, double g, double b);

    // Discrete-invariant probes (for headless tests; read-only, no render).
    int  nodeActorCount(ViewId v) const;  // node actors only (no image slice / crosshair)
    bool hasNode(const NodeId& id) const;
    bool nodeVisible(const NodeId& id) const;         // unknown id -> false
    long long uploadedPointCount(const NodeId& id) const; // unknown id -> -1

    // Probe: this node's slice-cut actor count in the given slice view (0 for the
    // 3D view / unknown id / a node kind that has no cut overlay).
    int nodeSliceCutCount(const NodeId& id, ViewId v) const;
    // Probe: for a contour-group node, 1 when at least one of its contours is
    // currently "hit" (centroid within spacing/2 of the slice plane) in the given
    // slice view, else 0. Always 0 for the 3D view / an unknown id / a node that
    // is not a contour group. Distinct from nodeSliceCutCount (surface cut lines):
    // contours are planar curves, distance-filtered onto slices, never cut.
    int nodeContourOverlayCount(const NodeId& id, ViewId v) const;
    // Probe: the node's current display colour (first actor's property). False for
    // an unknown id.
    bool nodeColor(const NodeId& id, double rgb[3]) const;

    // Widget mounting (B1b): opaque vtkRenderer*.
    void* vtkRendererHandle(ViewId v) const;

    // Opaque vtkImageData* of the resident volume (null when no volume). Lets the
    // cross-section resampler reslice the same resident image without a second copy.
    void* vtkImageDataHandle() const;
private:
    class Impl; std::unique_ptr<Impl> impl_;
};
} // namespace xq
#endif
