#include "visualization/XQRenderScene.h"

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQContourGroup.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResult.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQMesh.h"
#include "core/XQMeshPayload.h"
#include "core/XQPath.h"
#include "core/XQPathPayload.h"
#include "core/XQPayload.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQSurfaceModel.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/IGeometrySource.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/source/ResidentTetSource.h"
#include "visualization/ChunkPlan.h"
#include "visualization/SurfaceLodBuilder.h"

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkCutter.h>
#include <vtkDoubleArray.h>
#include <vtkExtractEdges.h>
#include <vtkFloatArray.h>
#include <vtkGeometryFilter.h>
#include <vtkGlyph3D.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkLODActor.h>
#include <vtkLineSource.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkType.h>
#include <vtkTypeInt32Array.h>
#include <vtkUnstructuredGrid.h>
#include <vtkVertexGlyphFilter.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

namespace xq {
namespace {

// Default single-colour anatomical green for surface models (B2c). The faceId
// LUT path in makeSurfaceMapper is preserved but off by default; surfaces mount
// flat-shaded in this colour so the model reads as one solid piece instead of a
// multi-hue patchwork. A future toggle can flip back to faceId colouring.
constexpr double kSurfaceColor[3] = {0.25, 0.80, 0.35};

// Bright anatomical green for .ctgr contour polylines (same green family as the
// surface model, brightened so the thin lines read against the dark 3D scene and
// on top of the grey slice image).
constexpr double kContourColor[3] = {0.30, 0.95, 0.40};

// --- internal converters: XQ geometry -> VTK data objects ------------------
// Copied wholesale from XQSceneRenderer.cpp (B1a inventory: reusable converters).
// Conversion lives here (anonymous namespace, .cpp only) so the scene just calls
// a converter. Each converter preserves XQ point counts so the RenderStats the
// caller asserts on stay truthful.

bool valid_geometry(const ImageGeometry& geometry)
{
    for (int i = 0; i < 3; ++i) {
        if (geometry.dimensions[i] <= 0 || geometry.spacing[i] <= 0.0) {
            return false;
        }
    }
    return true;
}

void apply_geometry(vtkImageData* image, const ImageGeometry& geometry)
{
    image->SetDimensions(geometry.dimensions);
    image->SetSpacing(geometry.spacing);
    image->SetOrigin(geometry.origin);

    double direction[9] = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            direction[row * 3 + column] = geometry.direction[row][column];
        }
    }
    image->SetDirectionMatrix(direction);
}

// Builds a single-component float vtkImageData from the volume geometry. When a
// real buffer is supplied its scalars are copied into component 0; a contiguous
// Float32/1-component buffer whose dimensions match is bulk-copied (memcpy fast
// path), otherwise every voxel is fetched through the buffer's scalarAt (the old
// per-voxel path, only reached at load time). A null buffer degrades to the
// midpoint of the intensity range -- a neutral fill.
vtkSmartPointer<vtkImageData> build_image(const XQImageVolume& volume,
                                          const XQMemoryImageBufferHandle* buffer)
{
    const ImageGeometry& geometry = volume.geometry();
    if (!valid_geometry(geometry)) {
        return nullptr;
    }

    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    apply_geometry(image, geometry);
    image->AllocateScalars(VTK_FLOAT, 1);
    if (image->GetScalarPointer() == nullptr) {
        return nullptr;
    }

    const int dimX = geometry.dimensions[0];
    const int dimY = geometry.dimensions[1];
    const int dimZ = geometry.dimensions[2];

    const bool useBuffer =
        buffer != nullptr && buffer->is_valid()
        && buffer->dimensionX() == dimX && buffer->dimensionY() == dimY
        && buffer->dimensionZ() == dimZ;

    const IntensityRange& range = volume.intensityRange();
    const double midpoint =
        range.maximum > range.minimum ? (range.minimum + range.maximum) * 0.5 : 0.0;

    float* base = static_cast<float*>(image->GetScalarPointer(0, 0, 0));

    // memcpy fast path (B1a): a contiguous Float32, single-component buffer whose
    // dimensions match uses the identical x-fastest layout as VTK image scalars
    // (XQMemoryImageBufferHandle.h:20-21), so the whole block copies in one shot
    // instead of the per-voxel scalarAt loop.
    const std::size_t voxelCount = static_cast<std::size_t>(dimX)
                                   * static_cast<std::size_t>(dimY)
                                   * static_cast<std::size_t>(dimZ);
    if (useBuffer && buffer->scalarType() == ScalarType::Float32
        && buffer->componentCount() == 1
        && buffer->bytes().size() == voxelCount * sizeof(float)) {
        std::memcpy(base, buffer->bytes().data(), voxelCount * sizeof(float));
        return image;
    }

    // VTK image scalars are contiguous row-major (x fastest), single-component
    // float, extent 0-based (SetDimensions from 0). Fetch the base once and
    // advance linearly instead of recomputing the offset per voxel.
    std::size_t offset = 0;
    for (int z = 0; z < dimZ; ++z) {
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x, ++offset) {
                float* scalar = base + offset;
                if (useBuffer) {
                    const std::size_t index = buffer->voxelIndex(x, y, z);
                    *scalar = static_cast<float>(buffer->scalarAt(index, 0));
                } else {
                    *scalar = static_cast<float>(midpoint);
                }
            }
        }
    }

    return image;
}

// --- batched VTK upload from Source views (M9a) -----------------------------
// Point3 is a packed double[3]; the scene copies the contiguous Source span
// straight into VTK-owned arrays (copy-on-upload), so no Source lease has to
// outlive the upsert call (no dangling borrow into VTK).
static_assert(sizeof(Point3) == 3 * sizeof(double),
              "Point3 must be packed double[3] for bulk upload");

// Copies a Point3 span into a VTK-owned 3-component double array wrapped in
// vtkPoints. Bulk memcpy, not per-point SetPoint.
vtkSmartPointer<vtkPoints> upload_points(const ReadSpan<Point3>& pts)
{
    vtkNew<vtkDoubleArray> coords;
    coords->SetNumberOfComponents(3);
    coords->SetNumberOfTuples(static_cast<vtkIdType>(pts.size()));
    if (!pts.empty()) {
        std::memcpy(coords->GetPointer(0), pts.data(), pts.size() * sizeof(Point3));
    }
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetData(coords);
    return points;
}

// Builds a VTK 9 cell array (offsets + connectivity) for uniform cells of
// `verticesPerCell` from a flat int32 connectivity span.
vtkSmartPointer<vtkCellArray> build_cells(const int* flatConnectivity,
                                          std::size_t cellCount,
                                          int verticesPerCell)
{
    vtkNew<vtkTypeInt32Array> connectivity;
    const std::size_t connSize = cellCount * static_cast<std::size_t>(verticesPerCell);
    connectivity->SetNumberOfValues(static_cast<vtkIdType>(connSize));
    if (connSize > 0) {
        std::memcpy(connectivity->GetPointer(0), flatConnectivity, connSize * sizeof(int));
    }

    vtkNew<vtkTypeInt32Array> offsets;
    offsets->SetNumberOfValues(static_cast<vtkIdType>(cellCount + 1));
    int* offsetPtr = offsets->GetPointer(0);
    for (std::size_t i = 0; i <= cellCount; ++i) {
        offsetPtr[i] = static_cast<int>(i * static_cast<std::size_t>(verticesPerCell));
    }

    vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
    cells->SetData(offsets, connectivity);
    return cells;
}

bool connectivity_in_bounds(const int* flatConnectivity,
                            std::size_t cellCount,
                            int verticesPerCell,
                            std::size_t pointCount)
{
    if (verticesPerCell <= 0) {
        return false;
    }
    if (cellCount == 0) {
        return true;
    }
    if (flatConnectivity == nullptr || pointCount == 0) {
        return false;
    }

    const std::size_t verts = static_cast<std::size_t>(verticesPerCell);
    if (cellCount > (std::numeric_limits<std::size_t>::max)() / verts) {
        return false;
    }
    const std::size_t connCount = cellCount * verts;
    for (std::size_t i = 0; i < connCount; ++i) {
        const int g = flatConnectivity[i];
        if (g < 0 || static_cast<std::size_t>(g) >= pointCount) {
            return false;
        }
    }
    return true;
}

vtkSmartPointer<vtkPolyData> build_surface(const IGeometrySource& source)
{
    GeometryLease<Point3> pointLease = source.acquire_points();
    TriangleLease triLease = source.acquire_triangles();
    const ReadSpan<Point3>& pts = pointLease.span();
    const TriangleView& triView = triLease.view();
    const ReadSpan<SourceTriangle>& tris = triView.triangles;

    vtkSmartPointer<vtkPoints> points = upload_points(pts);

    const int* flatConn = reinterpret_cast<const int*>(tris.data());
    if (!connectivity_in_bounds(flatConn, tris.size(), 3, pts.size())) {
        return nullptr;
    }
    vtkSmartPointer<vtkCellArray> triangles =
        build_cells(flatConn, tris.size(), 3);

    vtkNew<vtkFloatArray> faceIds;
    faceIds->SetName("faceId");
    faceIds->SetNumberOfComponents(1);
    faceIds->SetNumberOfTuples(static_cast<vtkIdType>(triView.faceIds.size()));
    for (std::size_t i = 0; i < triView.faceIds.size(); ++i) {
        faceIds->SetValue(static_cast<vtkIdType>(i), static_cast<float>(triView.faceIds[i]));
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetPolys(triangles);
    polyData->GetCellData()->SetScalars(faceIds);
    return polyData;
}

// --- M9b-C: progressive / chunked upload helpers ---------------------------

struct ChunkMesh {
    vtkSmartPointer<vtkPoints> points;
    vtkSmartPointer<vtkCellArray> cells;
    std::size_t cellCount = 0;
    std::size_t pointCount = 0;
};

ChunkMesh build_cell_chunk(const ReadSpan<Point3>& allPoints,
                           const int* flatConn,
                           const ChunkRange& range,
                           int vertsPerCell,
                           std::vector<int>& remap)
{
    const std::size_t cellCount = range.end - range.begin;
    const std::size_t connBase = range.begin * static_cast<std::size_t>(vertsPerCell);
    const std::size_t connCount = cellCount * static_cast<std::size_t>(vertsPerCell);

    std::vector<int> localConn(connCount);
    std::vector<Point3> localPoints;
    localPoints.reserve(connCount);
    std::vector<int> touched;
    touched.reserve(connCount);

    for (std::size_t i = 0; i < connCount; ++i) {
        const int g = flatConn[connBase + i];
        int local = (g >= 0 && static_cast<std::size_t>(g) < remap.size()) ? remap[g] : -1;
        if (local < 0) {
            local = static_cast<int>(localPoints.size());
            localPoints.push_back(allPoints[static_cast<std::size_t>(g)]);
            if (g >= 0 && static_cast<std::size_t>(g) < remap.size()) {
                remap[g] = local;
                touched.push_back(g);
            }
        }
        localConn[i] = local;
    }

    ChunkMesh mesh;
    mesh.points = upload_points(ReadSpan<Point3>(localPoints.data(), localPoints.size()));
    mesh.cells = build_cells(localConn.data(), cellCount, vertsPerCell);
    mesh.cellCount = cellCount;
    mesh.pointCount = localPoints.size();

    for (int g : touched) {
        remap[g] = -1;
    }
    return mesh;
}

vtkSmartPointer<vtkUnstructuredGrid> build_volume(const IGeometrySource& source)
{
    GeometryLease<Point3> pointLease = source.acquire_points();
    GeometryLease<SourceTet> tetLease = source.acquire_tetrahedra();
    const ReadSpan<Point3>& pts = pointLease.span();
    const ReadSpan<SourceTet>& tets = tetLease.span();

    const int* flatConn = reinterpret_cast<const int*>(tets.data());
    if (!connectivity_in_bounds(flatConn, tets.size(), 4, pts.size())) {
        return nullptr;
    }

    vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(upload_points(pts));

    vtkSmartPointer<vtkCellArray> cells = build_cells(flatConn, tets.size(), 4);
    grid->SetCells(VTK_TETRA, cells);

    return grid;
}

// Builds a single open polyline through the given world points. Used both for
// real paths and for the schematic flow polyline.
vtkSmartPointer<vtkPolyData> build_polyline(const std::vector<Point3>& worldPoints)
{
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    points->SetNumberOfPoints(static_cast<vtkIdType>(worldPoints.size()));
    for (std::size_t i = 0; i < worldPoints.size(); ++i) {
        const Point3& p = worldPoints[i];
        points->SetPoint(static_cast<vtkIdType>(i), p.x, p.y, p.z);
    }

    vtkNew<vtkCellArray> lines;
    if (worldPoints.size() >= 2) {
        lines->InsertNextCell(static_cast<vtkIdType>(worldPoints.size()));
        for (std::size_t i = 0; i < worldPoints.size(); ++i) {
            lines->InsertCellPoint(static_cast<vtkIdType>(i));
        }
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    return polyData;
}

// One cached contour of a contour-group node: its world points (already world
// coordinates from the reader), whether it is closed, and its centroid (used for
// distance-based slice filtering). Cached on the NodeEntry so a slice move can
// re-filter + rebuild the thin per-view overlay polydata without going back to
// the payload. Defined here (in the anonymous namespace) so the converters below
// can take it; NodeEntry (further down) holds a vector of these.
struct ContourCurve {
    std::vector<Point3> points;
    Point3 centroid = {0.0, 0.0, 0.0};
    bool closed = false;
};

// Centroid of a contour's points (mean position). Empty -> origin.
Point3 contour_centroid(const std::vector<Point3>& points)
{
    Point3 c = {0.0, 0.0, 0.0};
    if (points.empty()) {
        return c;
    }
    for (const Point3& p : points) {
        c.x += p.x;
        c.y += p.y;
        c.z += p.z;
    }
    const double n = static_cast<double>(points.size());
    c.x /= n;
    c.y /= n;
    c.z /= n;
    return c;
}

// Builds one vtkPolyData holding a polyline cell for each supplied contour. A
// closed contour's cell revisits its first point so VTK draws the closing edge.
// All contours share one point list (contiguous), one cell each. Empty input ->
// an empty (but valid) polydata.
vtkSmartPointer<vtkPolyData> build_contour_polydata(
    const std::vector<const ContourCurve*>& curves)
{
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    vtkNew<vtkCellArray> lines;

    for (const ContourCurve* curve : curves) {
        const std::vector<Point3>& pts = curve->points;
        if (pts.size() < 2) {
            continue;
        }
        const vtkIdType base = points->GetNumberOfPoints();
        for (const Point3& p : pts) {
            points->InsertNextPoint(p.x, p.y, p.z);
        }
        const vtkIdType n = static_cast<vtkIdType>(pts.size());
        const vtkIdType cellPoints = curve->closed ? n + 1 : n;
        lines->InsertNextCell(cellPoints);
        for (vtkIdType i = 0; i < n; ++i) {
            lines->InsertCellPoint(base + i);
        }
        if (curve->closed) {
            lines->InsertCellPoint(base); // close the loop
        }
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    return polyData;
}

void voxel_to_world(const ImageGeometry& g, int x, int y, int z, double world[3])
{
    const double voxel[3] = {static_cast<double>(x),
                             static_cast<double>(y),
                             static_cast<double>(z)};
    for (int row = 0; row < 3; ++row) {
        double sum = g.origin[row];
        for (int column = 0; column < 3; ++column) {
            sum += g.direction[row][column] * g.spacing[column] * voxel[column];
        }
        world[row] = sum;
    }
}

// Builds the normals + LUT-or-flat mapper for one surface polyData.
// faceIdValid==false -> flat shaded (far LOD level). faceRange is the global
// faceId range so colours match across LOD levels.
vtkSmartPointer<vtkPolyDataMapper> makeSurfaceMapper(vtkPolyData* polyData,
                                                     bool faceIdValid,
                                                     const double faceRange[2])
{
    // M3 surface winding is NOT globally consistent (closed != consistent
    // normals; memory xq-surface-winding-not-consistent). vtkPolyDataNormals with
    // AutoOrientNormals + Consistency re-derives a coherent orientation instead
    // of trusting input winding.
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputData(polyData);
    normals->SetAutoOrientNormals(true);
    normals->SetConsistency(true);
    normals->SplittingOff();

    vtkSmartPointer<vtkPolyDataMapper> mapper =
        vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(normals->GetOutputPort());

    if (faceIdValid && polyData->GetCellData()->GetScalars() != nullptr) {
        vtkNew<vtkLookupTable> lut;
        lut->SetHueRange(0.55, 0.0);
        const double hi = faceRange[1] > faceRange[0] ? faceRange[1] : faceRange[0] + 1.0;
        lut->SetTableRange(faceRange[0], hi);
        lut->Build();
        mapper->SetScalarModeToUseCellData();
        mapper->SetColorModeToMapScalars();
        mapper->SetLookupTable(lut);
        mapper->SetScalarRange(faceRange[0], hi);
    } else {
        mapper->ScalarVisibilityOff();
    }
    return mapper;
}

} // namespace

// A rendered node's actor group + presentation state. B1a builds actors into the
// Volume3D renderer only (slice overlays are M3). visible/opacity/color are
// presentation attributes owned here (design §3.4), not in core.
struct NodeEntry {
    std::vector<vtkSmartPointer<vtkActor>> volumeActors; // plain actors (path/mesh/mask/flow)
    std::vector<vtkSmartPointer<vtkLODActor>> lodActors; // surface (LOD) actors
    // Slice-cut overlay (B2c): one resident vtkCutter->actor per slice view
    // (index 0=Sagittal/x, 1=Coronal/y, 2=Axial/z), each driven by a persistent
    // vtkPlane. Only surface nodes populate these; the plane origin follows the
    // matching slice, so a slice move only re-origins the plane (the cutter
    // recomputes lazily) -- no rebuild. Empty for non-surface nodes.
    std::array<vtkSmartPointer<vtkActor>, 3> cutActors;
    std::array<vtkSmartPointer<vtkPlane>, 3> cutPlanes;
    // Contour overlay (M3/B3): only contour-group nodes populate these. `contours`
    // caches the parsed curves; per slice view a resident actor shows the union of
    // the curves currently "hit" (centroid within spacing/2 of the slice plane),
    // rebuilt on a slice move. hitCount mirrors how many curves each view shows.
    bool isContourGroup = false;
    std::vector<ContourCurve> contours;
    std::array<vtkSmartPointer<vtkActor>, 3> contourOverlayActors;
    std::array<int, 3> contourOverlayHitCount = {0, 0, 0};
    RenderStats stats;
    bool visible = true;
    double opacity = 1.0;
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;
    // faceId-coloured surface actors keep a LUT; color() is a fallback only when
    // there is no LUT (design §3: LUT path only takes opacity).
    bool faceIdColoured = false;
};

class XQRenderScene::Impl {
public:
    // The owning class forwards public API to Impl; a couple of B2a probes
    // (crosshairLineAxis / crosshairLineColor / sliceWorldCoord /
    // worldToSliceIndex) live in the private section below, so grant the outer
    // class access rather than reshuffling the section boundaries.
    friend class XQRenderScene;

    Impl()
    {
        for (int v = 0; v < 4; ++v) {
            renderers_[v] = vtkSmartPointer<vtkRenderer>::New();
        }
        renderers_[static_cast<int>(ViewId::Axial)]->SetBackground(0.02, 0.02, 0.04);
        renderers_[static_cast<int>(ViewId::Sagittal)]->SetBackground(0.02, 0.02, 0.04);
        renderers_[static_cast<int>(ViewId::Coronal)]->SetBackground(0.02, 0.02, 0.04);
        renderers_[static_cast<int>(ViewId::Volume3D)]->SetBackground(0.05, 0.05, 0.08);

        imageProperty_ = vtkSmartPointer<vtkImageProperty>::New();
    }

    vtkRenderer* renderer(ViewId v) const
    {
        return renderers_[static_cast<int>(v)].Get();
    }

    // ---- volume -----------------------------------------------------------

    bool setVolume(const XQImageVolume& volume, const XQMemoryImageBufferHandle* buffer)
    {
        vtkSmartPointer<vtkImageData> image = build_image(volume, buffer);
        if (image == nullptr) {
            return false;
        }

        clearVolume();
        image_ = image;
        image_->GetExtent(extent_);
        imageGeometry_ = volume.geometry();

        const IntensityRange& range = volume.intensityRange();
        const double window =
            volume.windowWidth() > 0.0
                ? volume.windowWidth()
                : (range.maximum > range.minimum ? range.maximum - range.minimum : 255.0);
        const double level =
            volume.windowWidth() > 0.0
                ? volume.windowCenter()
                : (range.maximum > range.minimum ? (range.minimum + range.maximum) * 0.5
                                                 : 127.5);
        imageProperty_->SetColorWindow(window);
        imageProperty_->SetColorLevel(level);

        // Initial slice index = each axis's middle slice (count/2 from the low
        // extent, so an N-slice axis starts at index N/2).
        for (int axis = 0; axis < 3; ++axis) {
            const int low = extent_[axis * 2];
            const int high = extent_[axis * 2 + 1];
            const int count = high - low + 1;
            sliceIndex_[axis] = low + count / 2;
        }

        // Three 2D slice views: one vtkImageSliceMapper + vtkImageSlice each,
        // sharing the one vtkImageData and the one vtkImageProperty. The 2D view
        // for a given axis shows the slice orthogonal to that axis.
        for (int axis = 0; axis < 3; ++axis) {
            const ViewId view = viewForAxis(axis);
            vtkNew<vtkImageSliceMapper> mapper;
            mapper->SetInputData(image_);
            mapper->SetOrientation(axis);
            mapper->SetSliceNumber(sliceIndex_[axis]);

            vtkSmartPointer<vtkImageSlice> slice = vtkSmartPointer<vtkImageSlice>::New();
            slice->SetMapper(mapper);
            slice->SetProperty(imageProperty_);

            renderer(view)->AddViewProp(slice);
            sliceMappers2d_[axis] = mapper;
            slices2d_[axis] = slice;
        }

        // 3D view: three orthogonal slice planes (MITK style), same shared data +
        // property; actor/mapper independent per renderer (design §3.2).
        for (int axis = 0; axis < 3; ++axis) {
            vtkNew<vtkImageSliceMapper> mapper;
            mapper->SetInputData(image_);
            mapper->SetOrientation(axis);
            mapper->SetSliceNumber(sliceIndex_[axis]);

            vtkSmartPointer<vtkImageSlice> slice = vtkSmartPointer<vtkImageSlice>::New();
            slice->SetMapper(mapper);
            slice->SetProperty(imageProperty_);

            renderer(ViewId::Volume3D)->AddViewProp(slice);
            sliceMappers3d_[axis] = mapper;
            slices3d_[axis] = slice;
        }

        // Re-apply the current image-visibility flags to the freshly rebuilt slice
        // actors (2D images + 3D planes) so a volume reload never silently resets a
        // user's hide choice.
        applyImageVisibility();

        buildCrosshairs();
        updateCrosshairs();
        // Resident seed spheres (hidden until a pick); a fresh volume starts with
        // no seed shown.
        buildSeedMarkers();
        // Resident path-control-point glyphs (hidden/empty until a draft pick).
        buildPathControlMarkers();

        // Slice views get axis-aligned parallel-projection cameras; the 3D view
        // keeps its default perspective. ResetCamera (called inside) then frames
        // the volume in each renderer.
        setupSliceCameras();
        renderer(ViewId::Volume3D)->ResetCamera();
        // Any nodes already resident (model loaded before the volume) get their
        // slice-cut planes re-origined onto the fresh slices.
        updateAllCutPlanes();
        // Contour-group nodes resident before the volume landed need their slice
        // overlays filtered against the now-real slice planes.
        rebuildAllContourOverlays();
        return true;
    }

    // Axis-aligned parallel-projection cameras for the three slice views (LPS).
    // Each camera looks down the slice axis at the image centre with the correct
    // in-plane up vector; ResetCamera then frames the slice and sets the parallel
    // scale. The 3D view is untouched (default perspective). Axis directions were
    // set from the LPS convention; the B1b real-machine pass may tune them.
    void setupSliceCameras()
    {
        if (image_ == nullptr) {
            return;
        }
        double center[3] = {0.0, 0.0, 0.0};
        image_->GetCenter(center);
        double bounds[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        image_->GetBounds(bounds);

        // For each slice axis: (view direction offset from focal point, view-up).
        //   Axial    (axis 2, z-slice): camera on -Z,  up = (0,-1,0)
        //   Sagittal (axis 0, x-slice): camera on +X,  up = (0, 0,1)
        //   Coronal  (axis 1, y-slice): camera on -Y,  up = (0, 0,1)
        struct SliceCam {
            int axis;
            double offset[3];
            double up[3];
        };
        const SliceCam cams[3] = {
            {2, {0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}},
            {0, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
            {1, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
        };
        for (const SliceCam& cam : cams) {
            vtkRenderer* view = renderer(viewForAxis(cam.axis));
            vtkCamera* camera = view->GetActiveCamera();
            camera->ParallelProjectionOn();
            camera->SetFocalPoint(center[0], center[1], center[2]);
            camera->SetPosition(center[0] + cam.offset[0],
                                center[1] + cam.offset[1],
                                center[2] + cam.offset[2]);
            camera->SetViewUp(cam.up[0], cam.up[1], cam.up[2]);
            view->ResetCamera();
            // ResetCamera frames the whole volume with a default margin, so in a
            // wide window a tall slice only occupies a narrow central strip. Tighten
            // the parallel scale to exactly half the image extent along this view's
            // up direction: the slice then fills the window vertically (no margin);
            // the horizontal fit follows from the window aspect ratio (a slight edge
            // crop is acceptable, matching SV). up is a unit axis vector, so its
            // non-zero component picks the world axis whose bounds span sets the fit.
            int upAxis = 0;
            for (int a = 0; a < 3; ++a) {
                if (cam.up[a] != 0.0) {
                    upAxis = a;
                    break;
                }
            }
            const double upExtent = bounds[2 * upAxis + 1] - bounds[2 * upAxis];
            if (upExtent > 0.0) {
                camera->SetParallelScale(0.5 * upExtent);
            }
        }
    }

    void clearVolume()
    {
        for (int axis = 0; axis < 3; ++axis) {
            if (slices2d_[axis] != nullptr) {
                renderer(viewForAxis(axis))->RemoveViewProp(slices2d_[axis]);
            }
            if (slices3d_[axis] != nullptr) {
                renderer(ViewId::Volume3D)->RemoveViewProp(slices3d_[axis]);
            }
            slices2d_[axis] = nullptr;
            slices3d_[axis] = nullptr;
            sliceMappers2d_[axis] = nullptr;
            sliceMappers3d_[axis] = nullptr;
            sliceIndex_[axis] = -1;
        }
        for (int axis = 0; axis < 3; ++axis) {
            for (int line = 0; line < 2; ++line) {
                if (crosshairActors_[axis][line] != nullptr) {
                    renderer(viewForAxis(axis))
                        ->RemoveViewProp(crosshairActors_[axis][line]);
                    crosshairActors_[axis][line] = nullptr;
                    crosshairSources_[axis][line] = nullptr;
                }
            }
        }
        // Seed markers + path-control glyphs: one per view, in the same order
        // their build helpers mount them.
        {
            const ViewId views[4] = {ViewId::Axial, ViewId::Sagittal,
                                     ViewId::Coronal, ViewId::Volume3D};
            for (int v = 0; v < 4; ++v) {
                if (seedMarkerActors_[v] != nullptr) {
                    renderer(views[v])->RemoveViewProp(seedMarkerActors_[v]);
                    seedMarkerActors_[v] = nullptr;
                    seedMarkerSources_[v] = nullptr;
                }
                if (pathControlActors_[v] != nullptr) {
                    renderer(views[v])->RemoveViewProp(pathControlActors_[v]);
                    pathControlActors_[v] = nullptr;
                    pathControlPoly_[v] = nullptr;
                    pathControlPoints_[v] = nullptr;
                }
            }
        }
        seedMarkerVisible_ = false;
        pathControlWorld_.clear();
        pathControlCount_ = 0;
        image_ = nullptr;
        for (int i = 0; i < 6; ++i) {
            extent_[i] = 0;
        }
        // No volume -> no meaningful slice; park every resident node's cut planes
        // at the origin (matches buildSliceCutActors' no-volume default). A later
        // setVolume re-origins them via updateAllCutPlanes.
        for (auto& kv : nodes_) {
            for (int axis = 0; axis < 3; ++axis) {
                if (kv.second.cutPlanes[axis] != nullptr) {
                    kv.second.cutPlanes[axis]->SetOrigin(0.0, 0.0, 0.0);
                }
            }
        }
        // With image_ now null, re-filtering clears every contour overlay (no
        // meaningful slice plane -> no hits). A later setVolume re-filters them.
        rebuildAllContourOverlays();
    }

    bool hasVolume() const { return image_ != nullptr; }

    // Opaque vtkImageData* of the resident volume (null when none). The
    // cross-section resampler reslices this same image without a second copy.
    void* imageDataHandle() const { return image_.GetPointer(); }

    void setSliceIndex(int axis, int index)
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return;
        }
        const int low = extent_[axis * 2];
        const int high = extent_[axis * 2 + 1];
        const int clamped = std::min(std::max(index, low), high);
        sliceIndex_[axis] = clamped;
        if (sliceMappers2d_[axis] != nullptr) {
            sliceMappers2d_[axis]->SetSliceNumber(clamped);
        }
        if (sliceMappers3d_[axis] != nullptr) {
            sliceMappers3d_[axis]->SetSliceNumber(clamped);
        }
        updateCrosshairs();
        updateCutPlanes(axis);
        // Re-filter contour overlays for the axis that moved (contour counts are
        // small; scoped to the moved axis to match updateCutPlanes).
        rebuildContourOverlaysForAxis(axis);
        // Re-filter path control-point markers so a marker only shows on the slice
        // it sits on (the moved slice plane may now include/exclude points).
        refreshPathControlMarkers();
    }

    // Re-origins every node's slice-cut plane for `axis` to that axis's current
    // slice world coord. Only the plane moves; the vtkCutter recomputes lazily on
    // the next render, so a slice change never rebuilds the cut geometry.
    void updateCutPlanes(int axis)
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return;
        }
        const double coord = sliceWorld(axis);
        for (auto& kv : nodes_) {
            vtkPlane* plane = kv.second.cutPlanes[axis];
            if (plane == nullptr) {
                continue;
            }
            double origin[3] = {0.0, 0.0, 0.0};
            origin[axis] = coord;
            plane->SetOrigin(origin);
        }
    }

    // Resets all three cut planes of every node to the current slice coords.
    // Called after the volume (re)builds so cut outlines land on the real slices.
    void updateAllCutPlanes()
    {
        for (int axis = 0; axis < 3; ++axis) {
            updateCutPlanes(axis);
        }
    }

    int sliceIndex(int axis) const
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return -1;
        }
        return sliceIndex_[axis];
    }

    int sliceCount(int axis) const
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return 0;
        }
        return extent_[axis * 2 + 1] - extent_[axis * 2] + 1;
    }

    void setWindowLevel(double window, double level)
    {
        imageProperty_->SetColorWindow(window);
        imageProperty_->SetColorLevel(level);
    }

    void windowLevel(double* window, double* level) const
    {
        if (window != nullptr) {
            *window = imageProperty_->GetColorWindow();
        }
        if (level != nullptr) {
            *level = imageProperty_->GetColorLevel();
        }
    }

    void setCrosshairVisible(bool on)
    {
        crosshairVisible_ = on;
        for (int axis = 0; axis < 3; ++axis) {
            for (int line = 0; line < 2; ++line) {
                if (crosshairActors_[axis][line] != nullptr) {
                    crosshairActors_[axis][line]->SetVisibility(on ? 1 : 0);
                }
            }
        }
    }

    bool crosshairVisible() const { return crosshairVisible_; }

    // Shows/hides the three 3D slice planes only (slices3d_); the 2D slice views
    // (slices2d_) and crosshairs are untouched. The flag is remembered so a later
    // setVolume re-applies it to the rebuilt planes. Effective 3D-plane visibility
    // is the AND of this flag and imageVisible_.
    void setImagePlanesVisible3d(bool on)
    {
        imagePlanes3dVisible_ = on;
        applyImageVisibility();
    }

    bool imagePlanesVisible3d() const { return imagePlanes3dVisible_; }

    // Shows/hides the whole volume image: the three 2D slice images follow this
    // flag directly; the 3D planes follow imageVisible_ && imagePlanes3dVisible_.
    // Crosshairs are untouched. Remembered so a later setVolume re-applies it.
    void setImageVisible(bool on)
    {
        imageVisible_ = on;
        applyImageVisibility();
    }

    bool imageVisible() const { return imageVisible_; }

    // Push the current image-visibility flags onto the resident slice actors: 2D
    // images = imageVisible_; 3D planes = imageVisible_ && imagePlanes3dVisible_.
    void applyImageVisibility()
    {
        for (int axis = 0; axis < 3; ++axis) {
            if (slices2d_[axis] != nullptr) {
                slices2d_[axis]->SetVisibility(imageVisible_ ? 1 : 0);
            }
            if (slices3d_[axis] != nullptr) {
                slices3d_[axis]->SetVisibility(
                    (imageVisible_ && imagePlanes3dVisible_) ? 1 : 0);
            }
        }
    }

    // ---- nodes ------------------------------------------------------------

    RenderStats upsertNode(const NodeId& id, const XQDataNode& node)
    {
        // Rebuild: an existing id has its old actor group removed first (payload
        // may have changed). Presentation state (visible/opacity/color) is
        // preserved across the rebuild.
        bool hadEntry = false;
        bool prevVisible = true;
        double prevOpacity = 1.0;
        double prevR = 1.0;
        double prevG = 1.0;
        double prevB = 1.0;
        auto existing = nodes_.find(id);
        if (existing != nodes_.end()) {
            hadEntry = true;
            prevVisible = existing->second.visible;
            prevOpacity = existing->second.opacity;
            prevR = existing->second.r;
            prevG = existing->second.g;
            prevB = existing->second.b;
            detachEntry(existing->second);
            nodes_.erase(existing);
        }

        NodeEntry entry;
        const RenderStats stats = buildNodeActors(node, entry);
        if (!stats.ok) {
            // Not renderable: leave no trace (even if an entry existed, the rebuild
            // failed -> the node is gone). ok=false, no actors added.
            return stats;
        }

        entry.stats = stats;
        if (hadEntry) {
            entry.visible = prevVisible;
            entry.opacity = prevOpacity;
            entry.r = prevR;
            entry.g = prevG;
            entry.b = prevB;
        } else if (node.domainType() == XQDomainType::Mesh) {
            // First sight of a volume mesh (tet wireframe): hidden by default.
            // Re-upsert keeps whatever the user set (the hadEntry branch above), so
            // a later checkbox toggle survives a payload rebuild. Ongoing
            // visibility is reconciled by the UI layer; this is only the initial
            // default.
            entry.visible = false;
        }
        applyPresentation(entry);
        nodes_.emplace(id, std::move(entry));
        return stats;
    }

    RenderStats upsertNodeProgressive(const NodeId& id, const IGeometrySource& src,
                                      GeoKind kind, const ChunkUploadSpec& spec,
                                      const ChunkProgressFn& onChunk)
    {
        bool hadEntry = false;
        bool prevVisible = true;
        double prevOpacity = 1.0;
        double prevR = 1.0;
        double prevG = 1.0;
        double prevB = 1.0;
        auto existing = nodes_.find(id);
        if (existing != nodes_.end()) {
            hadEntry = true;
            prevVisible = existing->second.visible;
            prevOpacity = existing->second.opacity;
            prevR = existing->second.r;
            prevG = existing->second.g;
            prevB = existing->second.b;
            detachEntry(existing->second);
            nodes_.erase(existing);
        }

        NodeEntry entry;
        RenderStats stats;
        if (kind == GeoKind::Surface) {
            stats = addSurfaceProgressive(src, spec, onChunk, entry);
        } else {
            stats = addVolumeMeshProgressive(src, spec, onChunk, entry);
        }
        if (!stats.ok) {
            return stats;
        }

        entry.stats = stats;
        if (hadEntry) {
            entry.visible = prevVisible;
            entry.opacity = prevOpacity;
            entry.r = prevR;
            entry.g = prevG;
            entry.b = prevB;
        } else if (kind == GeoKind::TetMesh) {
            // First sight of a tet mesh: hidden by default (see upsertNode).
            // Ongoing visibility is reconciled by the UI layer; this is only the
            // initial default.
            entry.visible = false;
        }
        applyPresentation(entry);
        nodes_.emplace(id, std::move(entry));
        return stats;
    }

    void removeNode(const NodeId& id)
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return;
        }
        detachEntry(it->second);
        nodes_.erase(it);
    }

    void clearNodes()
    {
        for (auto& kv : nodes_) {
            detachEntry(kv.second);
        }
        nodes_.clear();
    }

    void setNodeVisible(const NodeId& id, bool on)
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return;
        }
        it->second.visible = on;
        for (auto& actor : it->second.volumeActors) {
            actor->SetVisibility(on ? 1 : 0);
        }
        for (auto& actor : it->second.lodActors) {
            actor->SetVisibility(on ? 1 : 0);
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (it->second.cutActors[axis] != nullptr) {
                it->second.cutActors[axis]->SetVisibility(on ? 1 : 0);
            }
            if (it->second.contourOverlayActors[axis] != nullptr) {
                it->second.contourOverlayActors[axis]->SetVisibility(on ? 1 : 0);
            }
        }
    }

    void setNodeOpacity(const NodeId& id, double a)
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return;
        }
        it->second.opacity = a;
        for (auto& actor : it->second.volumeActors) {
            actor->GetProperty()->SetOpacity(a);
        }
        for (auto& actor : it->second.lodActors) {
            actor->GetProperty()->SetOpacity(a);
        }
    }

    void setNodeColor(const NodeId& id, double r, double g, double b)
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return;
        }
        it->second.r = r;
        it->second.g = g;
        it->second.b = b;
        // faceId-coloured surfaces keep their LUT; color is only a fallback when
        // there is no LUT (design §3). Plain actors take the colour directly.
        if (it->second.faceIdColoured) {
            return;
        }
        for (auto& actor : it->second.volumeActors) {
            actor->GetProperty()->SetColor(r, g, b);
        }
        // Slice-cut outlines and contour overlays track the node colour.
        for (int axis = 0; axis < 3; ++axis) {
            if (it->second.cutActors[axis] != nullptr) {
                it->second.cutActors[axis]->GetProperty()->SetColor(r, g, b);
            }
            if (it->second.contourOverlayActors[axis] != nullptr) {
                it->second.contourOverlayActors[axis]->GetProperty()->SetColor(r, g, b);
            }
        }
    }

    int nodeActorCount(ViewId v) const
    {
        if (v != ViewId::Volume3D) {
            return 0; // B1a: node actors live only in Volume3D
        }
        int count = 0;
        for (const auto& kv : nodes_) {
            count += static_cast<int>(kv.second.volumeActors.size());
            count += static_cast<int>(kv.second.lodActors.size());
        }
        return count;
    }

    bool hasNode(const NodeId& id) const { return nodes_.find(id) != nodes_.end(); }

    bool nodeVisible(const NodeId& id) const
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return false;
        }
        // Read the actors' real GetVisibility (which SetVisibility drives), not a
        // bookkeeping bool -- so the probe reflects the true render state and a
        // broken setNodeVisible is observable in a headless test. entry.visible is
        // the fallback only when there is no actor yet.
        const NodeEntry& e = it->second;
        if (!e.volumeActors.empty()) {
            return e.volumeActors.front()->GetVisibility() != 0;
        }
        if (!e.lodActors.empty()) {
            return e.lodActors.front()->GetVisibility() != 0;
        }
        return e.visible;
    }

    long long uploadedPointCount(const NodeId& id) const
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return -1;
        }
        return it->second.stats.uploadedPointCount;
    }

    // Number of slice-cut actors this node holds in slice view `v` (0 or 1). The
    // 3D view / an unknown id / a node with no cut overlay all give 0. Each slice
    // view maps to exactly one axis, so at most one cut actor lives there.
    int nodeSliceCutCount(const NodeId& id, ViewId v) const
    {
        if (v == ViewId::Volume3D) {
            return 0;
        }
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return 0;
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (viewForAxis(axis) == v) {
                return it->second.cutActors[axis] != nullptr ? 1 : 0;
            }
        }
        return 0;
    }

    // Contour-overlay hit indicator for a slice view: 1 when the contour-group
    // node currently shows at least one contour in that view (centroid within
    // spacing/2 of the slice), else 0. 0 for the 3D view / unknown id / a
    // non-contour node. Each slice view maps to exactly one axis.
    int nodeContourOverlayCount(const NodeId& id, ViewId v) const
    {
        if (v == ViewId::Volume3D) {
            return 0;
        }
        auto it = nodes_.find(id);
        if (it == nodes_.end() || !it->second.isContourGroup) {
            return 0;
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (viewForAxis(axis) == v) {
                return it->second.contourOverlayHitCount[axis] > 0 ? 1 : 0;
            }
        }
        return 0;
    }

    // The node's current display colour (first mounted actor's property). False
    // (rgb untouched) for an unknown id or a node with no actor.
    bool nodeColor(const NodeId& id, double rgb[3]) const
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return false;
        }
        const NodeEntry& e = it->second;
        vtkActor* actor = nullptr;
        if (!e.volumeActors.empty()) {
            actor = e.volumeActors.front();
        } else if (!e.lodActors.empty()) {
            actor = e.lodActors.front();
        }
        if (actor == nullptr) {
            return false;
        }
        actor->GetProperty()->GetColor(rgb);
        return true;
    }

private:
    // Axis 0=x->Sagittal, 1=y->Coronal, 2=z->Axial.
    static ViewId viewForAxis(int axis)
    {
        switch (axis) {
        case 0:
            return ViewId::Sagittal;
        case 1:
            return ViewId::Coronal;
        default:
            return ViewId::Axial;
        }
    }

    void detachEntry(NodeEntry& entry)
    {
        vtkRenderer* vol = renderer(ViewId::Volume3D);
        for (auto& actor : entry.volumeActors) {
            vol->RemoveActor(actor);
        }
        for (auto& actor : entry.lodActors) {
            vol->RemoveActor(actor);
        }
        // Slice-cut actors live in the three 2D slice renderers, one per axis --
        // remove each from its own renderer (never the 3D view).
        for (int axis = 0; axis < 3; ++axis) {
            if (entry.cutActors[axis] != nullptr) {
                renderer(viewForAxis(axis))->RemoveViewProp(entry.cutActors[axis]);
                entry.cutActors[axis] = nullptr;
                entry.cutPlanes[axis] = nullptr;
            }
            // Contour overlays likewise live one per slice renderer.
            if (entry.contourOverlayActors[axis] != nullptr) {
                renderer(viewForAxis(axis))
                    ->RemoveViewProp(entry.contourOverlayActors[axis]);
                entry.contourOverlayActors[axis] = nullptr;
                entry.contourOverlayHitCount[axis] = 0;
            }
        }
        entry.volumeActors.clear();
        entry.lodActors.clear();
    }

    void applyPresentation(NodeEntry& entry)
    {
        for (auto& actor : entry.volumeActors) {
            actor->SetVisibility(entry.visible ? 1 : 0);
            actor->GetProperty()->SetOpacity(entry.opacity);
            if (!entry.faceIdColoured) {
                actor->GetProperty()->SetColor(entry.r, entry.g, entry.b);
            }
        }
        for (auto& actor : entry.lodActors) {
            actor->SetVisibility(entry.visible ? 1 : 0);
            actor->GetProperty()->SetOpacity(entry.opacity);
        }
        // Slice-cut outlines and contour overlays follow the node's visibility and
        // colour (neither is ever faceId-coloured).
        for (int axis = 0; axis < 3; ++axis) {
            if (entry.cutActors[axis] != nullptr) {
                entry.cutActors[axis]->SetVisibility(entry.visible ? 1 : 0);
                entry.cutActors[axis]->GetProperty()->SetColor(entry.r, entry.g, entry.b);
            }
            if (entry.contourOverlayActors[axis] != nullptr) {
                entry.contourOverlayActors[axis]->SetVisibility(entry.visible ? 1 : 0);
                entry.contourOverlayActors[axis]->GetProperty()->SetColor(
                    entry.r, entry.g, entry.b);
            }
        }
    }

    // Payload dispatch: mirrors XQMainWindow.cpp:2156-2227 but without the
    // lazy-resolve branch (that is the app layer's job -> upsertNodeProgressive)
    // and without the LodOptions strategy constant (LodOptions defaults are
    // embedded here; the app passes tuned options in B1b). The Mesh memory
    // fallback chain (volumeTets -> surfaceTriangles) is kept.
    RenderStats buildNodeActors(const XQDataNode& node, NodeEntry& entry)
    {
        if (!node.payload()) {
            return {};
        }

        switch (node.domainType()) {
        case XQDomainType::SurfaceModel: {
            const auto payload =
                std::static_pointer_cast<XQSurfaceModelPayload>(node.payload());
            const XQSurfaceModel& model = payload->model();
            if (model.hasTriangleGeometry()) {
                return addSurface(*model.triangleGeometry(), entry);
            }
            return {};
        }
        case XQDomainType::Mesh: {
            const auto payload = std::static_pointer_cast<XQMeshPayload>(node.payload());
            const XQMesh& mesh = payload->mesh();
            if (mesh.hasVolumeTets()) {
                return addVolumeMesh(*mesh.volumeTets(), entry);
            }
            if (mesh.hasSurfaceTriangles()) {
                return addSurface(*mesh.surfaceTriangles(), entry);
            }
            return {};
        }
        case XQDomainType::Path: {
            const auto payload = std::static_pointer_cast<XQPathPayload>(node.payload());
            return addPath(*payload, entry);
        }
        case XQDomainType::SegmentationMask: {
            const auto payload =
                std::static_pointer_cast<XQSegmentationMaskPayload>(node.payload());
            return addSegmentationMask(*payload, entry);
        }
        case XQDomainType::FlowResult: {
            const auto payload =
                std::static_pointer_cast<XQFlowResultPayload>(node.payload());
            return addFlowResult(*payload, entry);
        }
        case XQDomainType::ContourGroup: {
            const auto payload =
                std::static_pointer_cast<XQContourGroupPayload>(node.payload());
            return addContourGroup(payload->group(), entry);
        }
        default:
            // Image / SimulationCase / AiAnalysis / Unknown: no geometry to show.
            return {};
        }
    }

    // Adds an actor to the Volume3D renderer and records it in the entry.
    void mountVolumeActor(NodeEntry& entry, vtkSmartPointer<vtkActor> actor)
    {
        renderer(ViewId::Volume3D)->AddActor(actor);
        entry.volumeActors.push_back(actor);
    }

    void mountLodActorInto(NodeEntry& entry, vtkSmartPointer<vtkLODActor> actor)
    {
        renderer(ViewId::Volume3D)->AddActor(actor);
        entry.lodActors.push_back(actor);
    }

    int volume3dActorCount() const { return nodeActorCount(ViewId::Volume3D); }

    RenderStats addPath(const XQPathPayload& payload, NodeEntry& entry)
    {
        const XQPath& path = payload.path();

        std::vector<Point3> worldPoints;
        const std::vector<PathSamplePoint>& samples = path.samplePoints();
        if (!samples.empty()) {
            worldPoints.reserve(samples.size());
            for (const PathSamplePoint& sample : samples) {
                worldPoints.push_back(sample.position);
            }
        } else {
            const std::vector<PathControlPoint>& controls = path.controlPoints();
            worldPoints.reserve(controls.size());
            for (const PathControlPoint& control : controls) {
                worldPoints.push_back(control.position);
            }
        }

        if (worldPoints.empty()) {
            return {};
        }

        vtkSmartPointer<vtkPolyData> polyline = build_polyline(worldPoints);

        // The path is a thin poly-line, not a tube: a tube filter sized off the
        // bounding-box diagonal made long centrelines read thicker than the vessel
        // model itself. Feed the poly-line straight to the mapper and draw it as a
        // 2px line (same treatment as the slice-cut outlines above).
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(polyline);
        mapper->ScalarVisibilityOff();

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.95, 0.75, 0.2);
        actor->GetProperty()->SetLineWidth(2.0);
        mountVolumeActor(entry, actor);

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = static_cast<long long>(worldPoints.size());
        stats.uploadedPointCount = static_cast<long long>(worldPoints.size());
        return stats;
    }

    RenderStats addSurface(const XQTriangleSurfaceGeometryHandle& surface, NodeEntry& entry)
    {
        // Consume through the Source abstraction (M9a); aliasing shared_ptr with a
        // no-op deleter, build_surface copies out (copy-on-upload).
        ResidentSurfaceSource source(
            std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
                std::shared_ptr<const void>(), &surface));
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.triangleCount == 0) {
            return {};
        }

        vtkSmartPointer<vtkPolyData> full = build_surface(source);
        if (full == nullptr) {
            return {};
        }

        // faceId cell data stays on `full` (preserved for a future colour toggle),
        // but B2c mounts surfaces single-colour: makeSurfaceMapper with
        // faceIdValid=false takes the ScalarVisibilityOff (flat) branch, and the
        // actor wears the anatomical green so the model reads as one solid piece.
        const long long srcPts = static_cast<long long>(surface.pointCount());

        double faceRange[2] = {0.0, 0.0};
        vtkSmartPointer<vtkPolyDataMapper> mapper =
            makeSurfaceMapper(full, /*faceIdValid=*/false, faceRange);
        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(kSurfaceColor[0], kSurfaceColor[1], kSurfaceColor[2]);
        mountVolumeActor(entry, actor);
        // Not faceId-coloured: setNodeColor now applies to the model, and the
        // green survives applyPresentation via entry's colour (below).
        entry.faceIdColoured = false;
        entry.r = kSurfaceColor[0];
        entry.g = kSurfaceColor[1];
        entry.b = kSurfaceColor[2];

        // Slice-cut overlay: one resident cutter/actor per slice view, driven off
        // the raw poly (`full`, pre-normals -- outlines need no shading).
        buildSliceCutActors(entry, full);

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = srcPts;
        stats.uploadedPointCount = srcPts;
        return stats;
    }

    // Builds the three resident slice-cut actors for a surface node. Each axis
    // gets a persistent vtkPlane (normal = axis unit vector, origin = that axis's
    // current slice world coord, or 0 with no volume yet -- setVolume re-origins
    // them) feeding a vtkCutter over the raw surface poly. A slice move only moves
    // the plane; the cutter recomputes lazily (no rebuild).
    void buildSliceCutActors(NodeEntry& entry, vtkPolyData* full)
    {
        for (int axis = 0; axis < 3; ++axis) {
            vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
            double normal[3] = {0.0, 0.0, 0.0};
            normal[axis] = 1.0;
            plane->SetNormal(normal);
            double origin[3] = {0.0, 0.0, 0.0};
            if (image_ != nullptr) {
                origin[axis] = sliceWorld(axis);
            }
            plane->SetOrigin(origin);

            vtkNew<vtkCutter> cutter;
            cutter->SetCutFunction(plane);
            cutter->SetInputData(full);

            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputConnection(cutter->GetOutputPort());
            mapper->ScalarVisibilityOff();

            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetColor(kSurfaceColor[0], kSurfaceColor[1], kSurfaceColor[2]);
            actor->GetProperty()->SetLineWidth(2.0);
            actor->GetProperty()->SetLighting(false);
            actor->SetVisibility(entry.visible ? 1 : 0);

            renderer(viewForAxis(axis))->AddViewProp(actor);
            entry.cutActors[axis] = actor;
            entry.cutPlanes[axis] = plane;
        }
    }

    RenderStats addVolumeMesh(const XQTetVolumeMeshHandle& mesh, NodeEntry& entry)
    {
        ResidentTetSource source(
            std::shared_ptr<const XQTetVolumeMeshHandle>(
                std::shared_ptr<const void>(), &mesh));
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.tetCount == 0) {
            return {};
        }

        vtkSmartPointer<vtkUnstructuredGrid> grid = build_volume(source);
        if (grid == nullptr) {
            return {};
        }
        vtkSmartPointer<vtkActor> actor = makeVolumeWireframeActor(grid);
        mountVolumeActor(entry, actor);

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = static_cast<long long>(mesh.pointCount());
        stats.uploadedPointCount = static_cast<long long>(mesh.pointCount());
        return stats;
    }

    // Boundary-edges wireframe actor for a tet grid (shared by resident +
    // progressive tet paths).
    vtkSmartPointer<vtkActor> makeVolumeWireframeActor(vtkUnstructuredGrid* grid)
    {
        vtkNew<vtkGeometryFilter> boundary;
        boundary->SetInputData(grid);

        vtkNew<vtkPolyDataNormals> normals;
        normals->SetInputConnection(boundary->GetOutputPort());
        normals->SetAutoOrientNormals(true);
        normals->SetConsistency(true);
        normals->SplittingOff();

        vtkNew<vtkExtractEdges> edges;
        edges->SetInputConnection(normals->GetOutputPort());

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(edges->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.4, 0.85, 0.95);
        actor->GetProperty()->SetRepresentationToWireframe();
        return actor;
    }

    RenderStats addFlowResult(const XQFlowResultPayload& payload, NodeEntry& entry)
    {
        const XQFlowResult& result = payload.result();
        const std::vector<FlowSegment>& segments = result.segments();
        if (segments.empty()) {
            return {};
        }

        std::vector<Point3> layout;
        layout.reserve(segments.size());
        for (std::size_t i = 0; i < segments.size(); ++i) {
            layout.push_back({static_cast<double>(i), 0.0, 0.0});
        }

        vtkSmartPointer<vtkPolyData> polyline = build_polyline(layout);

        vtkNew<vtkDoubleArray> pressure;
        pressure->SetName("pressure");
        pressure->SetNumberOfComponents(1);
        const std::vector<std::vector<double>>& pressureP = result.pressureP();
        for (std::size_t i = 0; i < segments.size(); ++i) {
            double value = 0.0;
            if (i < pressureP.size() && !pressureP[i].empty()) {
                value = pressureP[i].back();
            }
            pressure->InsertNextValue(value);
        }
        polyline->GetPointData()->SetScalars(pressure);

        double pressureRange[2] = {0.0, 0.0};
        pressure->GetRange(pressureRange);
        const double upper =
            pressureRange[1] > pressureRange[0] ? pressureRange[1] : pressureRange[0] + 1.0;

        vtkNew<vtkLookupTable> lut;
        lut->SetHueRange(0.667, 0.0);
        lut->SetTableRange(pressureRange[0], upper);
        lut->Build();

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(polyline);
        mapper->SetScalarModeToUsePointData();
        mapper->SetColorModeToMapScalars();
        mapper->SetLookupTable(lut);
        mapper->SetScalarRange(pressureRange[0], upper);

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetLineWidth(3.0);
        mountVolumeActor(entry, actor);
        entry.faceIdColoured = true; // scalar-mapped: keep LUT, color is fallback

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = static_cast<long long>(segments.size());
        stats.uploadedPointCount = static_cast<long long>(segments.size());
        return stats;
    }

    RenderStats addSegmentationMask(const XQSegmentationMaskPayload& payload, NodeEntry& entry)
    {
        const XQSegmentationMask& mask = payload.mask();
        if (!mask.is_valid()) {
            return {};
        }

        const int dimX = mask.dimensionX();
        const int dimY = mask.dimensionY();
        const int dimZ = mask.dimensionZ();

        const bool hasGeometry = mask.hasGeometry();
        ImageGeometry geometry = {};
        if (hasGeometry) {
            geometry = mask.geometry();
        }

        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        vtkIdType nnz = 0;
        for (int z = 0; z < dimZ; ++z) {
            for (int y = 0; y < dimY; ++y) {
                for (int x = 0; x < dimX; ++x) {
                    if (mask.labelAt(mask.voxelIndex(x, y, z)) != 0) {
                        ++nnz;
                    }
                }
            }
        }
        points->SetNumberOfPoints(nnz);
        vtkIdType pi = 0;
        for (int z = 0; z < dimZ; ++z) {
            for (int y = 0; y < dimY; ++y) {
                for (int x = 0; x < dimX; ++x) {
                    const std::size_t index = mask.voxelIndex(x, y, z);
                    if (mask.labelAt(index) == 0) {
                        continue;
                    }
                    double world[3] = {static_cast<double>(x),
                                       static_cast<double>(y),
                                       static_cast<double>(z)};
                    if (hasGeometry) {
                        voxel_to_world(geometry, x, y, z, world);
                    }
                    points->SetPoint(pi++, world[0], world[1], world[2]);
                }
            }
        }

        const vtkIdType foreground = points->GetNumberOfPoints();
        if (foreground == 0) {
            return {};
        }

        vtkSmartPointer<vtkPolyData> cloud = vtkSmartPointer<vtkPolyData>::New();
        cloud->SetPoints(points);

        vtkNew<vtkVertexGlyphFilter> glyph;
        glyph->SetInputData(cloud);

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(glyph->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.9, 0.3, 0.3);
        actor->GetProperty()->SetPointSize(3.0);
        mountVolumeActor(entry, actor);

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = static_cast<long long>(foreground);
        stats.uploadedPointCount = static_cast<long long>(foreground);
        return stats;
    }

    // Assembles a contour-group node: a single 3D actor drawing every closed
    // contour as a bright-green closed polyline, plus one resident overlay actor
    // per slice view showing the contours whose centroid is near that view's
    // current slice plane (rebuilt on a slice move). Contour points are already
    // world coordinates (reader contract). ok=false with no contours at all.
    RenderStats addContourGroup(const XQContourGroup& group, NodeEntry& entry)
    {
        const std::vector<XQContour>& contours = group.contours();
        long long totalPoints = 0;
        entry.isContourGroup = true;
        entry.contours.clear();
        entry.contours.reserve(contours.size());
        for (const XQContour& contour : contours) {
            if (contour.points.empty()) {
                continue;
            }
            ContourCurve curve;
            curve.points = contour.points;
            curve.closed = contour.closed;
            curve.centroid = contour_centroid(contour.points);
            totalPoints += static_cast<long long>(contour.points.size());
            entry.contours.push_back(std::move(curve));
        }
        if (entry.contours.empty()) {
            return {};
        }

        // 3D view: every closed contour as one closed polyline, all in one actor.
        std::vector<const ContourCurve*> closedCurves;
        for (const ContourCurve& c : entry.contours) {
            if (c.closed) {
                closedCurves.push_back(&c);
            }
        }
        // Fall back to drawing all curves in 3D when none are flagged closed, so
        // an open-contour group is still visible rather than an empty 3D actor.
        if (closedCurves.empty()) {
            for (const ContourCurve& c : entry.contours) {
                closedCurves.push_back(&c);
            }
        }
        vtkSmartPointer<vtkPolyData> volumePoly = build_contour_polydata(closedCurves);
        vtkNew<vtkPolyDataMapper> volumeMapper;
        volumeMapper->SetInputData(volumePoly);
        volumeMapper->ScalarVisibilityOff();
        vtkSmartPointer<vtkActor> volumeActor = vtkSmartPointer<vtkActor>::New();
        volumeActor->SetMapper(volumeMapper);
        volumeActor->GetProperty()->SetColor(kContourColor[0], kContourColor[1],
                                             kContourColor[2]);
        volumeActor->GetProperty()->SetLineWidth(1.5);
        volumeActor->GetProperty()->SetLighting(false);
        mountVolumeActor(entry, volumeActor);

        // Per slice view: a resident (initially empty) overlay actor; the slice
        // filter fills its polydata now and on every slice move.
        for (int axis = 0; axis < 3; ++axis) {
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->ScalarVisibilityOff();
            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetColor(kContourColor[0], kContourColor[1],
                                           kContourColor[2]);
            actor->GetProperty()->SetLineWidth(1.5);
            actor->GetProperty()->SetLighting(false);
            renderer(viewForAxis(axis))->AddViewProp(actor);
            entry.contourOverlayActors[axis] = actor;
        }
        rebuildContourOverlays(entry);

        entry.faceIdColoured = false;
        entry.r = kContourColor[0];
        entry.g = kContourColor[1];
        entry.b = kContourColor[2];

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = totalPoints;
        stats.uploadedPointCount = totalPoints;
        return stats;
    }

    // Re-filters one contour-group entry's curves for one slice axis and rebuilds
    // that view's overlay polydata: curves whose centroid is within spacing/2 of
    // the plane along that axis. With no volume the view shows nothing (no
    // meaningful slice plane). The single source of truth for the distance filter.
    void filterContourOverlay(NodeEntry& entry, int axis)
    {
        if (!entry.isContourGroup || axis < 0 || axis > 2) {
            return;
        }
        vtkActor* actor = entry.contourOverlayActors[axis];
        if (actor == nullptr) {
            return;
        }
        std::vector<const ContourCurve*> hits;
        if (image_ != nullptr) {
            const double planeCoord = sliceWorld(axis);
            const double halfSpacing = imageGeometry_.spacing[axis] * 0.5;
            for (const ContourCurve& c : entry.contours) {
                const double centroidCoord =
                    (axis == 0) ? c.centroid.x
                                : (axis == 1) ? c.centroid.y : c.centroid.z;
                if (std::abs(centroidCoord - planeCoord) < halfSpacing) {
                    hits.push_back(&c);
                }
            }
        }
        entry.contourOverlayHitCount[axis] = static_cast<int>(hits.size());
        vtkSmartPointer<vtkPolyData> poly = build_contour_polydata(hits);
        vtkPolyDataMapper* mapper =
            vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
        if (mapper != nullptr) {
            mapper->SetInputData(poly);
        }
    }

    // Re-filters one entry across all three slice axes (used at build time / after
    // a volume reload).
    void rebuildContourOverlays(NodeEntry& entry)
    {
        for (int axis = 0; axis < 3; ++axis) {
            filterContourOverlay(entry, axis);
        }
    }

    // Re-filters every contour-group node's overlay for a single slice axis (used
    // on a slice move, scoped to the axis that moved).
    void rebuildContourOverlaysForAxis(int axis)
    {
        for (auto& kv : nodes_) {
            filterContourOverlay(kv.second, axis);
        }
    }

    // Re-filters every contour-group node's slice overlays across all axes. Called
    // after a volume (re)build so the overlays track the fresh slices.
    void rebuildAllContourOverlays()
    {
        for (auto& kv : nodes_) {
            rebuildContourOverlays(kv.second);
        }
    }

    // Progressive surface upload (M9b-C): splits the source triangles into
    // contiguous chunks and builds one actor per chunk. connectivity bounds gate:
    // invalid connectivity -> ok=false, zero actors, zero chunks, onChunk never
    // fires (spec scene contract).
    RenderStats addSurfaceProgressive(const IGeometrySource& source,
                                      const ChunkUploadSpec& spec,
                                      const ChunkProgressFn& onChunk,
                                      NodeEntry& entry)
    {
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.triangleCount == 0) {
            return {};
        }

        GeometryLease<Point3> pointLease = source.acquire_points();
        TriangleLease triLease = source.acquire_triangles();
        const ReadSpan<Point3>& pts = pointLease.span();
        const TriangleView& triView = triLease.view();
        const ReadSpan<SourceTriangle>& tris = triView.triangles;
        const ReadSpan<int>& faceIds = triView.faceIds;

        double faceRange[2] = {0.0, 0.0};
        if (!faceIds.empty()) {
            faceRange[0] = faceRange[1] = static_cast<double>(faceIds[0]);
            for (std::size_t i = 1; i < faceIds.size(); ++i) {
                const double v = static_cast<double>(faceIds[i]);
                faceRange[0] = std::min(faceRange[0], v);
                faceRange[1] = std::max(faceRange[1], v);
            }
        }

        const long long srcPts = static_cast<long long>(meta.pointCount);
        const int* flatConn = reinterpret_cast<const int*>(tris.data());
        if (!connectivity_in_bounds(flatConn, tris.size(), 3, pts.size())) {
            return {};
        }

        const std::vector<ChunkRange> chunks =
            plan_chunks(tris.size(), spec.maxCellsPerChunk);
        const int chunkCount = static_cast<int>(chunks.size());

        std::vector<int> remap(meta.pointCount, -1);

        int completed = 0;
        for (const ChunkRange& range : chunks) {
            ChunkMesh mesh = build_cell_chunk(pts, flatConn, range, 3, remap);

            vtkNew<vtkFloatArray> chunkFaceIds;
            chunkFaceIds->SetName("faceId");
            chunkFaceIds->SetNumberOfComponents(1);
            chunkFaceIds->SetNumberOfTuples(static_cast<vtkIdType>(mesh.cellCount));
            for (std::size_t j = 0; j < mesh.cellCount; ++j) {
                const std::size_t g = range.begin + j;
                const float fid = g < faceIds.size() ? static_cast<float>(faceIds[g]) : 0.0f;
                chunkFaceIds->SetValue(static_cast<vtkIdType>(j), fid);
            }

            vtkSmartPointer<vtkPolyData> poly = vtkSmartPointer<vtkPolyData>::New();
            poly->SetPoints(mesh.points);
            poly->SetPolys(mesh.cells);
            poly->GetCellData()->SetScalars(chunkFaceIds);

            if (spec.lod.interactive) {
                const LodLevels levels = SurfaceLodBuilder::buildSync(poly, faceIds);
                if (levels.ok) {
                    mountLodChunk(entry, levels, faceRange);
                } else {
                    // B2c: single-colour green (faceIdValid=false -> flat branch);
                    // faceId cell data stays on the poly for a future toggle.
                    vtkSmartPointer<vtkPolyDataMapper> mapper =
                        makeSurfaceMapper(poly, /*faceIdValid=*/false, faceRange);
                    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
                    actor->SetMapper(mapper);
                    actor->GetProperty()->SetColor(kSurfaceColor[0], kSurfaceColor[1], kSurfaceColor[2]);
                    mountVolumeActor(entry, actor);
                }
            } else {
                vtkSmartPointer<vtkPolyDataMapper> mapper =
                    makeSurfaceMapper(poly, /*faceIdValid=*/false, faceRange);
                vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
                actor->SetMapper(mapper);
                actor->GetProperty()->SetColor(kSurfaceColor[0], kSurfaceColor[1], kSurfaceColor[2]);
                mountVolumeActor(entry, actor);
            }

            ++completed;
            if (onChunk) {
                RenderStats cum;
                cum.ok = true;
                cum.actorCount = volume3dActorCount();
                cum.pointCount = srcPts;
                cum.uploadedPointCount = srcPts;
                cum.chunkCount = chunkCount;
                cum.completedChunkCount = completed;
                onChunk(cum);
            }
        }

        // Single-colour green. faceIdColoured=false so applyPresentation
        // paints the plain chunk actors with entry's colour (LOD actors are
        // coloured directly in mountLodChunk). The progressive path builds no
        // slice-cut actors: the real SV project loads via addSurface, so cut
        // outlines are only wired there. A progressive surface that needs slice
        // cuts would have to wire them here.
        entry.faceIdColoured = false;
        entry.r = kSurfaceColor[0];
        entry.g = kSurfaceColor[1];
        entry.b = kSurfaceColor[2];
        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = srcPts;
        stats.uploadedPointCount = srcPts;
        stats.chunkCount = chunkCount;
        stats.completedChunkCount = completed;
        return stats;
    }

    void mountLodChunk(NodeEntry& entry, const LodLevels& levels, const double faceRange[2])
    {
        // B2c: LOD chunks mount single-colour green too (flat branch at every
        // level); faceRange is unused now but kept in the signature for the future
        // colour toggle.
        (void)faceRange;
        const double flat[2] = {0.0, 0.0};
        vtkSmartPointer<vtkLODActor> actor = vtkSmartPointer<vtkLODActor>::New();
        actor->SetMapper(makeSurfaceMapper(levels.level[0].poly, /*faceIdValid=*/false, flat));
        for (int k = 1; k < 3; ++k) {
            if (levels.level[k].poly != nullptr && levels.level[k].triangleCount > 0) {
                actor->AddLODMapper(
                    makeSurfaceMapper(levels.level[k].poly, /*faceIdValid=*/false, flat));
            }
        }
        actor->GetProperty()->SetColor(kSurfaceColor[0], kSurfaceColor[1], kSurfaceColor[2]);
        mountLodActorInto(entry, actor);
    }

    RenderStats addVolumeMeshProgressive(const IGeometrySource& source,
                                         const ChunkUploadSpec& spec,
                                         const ChunkProgressFn& onChunk,
                                         NodeEntry& entry)
    {
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.tetCount == 0) {
            return {};
        }

        GeometryLease<Point3> pointLease = source.acquire_points();
        GeometryLease<SourceTet> tetLease = source.acquire_tetrahedra();
        const ReadSpan<Point3>& pts = pointLease.span();
        const ReadSpan<SourceTet>& tets = tetLease.span();

        const long long srcPts = static_cast<long long>(meta.pointCount);
        const int* flatConn = reinterpret_cast<const int*>(tets.data());
        if (!connectivity_in_bounds(flatConn, tets.size(), 4, pts.size())) {
            return {};
        }

        const std::vector<ChunkRange> chunks =
            plan_chunks(tets.size(), spec.maxCellsPerChunk);
        const int chunkCount = static_cast<int>(chunks.size());

        std::vector<int> remap(meta.pointCount, -1);

        int completed = 0;
        for (const ChunkRange& range : chunks) {
            ChunkMesh mesh = build_cell_chunk(pts, flatConn, range, 4, remap);

            vtkSmartPointer<vtkUnstructuredGrid> grid =
                vtkSmartPointer<vtkUnstructuredGrid>::New();
            grid->SetPoints(mesh.points);
            grid->SetCells(VTK_TETRA, mesh.cells);
            vtkSmartPointer<vtkActor> actor = makeVolumeWireframeActor(grid);
            mountVolumeActor(entry, actor);

            ++completed;
            if (onChunk) {
                RenderStats cum;
                cum.ok = true;
                cum.actorCount = volume3dActorCount();
                cum.pointCount = srcPts;
                cum.uploadedPointCount = srcPts;
                cum.chunkCount = chunkCount;
                cum.completedChunkCount = completed;
                onChunk(cum);
            }
        }

        RenderStats stats;
        stats.ok = true;
        stats.actorCount = volume3dActorCount();
        stats.pointCount = srcPts;
        stats.uploadedPointCount = srcPts;
        stats.chunkCount = chunkCount;
        stats.completedChunkCount = completed;
        return stats;
    }

    // ---- crosshairs -------------------------------------------------------
    // Each 2D slice renderer gets two orthogonal line actors. Endpoints span the
    // image bounds; positions are driven by the other two axes' current slice
    // world coordinates.
    //
    // A crosshair line's colour follows the axis (== the orthogonal slice plane)
    // it represents, matching that view's frame colour: Sagittal/x = green,
    // Coronal/y = blue, Axial/z = red. crosshairLineAxis() is the single source of
    // truth for the (viewAxis, line) -> represented-axis mapping; build / update /
    // the drag probes all read it, so the geometry in updateCrosshairs() and the
    // colours here can never drift.

    // (viewAxis, line) -> the axis (== slice plane) that line represents.
    //   Axial   (2): line 0 -> axis 0 (Sagittal), line 1 -> axis 1 (Coronal)
    //   Sagittal(0): line 0 -> axis 1 (Coronal),  line 1 -> axis 2 (Axial)
    //   Coronal (1): line 0 -> axis 0 (Sagittal), line 1 -> axis 2 (Axial)
    static int crosshairLineAxis(int viewAxis, int line)
    {
        switch (viewAxis) {
        case 2: // Axial
            return line == 0 ? 0 : 1;
        case 0: // Sagittal
            return line == 0 ? 1 : 2;
        case 1: // Coronal
            return line == 0 ? 0 : 2;
        default:
            return -1;
        }
    }

    // The axis colour (rgb 0-1) for a represented axis: 0=Sagittal green,
    // 1=Coronal blue, 2=Axial red -- the window frame colours.
    static void axisColor(int axis, double rgb[3])
    {
        switch (axis) {
        case 0: // Sagittal -> green #3C9C4A
            rgb[0] = 0.235; rgb[1] = 0.612; rgb[2] = 0.290;
            break;
        case 1: // Coronal -> blue #3C6CC4
            rgb[0] = 0.235; rgb[1] = 0.424; rgb[2] = 0.769;
            break;
        default: // Axial -> red #C43C3C
            rgb[0] = 0.769; rgb[1] = 0.235; rgb[2] = 0.235;
            break;
        }
    }

    // Builds the four resident seed-marker spheres (one per view, sharing a
    // centre), radius = 2x mean spacing, lighting-flat bright red, hidden until a
    // seed is picked. Called after a volume lands (needs spacing for the radius);
    // clearVolume removes them.
    void buildSeedMarkers()
    {
        const double radius =
            (imageGeometry_.spacing[0] + imageGeometry_.spacing[1]
             + imageGeometry_.spacing[2])
            / 3.0 * 2.0;
        const ViewId views[4] = {ViewId::Axial, ViewId::Sagittal, ViewId::Coronal,
                                 ViewId::Volume3D};
        for (int v = 0; v < 4; ++v) {
            vtkSmartPointer<vtkSphereSource> src =
                vtkSmartPointer<vtkSphereSource>::New();
            src->SetRadius(radius > 0.0 ? radius : 1.0);
            src->SetThetaResolution(16);
            src->SetPhiResolution(16);
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputConnection(src->GetOutputPort());
            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetColor(0.9, 0.2, 0.2);
            actor->GetProperty()->LightingOff();
            actor->SetVisibility(0);
            renderer(views[v])->AddViewProp(actor);
            seedMarkerSources_[v] = src;
            seedMarkerActors_[v] = actor;
        }
    }

    // Builds the four resident path-control-point glyph actors (one per view). A
    // small sphere is glyphed onto every point of a per-view vtkPoints set, so one
    // actor renders all current draft control points. Cyan (distinct from the red
    // seed sphere), lighting-flat, radius slightly under the seed marker, hidden
    // and empty until setPathControlPoints. Called after a volume lands (needs
    // spacing for the radius); clearVolume removes them.
    void buildPathControlMarkers()
    {
        const double radius =
            (imageGeometry_.spacing[0] + imageGeometry_.spacing[1]
             + imageGeometry_.spacing[2])
            / 3.0 * 1.5;
        const ViewId views[4] = {ViewId::Axial, ViewId::Sagittal, ViewId::Coronal,
                                 ViewId::Volume3D};
        for (int v = 0; v < 4; ++v) {
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkPolyData> poly = vtkSmartPointer<vtkPolyData>::New();
            poly->SetPoints(points);

            vtkSmartPointer<vtkSphereSource> src =
                vtkSmartPointer<vtkSphereSource>::New();
            src->SetRadius(radius > 0.0 ? radius : 1.0);
            src->SetThetaResolution(12);
            src->SetPhiResolution(12);

            vtkSmartPointer<vtkGlyph3D> glyph = vtkSmartPointer<vtkGlyph3D>::New();
            glyph->SetInputData(poly);
            glyph->SetSourceConnection(src->GetOutputPort());
            glyph->ScalingOff();

            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputConnection(glyph->GetOutputPort());
            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetColor(0.2, 0.7, 0.9);
            actor->GetProperty()->LightingOff();
            actor->SetVisibility(0);
            renderer(views[v])->AddViewProp(actor);
            pathControlPoints_[v] = points;
            pathControlPoly_[v] = poly;
            pathControlActors_[v] = actor;
        }
        pathControlCount_ = 0;
    }

    void buildCrosshairs()
    {
        for (int axis = 0; axis < 3; ++axis) {
            const ViewId view = viewForAxis(axis);
            for (int line = 0; line < 2; ++line) {
                vtkSmartPointer<vtkLineSource> src = vtkSmartPointer<vtkLineSource>::New();
                vtkNew<vtkPolyDataMapper> mapper;
                mapper->SetInputConnection(src->GetOutputPort());
                vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
                actor->SetMapper(mapper);
                double rgb[3] = {0.0, 0.0, 0.0};
                axisColor(crosshairLineAxis(axis, line), rgb);
                actor->GetProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
                actor->GetProperty()->SetLineWidth(1.5);
                actor->SetVisibility(crosshairVisible_ ? 1 : 0);
                renderer(view)->AddViewProp(actor);
                crosshairSources_[axis][line] = src;
                crosshairActors_[axis][line] = actor;
            }
        }
    }

    // World coordinate along a single axis for the current slice index of that
    // axis (origin + direction*spacing*index, projected on that axis component).
    double sliceWorld(int axis) const
    {
        double world[3] = {0.0, 0.0, 0.0};
        int idx[3] = {0, 0, 0};
        idx[axis] = sliceIndex_[axis];
        voxel_to_world(imageGeometry_, idx[0], idx[1], idx[2], world);
        return world[axis];
    }

    // Public probe: current slice-plane world coordinate of `axis` (0.0 with no
    // volume). Reuses sliceWorld; the drag path in the widget queries it.
    double sliceWorldCoord(int axis) const
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return 0.0;
        }
        return sliceWorld(axis);
    }

    // Inverse of sliceWorld: a world coordinate along `axis` -> the nearest slice
    // index, clamped into the valid extent. -1 with no volume.
    //
    // Assumes an axis-aligned, diagonal-dominant direction matrix (0007 is the
    // identity), so the single-axis inverse is
    //   index = round((world - origin[axis]) / (spacing[axis]*direction[axis][axis]))
    // matching voxel_to_world when the other two voxel components are the current
    // slice indices (they cancel out of the axis component under this assumption).
    // A non-axis-aligned (rotated) direction matrix would need the full 3x3
    // inverse -- Note: not handled, as 0007 does not exercise it.
    int worldToSliceIndex(int axis, double world) const
    {
        if (image_ == nullptr || axis < 0 || axis > 2) {
            return -1;
        }
        const double origin = imageGeometry_.origin[axis];
        const double scale =
            imageGeometry_.spacing[axis] * imageGeometry_.direction[axis][axis];
        int index = 0;
        if (scale != 0.0) {
            index = static_cast<int>(std::lround((world - origin) / scale));
        }
        const int low = extent_[axis * 2];
        const int high = extent_[axis * 2 + 1];
        return std::min(std::max(index, low), high);
    }

    // A world point -> nearest voxel index on each axis (each axis reuses the
    // single-axis worldToSliceIndex inverse; on the current slice plane the click
    // axis' world coord round-trips back to its current slice index). False with
    // no volume.
    bool worldToVoxelIndex(const double world[3], int* i, int* j, int* k) const
    {
        if (image_ == nullptr) {
            return false;
        }
        int* out[3] = {i, j, k};
        for (int axis = 0; axis < 3; ++axis) {
            const int idx = worldToSliceIndex(axis, world[axis]);
            if (out[axis] != nullptr) {
                *out[axis] = idx;
            }
        }
        return true;
    }

    // Moves + shows the resident seed sphere (one actor per view, sharing the
    // voxel's world centre). No volume / no built marker -> no-op.
    void setSeedMarker(int i, int j, int k)
    {
        if (image_ == nullptr) {
            return;
        }
        double world[3] = {0.0, 0.0, 0.0};
        voxel_to_world(imageGeometry_, i, j, k, world);
        for (int v = 0; v < 4; ++v) {
            if (seedMarkerSources_[v] != nullptr) {
                seedMarkerSources_[v]->SetCenter(world[0], world[1], world[2]);
                seedMarkerSources_[v]->Update();
            }
            if (seedMarkerActors_[v] != nullptr) {
                seedMarkerActors_[v]->SetVisibility(1);
            }
        }
        seedMarkerVisible_ = true;
    }

    void clearSeedMarker()
    {
        for (int v = 0; v < 4; ++v) {
            if (seedMarkerActors_[v] != nullptr) {
                seedMarkerActors_[v]->SetVisibility(0);
            }
        }
        seedMarkerVisible_ = false;
    }

    bool seedMarkerVisible() const { return seedMarkerVisible_; }

    // Replaces every view's path-control glyph points with worldPoints (already in
    // world space, so no voxel round-trip). Each actor shows only when it has at
    // least one point. No built markers (no volume) -> no-op.
    void setPathControlPoints(const std::vector<std::array<double, 3>>& worldPoints)
    {
        // The glyph markers only exist after a volume has landed
        // (buildPathControlMarkers). With no markers built there is nothing to
        // draw, so this is a no-op and the count must stay 0 -- reporting a count
        // while nothing is rendered would let it drift from the actual scene.
        if (pathControlPoints_[0] == nullptr) {
            return;
        }
        // Keep the full world-point set; the per-view vtkPoints are a filtered
        // projection of it (see refreshPathControlMarkers): a 2D slice view only
        // shows the points near its current slice plane, the 3D view shows all.
        pathControlWorld_ = worldPoints;
        pathControlCount_ = worldPoints.size();
        refreshPathControlMarkers();
    }

    // Rebuilds each view's control-point glyph input from pathControlWorld_,
    // filtering the three 2D slice views to points whose distance to that view's
    // current slice plane is within one voxel-spacing band (so a marker only
    // appears on the slice it sits on, and moving the crosshair shows/hides it).
    // The 3D view shows every point. Cheap (a handful of points), so it is called
    // on every set and on every slice-index change.
    void refreshPathControlMarkers()
    {
        if (pathControlPoints_[0] == nullptr) {
            return;
        }
        const ViewId views[4] = {ViewId::Axial, ViewId::Sagittal, ViewId::Coronal,
                                 ViewId::Volume3D};
        for (int v = 0; v < 4; ++v) {
            if (pathControlPoints_[v] == nullptr) {
                continue;
            }
            const bool is3d = views[v] == ViewId::Volume3D;
            // For a 2D view, the axis whose slice plane it shows (its normal).
            int planeAxis = -1;
            if (!is3d) {
                for (int axis = 0; axis < 3; ++axis) {
                    if (viewForAxis(axis) == views[v]) {
                        planeAxis = axis;
                        break;
                    }
                }
            }
            const double sliceCoord = planeAxis >= 0 ? sliceWorldCoord(planeAxis) : 0.0;
            // Half a slice-spacing band on each side keeps a point visible on the
            // one slice it lands on (spacing 0 -> always visible, e.g. no volume).
            const double band = planeAxis >= 0
                ? std::max(imageGeometry_.spacing[planeAxis], 0.0)
                : 0.0;

            pathControlPoints_[v]->Reset();
            int shown = 0;
            for (const std::array<double, 3>& p : pathControlWorld_) {
                if (!is3d && band > 0.0
                    && std::abs(p[planeAxis] - sliceCoord) > band) {
                    continue; // off this slice plane
                }
                pathControlPoints_[v]->InsertNextPoint(p[0], p[1], p[2]);
                ++shown;
            }
            pathControlPoints_[v]->Modified();
            if (pathControlPoly_[v] != nullptr) {
                pathControlPoly_[v]->Modified();
            }
            if (pathControlActors_[v] != nullptr) {
                pathControlActors_[v]->SetVisibility(shown > 0 ? 1 : 0);
            }
        }
    }

    void clearPathControlPoints()
    {
        for (int v = 0; v < 4; ++v) {
            if (pathControlPoints_[v] != nullptr) {
                pathControlPoints_[v]->Reset();
                pathControlPoints_[v]->Modified();
            }
            if (pathControlPoly_[v] != nullptr) {
                pathControlPoly_[v]->Modified();
            }
            if (pathControlActors_[v] != nullptr) {
                pathControlActors_[v]->SetVisibility(0);
            }
        }
        pathControlWorld_.clear();
        pathControlCount_ = 0;
    }

    std::size_t pathControlPointCount() const { return pathControlCount_; }

    // Points currently filled into the 2D slice view's glyph input for `axis`,
    // i.e. how many markers pass the slice-distance filter right now. -1 for an
    // invalid axis.
    int pathControlVisibleCount(int axis) const
    {
        if (axis < 0 || axis > 2) {
            return -1;
        }
        // The marker arrays are indexed in the same order the build/refresh
        // helpers use: {Axial, Sagittal, Coronal, Volume3D}. Find this axis's 2D
        // view in that order (not by ViewId enum value, which differs).
        const ViewId order[4] = {ViewId::Axial, ViewId::Sagittal, ViewId::Coronal,
                                 ViewId::Volume3D};
        const ViewId view = viewForAxis(axis);
        for (int v = 0; v < 4; ++v) {
            if (order[v] == view) {
                if (pathControlPoints_[v] == nullptr) {
                    return 0;
                }
                return static_cast<int>(pathControlPoints_[v]->GetNumberOfPoints());
            }
        }
        return 0;
    }

    // Public probe: the colour (rgb 0-1) of a crosshair line. False (rgb left
    // untouched) when there is no volume / no built actor for that line.
    bool crosshairLineColor(int viewAxis, int line, double rgb[3]) const
    {
        if (image_ == nullptr || viewAxis < 0 || viewAxis > 2 || line < 0
            || line > 1 || crosshairActors_[viewAxis][line] == nullptr) {
            return false;
        }
        axisColor(crosshairLineAxis(viewAxis, line), rgb);
        return true;
    }

    void updateCrosshairs()
    {
        if (image_ == nullptr) {
            return;
        }
        double bounds[6] = {0, 0, 0, 0, 0, 0};
        image_->GetBounds(bounds);
        const double x = sliceWorld(0);
        const double y = sliceWorld(1);
        const double z = sliceWorld(2);

        // Axial (axis 2, z-plane): lines at x=const (spanning y) and y=const
        // (spanning x), drawn on the current z.
        setCrosshairLine(2, 0, x, bounds[2], z, x, bounds[3], z);
        setCrosshairLine(2, 1, bounds[0], y, z, bounds[1], y, z);

        // Sagittal (axis 0, x-plane): lines at y=const (spanning z) and z=const
        // (spanning y), drawn on the current x.
        setCrosshairLine(0, 0, x, y, bounds[4], x, y, bounds[5]);
        setCrosshairLine(0, 1, x, bounds[2], z, x, bounds[3], z);

        // Coronal (axis 1, y-plane): lines at x=const (spanning z) and z=const
        // (spanning x), drawn on the current y.
        setCrosshairLine(1, 0, x, y, bounds[4], x, y, bounds[5]);
        setCrosshairLine(1, 1, bounds[0], y, z, bounds[1], y, z);
    }

    void setCrosshairLine(int axis, int line,
                          double x1, double y1, double z1,
                          double x2, double y2, double z2)
    {
        vtkLineSource* src = crosshairSources_[axis][line];
        if (src == nullptr) {
            return;
        }
        src->SetPoint1(x1, y1, z1);
        src->SetPoint2(x2, y2, z2);
        src->Update();
    }

private:
    vtkSmartPointer<vtkRenderer> renderers_[4];

    // Cache the image geometry so crosshair world math does not go back through
    // the volume object after setVolume returns.
    ImageGeometry imageGeometry_ = {};

    // volume
    vtkSmartPointer<vtkImageData> image_;
    vtkSmartPointer<vtkImageProperty> imageProperty_;
    vtkSmartPointer<vtkImageSliceMapper> sliceMappers2d_[3];
    vtkSmartPointer<vtkImageSlice> slices2d_[3];
    vtkSmartPointer<vtkImageSliceMapper> sliceMappers3d_[3];
    vtkSmartPointer<vtkImageSlice> slices3d_[3];
    // Visibility of the three 3D slice planes (slices3d_). Remembered across
    // setVolume so a volume reload re-applies the user's choice.
    bool imagePlanes3dVisible_ = true;
    // Master visibility of the volume image (2D slice images + 3D planes).
    // Remembered across setVolume.
    bool imageVisible_ = true;
    int extent_[6] = {0, 0, 0, 0, 0, 0};
    int sliceIndex_[3] = {-1, -1, -1};

    // crosshairs
    vtkSmartPointer<vtkLineSource> crosshairSources_[3][2];
    vtkSmartPointer<vtkActor> crosshairActors_[3][2];
    bool crosshairVisible_ = true;

    // seed marker (one sphere per view, index order Axial/Sagittal/Coronal/3D)
    vtkSmartPointer<vtkSphereSource> seedMarkerSources_[4];
    vtkSmartPointer<vtkActor> seedMarkerActors_[4];
    bool seedMarkerVisible_ = false;

    // path control-point markers (one glyph actor per view, same index order);
    // every draft control point is a sphere glyph on the per-view point set.
    vtkSmartPointer<vtkPoints> pathControlPoints_[4];
    vtkSmartPointer<vtkPolyData> pathControlPoly_[4];
    vtkSmartPointer<vtkActor> pathControlActors_[4];
    // Full set of control-point world coords; the per-view vtkPoints above are a
    // slice-distance-filtered projection of this (refreshPathControlMarkers).
    std::vector<std::array<double, 3>> pathControlWorld_;
    std::size_t pathControlCount_ = 0;

    // nodes
    std::map<NodeId, NodeEntry> nodes_;
};

// --- public forwarders ------------------------------------------------------

XQRenderScene::XQRenderScene()
    : impl_(new Impl())
{
}

XQRenderScene::~XQRenderScene() = default;

bool XQRenderScene::setVolume(const XQImageVolume& img,
                              const XQMemoryImageBufferHandle* buffer)
{
    return impl_->setVolume(img, buffer);
}

void XQRenderScene::clearVolume() { impl_->clearVolume(); }
bool XQRenderScene::hasVolume() const { return impl_->hasVolume(); }

void XQRenderScene::setSliceIndex(int axis, int index) { impl_->setSliceIndex(axis, index); }
int XQRenderScene::sliceIndex(int axis) const { return impl_->sliceIndex(axis); }
int XQRenderScene::sliceCount(int axis) const { return impl_->sliceCount(axis); }

void XQRenderScene::setWindowLevel(double window, double level)
{
    impl_->setWindowLevel(window, level);
}

void XQRenderScene::windowLevel(double* window, double* level) const
{
    impl_->windowLevel(window, level);
}

void XQRenderScene::setCrosshairVisible(bool on) { impl_->setCrosshairVisible(on); }
bool XQRenderScene::crosshairVisible() const { return impl_->crosshairVisible(); }

void XQRenderScene::setImagePlanesVisible3d(bool on) { impl_->setImagePlanesVisible3d(on); }
bool XQRenderScene::imagePlanesVisible3d() const { return impl_->imagePlanesVisible3d(); }

void XQRenderScene::setImageVisible(bool on) { impl_->setImageVisible(on); }
bool XQRenderScene::imageVisible() const { return impl_->imageVisible(); }

double XQRenderScene::sliceWorldCoord(int axis) const
{
    return impl_->sliceWorldCoord(axis);
}
int XQRenderScene::worldToSliceIndex(int axis, double world) const
{
    return impl_->worldToSliceIndex(axis, world);
}
bool XQRenderScene::worldToVoxelIndex(const double world[3], int* i, int* j,
                                      int* k) const
{
    return impl_->worldToVoxelIndex(world, i, j, k);
}
void XQRenderScene::setSeedMarker(int i, int j, int k)
{
    impl_->setSeedMarker(i, j, k);
}
void XQRenderScene::clearSeedMarker() { impl_->clearSeedMarker(); }
bool XQRenderScene::seedMarkerVisible() const { return impl_->seedMarkerVisible(); }
void XQRenderScene::setPathControlPoints(
    const std::vector<std::array<double, 3>>& worldPoints)
{
    impl_->setPathControlPoints(worldPoints);
}
void XQRenderScene::clearPathControlPoints() { impl_->clearPathControlPoints(); }
std::size_t XQRenderScene::pathControlPointCount() const
{
    return impl_->pathControlPointCount();
}
int XQRenderScene::pathControlVisibleCount(int axis) const
{
    return impl_->pathControlVisibleCount(axis);
}
int XQRenderScene::crosshairLineAxis(int viewAxis, int line)
{
    return Impl::crosshairLineAxis(viewAxis, line);
}
bool XQRenderScene::crosshairLineColor(int viewAxis, int line, double rgb[3]) const
{
    return impl_->crosshairLineColor(viewAxis, line, rgb);
}

RenderStats XQRenderScene::upsertNode(const NodeId& id, const XQDataNode& node)
{
    return impl_->upsertNode(id, node);
}

RenderStats XQRenderScene::upsertNodeProgressive(const NodeId& id,
                                                 const IGeometrySource& src,
                                                 GeoKind kind,
                                                 const ChunkUploadSpec& spec,
                                                 const ChunkProgressFn& onChunk)
{
    return impl_->upsertNodeProgressive(id, src, kind, spec, onChunk);
}

void XQRenderScene::removeNode(const NodeId& id) { impl_->removeNode(id); }
void XQRenderScene::clearNodes() { impl_->clearNodes(); }
void XQRenderScene::setNodeVisible(const NodeId& id, bool on) { impl_->setNodeVisible(id, on); }
void XQRenderScene::setNodeOpacity(const NodeId& id, double a) { impl_->setNodeOpacity(id, a); }
void XQRenderScene::setNodeColor(const NodeId& id, double r, double g, double b)
{
    impl_->setNodeColor(id, r, g, b);
}

int XQRenderScene::nodeActorCount(ViewId v) const { return impl_->nodeActorCount(v); }
bool XQRenderScene::hasNode(const NodeId& id) const { return impl_->hasNode(id); }
bool XQRenderScene::nodeVisible(const NodeId& id) const { return impl_->nodeVisible(id); }
long long XQRenderScene::uploadedPointCount(const NodeId& id) const
{
    return impl_->uploadedPointCount(id);
}

int XQRenderScene::nodeSliceCutCount(const NodeId& id, ViewId v) const
{
    return impl_->nodeSliceCutCount(id, v);
}
int XQRenderScene::nodeContourOverlayCount(const NodeId& id, ViewId v) const
{
    return impl_->nodeContourOverlayCount(id, v);
}
bool XQRenderScene::nodeColor(const NodeId& id, double rgb[3]) const
{
    return impl_->nodeColor(id, rgb);
}

void* XQRenderScene::vtkRendererHandle(ViewId v) const { return impl_->renderer(v); }

void* XQRenderScene::vtkImageDataHandle() const { return impl_->imageDataHandle(); }

} // namespace xq
