#include "visualization/XQSceneRenderer.h"

#include "core/GeometryTypes.h"
#include "core/XQFlowResult.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQPath.h"
#include "core/XQPathPayload.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/IGeometrySource.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/source/ResidentTetSource.h"
#include "visualization/ChunkPlan.h"
#include "visualization/SurfaceLodBuilder.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkExtractEdges.h>
#include <vtkFloatArray.h>
#include <vtkGeometryFilter.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkLODActor.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTubeFilter.h>
#include <vtkType.h>
#include <vtkTypeInt32Array.h>
#include <vtkUnstructuredGrid.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkWindowToImageFilter.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

namespace xq {
namespace {

// --- internal converters: XQ geometry -> VTK data objects ------------------
// All conversion lives here (anonymous namespace, .cpp only) so the renderer
// just calls a converter and never scatters conversion logic. Each converter
// preserves XQ point counts so the RenderStats the caller asserts on stay
// truthful.

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
// real buffer is supplied its scalars are copied voxel-for-voxel (component 0);
// otherwise every voxel takes the midpoint of the intensity range -- a neutral
// fill, deliberately not the synthetic gradient the early XQImageViewer used.
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

    // VTK image scalars are contiguous row-major (x fastest), single-component
    // float, extent 0-based (SetDimensions from 0). Fetch the base once and
    // advance linearly instead of recomputing the offset per voxel.
    float* base = static_cast<float*>(image->GetScalarPointer(0, 0, 0));
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
// Point3 is a packed double[3]; the renderer copies the contiguous Source span
// straight into VTK-owned arrays (copy-on-upload), so no per-element SetPoint /
// InsertNextCell / per-cell vtkNew remains, and no Source lease has to outlive
// the add* call (no dangling borrow into VTK).
static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3] for bulk upload");

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
// `verticesPerCell` from a flat int32 connectivity span. Both arrays are
// VTK-owned int32: connectivity is bulk-copied, offsets are generated 0,k,2k,...
// `flatConnectivity` must point at cellCount*verticesPerCell ints (e.g. the
// reinterpreted SourceTriangle/SourceTet span).
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

    // SourceTriangle is std::array<int,3> => contiguous int[3n]; reinterpret the
    // span as flat int32 connectivity.
    const int* flatConn = reinterpret_cast<const int*>(tris.data());
    if (!connectivity_in_bounds(flatConn, tris.size(), 3, pts.size())) {
        return nullptr;
    }
    vtkSmartPointer<vtkCellArray> triangles =
        build_cells(flatConn, tris.size(), 3);

    // faceId cell scalars: bulk-fill a VTK float array from the parallel span.
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
// A chunk is a contiguous cell range [begin, end) over the source's full cell
// span. Its cells reference global point indices; we gather only the points the
// chunk touches and remap them to local indices so each chunk's vtkPolyData is
// compact and self-contained (copy-on-upload, no borrow into VTK).

// ChunkRange + plan_chunks live in ChunkPlan.h (VTK-free) so the partition
// invariant can be unit tested directly; see test_chunk_plan.

// Gathers the points referenced by cells [range) (a flat global-index span of
// vertsPerCell ints per cell), remaps them to local indices, and copies both the
// local points and the locally-indexed connectivity into VTK-owned arrays.
// `remap` is a reusable scratch buffer sized to the source point count (values
// -1 == unassigned); only the entries this chunk touches are reset afterwards so
// it can be reused across chunks without an O(pointCount) clear each time.
// Returns the local points + cells; the caller wraps them into a vtkPolyData
// (surface) or vtkUnstructuredGrid (tet) and attaches any cell data.
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
    localPoints.reserve(connCount); // upper bound; shrinks via dedup
    std::vector<int> touched;       // global indices assigned this chunk, to reset remap
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

    // Reset only the entries this chunk touched, so remap is clean for the next.
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

    // SourceTet is std::array<int,4> => contiguous int[4n]; one uniform-type
    // SetCells, no per-tet vtkTetra allocation.
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

} // namespace

class XQSceneRenderer::Impl {
public:
    Impl()
        : renderer_(vtkSmartPointer<vtkRenderer>::New())
    {
        renderer_->SetBackground(0.05, 0.05, 0.08);
    }

    void clear()
    {
        renderer_->RemoveAllViewProps();
    }

    int actorCount() const
    {
        return static_cast<int>(renderer_->GetViewProps()->GetNumberOfItems());
    }

    RenderStats addImageSlice(const XQImageVolume& volume,
                              const XQMemoryImageBufferHandle* buffer,
                              int axis,
                              int slice)
    {
        vtkSmartPointer<vtkImageData> image = build_image(volume, buffer);
        if (image == nullptr) {
            return {false, actorCount(), 0};
        }

        int extent[6] = {0, 0, 0, 0, 0, 0};
        image->GetExtent(extent);
        const int clampedAxis = std::min(std::max(axis, 0), 2);
        const int low = extent[clampedAxis * 2];
        const int high = extent[clampedAxis * 2 + 1];
        const int clampedSlice = std::min(std::max(slice, low), high);

        vtkNew<vtkImageSliceMapper> mapper;
        mapper->SetInputData(image);
        mapper->SetOrientation(clampedAxis);
        mapper->SetSliceNumber(clampedSlice);

        vtkNew<vtkImageSlice> sliceProp;
        sliceProp->SetMapper(mapper);

        const IntensityRange& range = volume.intensityRange();
        const double window =
            volume.windowWidth() > 0.0
                ? volume.windowWidth()
                : (range.maximum > range.minimum ? range.maximum - range.minimum : 255.0);
        const double level =
            volume.windowWidth() > 0.0
                ? volume.windowCenter()
                : (range.maximum > range.minimum ? (range.minimum + range.maximum) * 0.5 : 127.5);
        sliceProp->GetProperty()->SetColorWindow(window);
        sliceProp->GetProperty()->SetColorLevel(level);

        renderer_->AddViewProp(sliceProp);
        renderer_->ResetCamera();

        return {true, actorCount(), static_cast<long long>(image->GetNumberOfPoints())};
    }

    RenderStats addPath(const XQPathPayload& payload)
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
            return {false, actorCount(), 0};
        }

        vtkSmartPointer<vtkPolyData> polyline = build_polyline(worldPoints);

        vtkNew<vtkTubeFilter> tube;
        tube->SetInputData(polyline);
        tube->SetRadius(estimate_tube_radius(worldPoints));
        tube->SetNumberOfSides(8);
        tube->CappingOn();

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(tube->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.95, 0.75, 0.2);

        renderer_->AddActor(actor);
        renderer_->ResetCamera();

        return {true, actorCount(), static_cast<long long>(worldPoints.size())};
    }

    RenderStats addSurface(const XQTriangleSurfaceGeometryHandle& surface,
                           const LodOptions& lod = {})
    {
        // Consume through the Source abstraction (M9a). The handle is borrowed
        // via an aliasing shared_ptr with a no-op deleter: the renderer does not
        // own the handle, and build_surface copies the data out (copy-on-upload),
        // so the source/lease need not outlive this call.
        ResidentSurfaceSource source(
            std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
                std::shared_ptr<const void>(), &surface));
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.triangleCount == 0) {
            return {false, actorCount(), 0, 0};
        }

        vtkSmartPointer<vtkPolyData> full = build_surface(source);
        if (full == nullptr) {
            return {false, actorCount(), 0, 0};
        }

        // Global faceId range from the full surface: shared by every LOD level so
        // colours stay consistent across levels (the LUT range never shifts).
        double faceRange[2] = {0.0, 0.0};
        full->GetCellData()->GetScalars()->GetRange(faceRange);

        const long long srcPts = static_cast<long long>(surface.pointCount());

        if (!lod.enabled) {
            // M9a path: render the full surface, uploaded == source.
            mountSurfaceActor(full, /*faceIdValid=*/true, faceRange);
            return {true, actorCount(), srcPts, srcPts};
        }

        // LOD path: build levels, pick one deterministically (CPU-side), and
        // upload only that level. uploadedPointCount reports the chosen level's
        // points; pointCount stays the source count (headless-stable contract).
        TriangleLease triLease = source.acquire_triangles();
        const LodLevels levels = SurfaceLodBuilder::buildSync(full, triLease.view().faceIds);
        if (!levels.ok) {
            mountSurfaceActor(full, true, faceRange);
            return {true, actorCount(), srcPts, srcPts};
        }
        if (lod.interactive) {
            // Windowed path: all levels on one vtkLODActor, interactor-driven
            // still/motion switching. uploadedPointCount reports the still
            // (full) level -- the headless-measurable level is the fixed path.
            const long long stillPts = mountLodActor(levels, faceRange);
            return {true, actorCount(), srcPts, stillPts};
        }
        const int k = selectLodLevel(lod, levels);
        const LodLevel& lvl = levels.level[k];
        mountSurfaceActor(lvl.poly, lvl.faceMap.valid, faceRange);
        return {true, actorCount(), srcPts, lvl.pointCount};
    }

    // Builds the normals + LUT-or-flat mapper for one surface polyData.
    // faceIdValid==false -> flat shaded (far LOD level). faceRange is the global
    // faceId range so colours match across LOD levels.
    static vtkSmartPointer<vtkPolyDataMapper> makeSurfaceMapper(vtkPolyData* polyData,
                                                               bool faceIdValid,
                                                               const double faceRange[2])
    {
        // M3 surface winding is NOT globally consistent (closed != consistent
        // normals; see memory xq-surface-winding-not-consistent / acceptance
        // ironrule 3). vtkPolyDataNormals with AutoOrientNormals + Consistency
        // re-derives a coherent orientation instead of trusting input winding.
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
            mapper->ScalarVisibilityOff(); // far level: flat shaded
        }
        return mapper;
    }

    // Builds the mapper + actor for one surface polyData and adds it to the
    // renderer. faceIdValid==false -> flat shaded (far LOD level).
    void mountSurfaceActor(vtkPolyData* polyData, bool faceIdValid,
                           const double faceRange[2])
    {
        vtkSmartPointer<vtkPolyDataMapper> mapper =
            makeSurfaceMapper(polyData, faceIdValid, faceRange);
        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        renderer_->AddActor(actor);
        renderer_->ResetCamera();
    }

    // Interactive (windowed) LOD assembly: mount every level on a vtkLODActor.
    // The full level is the actor's primary mapper (rendered when still); the
    // coarser levels are added via AddLODMapper and the QVTK interactor's
    // DesiredUpdateRate switches to them while moving. Not used headless --
    // there is no interactor offscreen to drive level switching, so this path
    // is only assembled for the interactive widget. Returns the still-level
    // (full) point count for uploadedPointCount.
    long long mountLodActor(const LodLevels& levels, const double faceRange[2])
    {
        vtkNew<vtkLODActor> actor;
        // Primary (still) mapper = full level.
        actor->SetMapper(makeSurfaceMapper(levels.level[0].poly, true, faceRange));
        // Coarser levels for motion; order is not significant to vtkLODActor.
        for (int k = 1; k < 3; ++k) {
            if (levels.level[k].poly != nullptr
                && levels.level[k].triangleCount > 0) {
                actor->AddLODMapper(
                    makeSurfaceMapper(levels.level[k].poly,
                                      levels.level[k].faceMap.valid, faceRange));
            }
        }
        renderer_->AddActor(actor);
        renderer_->ResetCamera();
        return levels.level[0].pointCount;
    }

    // CPU-side LOD level selection (the single source of truth; vtkLODActor's
    // auto-switching is not usable headless). fixedLevel >= 0 forces a level;
    // otherwise pick the highest-fidelity level whose triangle count is within
    // budgetTriangles (falls back to far when none fit / no budget given).
    static int selectLodLevel(const LodOptions& lod, const LodLevels& levels)
    {
        if (lod.fixedLevel >= 0) {
            return lod.fixedLevel <= 2 ? lod.fixedLevel : 2;
        }
        if (lod.budgetTriangles <= 0) {
            return 0; // no budget -> full
        }
        for (int k = 0; k < 3; ++k) {
            if (levels.level[k].triangleCount <= lod.budgetTriangles) {
                return k;
            }
        }
        return 2; // nothing fits -> coarsest
    }

    // M9b-C: progressive surface upload. Splits the source triangles into
    // contiguous chunks and builds one actor per chunk so the first chunk is
    // visible before the whole upload completes. Each chunk is compacted +
    // copy-on-upload (no borrow into VTK); the global faceId range is computed
    // once up front so colours stay consistent across chunks. onChunk (optional)
    // fires after each chunk's actor is added, with cumulative stats.
    RenderStats addSurfaceProgressive(const IGeometrySource& source,
                                      const ChunkUploadSpec& spec,
                                      const ChunkProgressFn& onChunk)
    {
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.triangleCount == 0) {
            return {false, actorCount(), 0, 0, 0, 0};
        }

        // One acquire each, held for the whole progressive build (RAII in this
        // call); each chunk copies its subset out (copy-on-upload), so neither
        // lease has to outlive this method.
        GeometryLease<Point3> pointLease = source.acquire_points();
        TriangleLease triLease = source.acquire_triangles();
        const ReadSpan<Point3>& pts = pointLease.span();
        const TriangleView& triView = triLease.view();
        const ReadSpan<SourceTriangle>& tris = triView.triangles;
        const ReadSpan<int>& faceIds = triView.faceIds;

        // Global faceId range (shared LUT range -> no cross-chunk colour drift).
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
            return {false, actorCount(), 0, 0, 0, 0};
        }

        const std::vector<ChunkRange> chunks =
            plan_chunks(tris.size(), spec.maxCellsPerChunk);
        const int chunkCount = static_cast<int>(chunks.size());

        std::vector<int> remap(meta.pointCount, -1);

        int completed = 0;
        for (const ChunkRange& range : chunks) {
            ChunkMesh mesh = build_cell_chunk(pts, flatConn, range, 3, remap);

            // Per-chunk faceId cell scalars (segment [begin, end)); the chunk's
            // local cell j maps to global faceId faceIds[range.begin + j].
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
                    mountLodActor(levels, faceRange);
                } else {
                    mountSurfaceActor(poly, /*faceIdValid=*/true, faceRange);
                }
            } else {
                mountSurfaceActor(poly, /*faceIdValid=*/true, faceRange);
            }

            ++completed;
            if (onChunk) {
                onChunk({true, actorCount(), srcPts, srcPts, chunkCount, completed});
            }
        }

        return {true, actorCount(), srcPts, srcPts, chunkCount, completed};
    }

    RenderStats addVolumeMesh(const XQTetVolumeMeshHandle& mesh)
    {
        // Consume through the Source abstraction (M9a); see addSurface for the
        // aliasing-shared_ptr / copy-on-upload rationale.
        ResidentTetSource source(
            std::shared_ptr<const XQTetVolumeMeshHandle>(
                std::shared_ptr<const void>(), &mesh));
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.tetCount == 0) {
            return {false, actorCount(), 0};
        }

        vtkSmartPointer<vtkUnstructuredGrid> grid = build_volume(source);
        if (grid == nullptr) {
            return {false, actorCount(), 0};
        }
        mountVolumeWireframeActor(grid);
        return {true, actorCount(), static_cast<long long>(mesh.pointCount())};
    }

    // Builds the boundary-edges wireframe actor for a tet grid and adds it to the
    // renderer. Shared by addVolumeMesh and addVolumeMeshProgressive.
    void mountVolumeWireframeActor(vtkUnstructuredGrid* grid)
    {
        // Extract the boundary surface, then its edges, for a wireframe-style
        // view of the volume mesh.
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

        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.4, 0.85, 0.95);
        actor->GetProperty()->SetRepresentationToWireframe();

        renderer_->AddActor(actor);
        renderer_->ResetCamera();
    }

    // M9b-C: progressive tet volume-mesh upload. Same chunk skeleton as
    // addSurfaceProgressive but over tetrahedra (4 verts/cell, no faceId); each
    // chunk becomes its own boundary-edges wireframe actor.
    RenderStats addVolumeMeshProgressive(const IGeometrySource& source,
                                         const ChunkUploadSpec& spec,
                                         const ChunkProgressFn& onChunk)
    {
        const GeometryMeta meta = source.meta();
        if (meta.pointCount == 0 || meta.tetCount == 0) {
            return {false, actorCount(), 0, 0, 0, 0};
        }

        GeometryLease<Point3> pointLease = source.acquire_points();
        GeometryLease<SourceTet> tetLease = source.acquire_tetrahedra();
        const ReadSpan<Point3>& pts = pointLease.span();
        const ReadSpan<SourceTet>& tets = tetLease.span();

        const long long srcPts = static_cast<long long>(meta.pointCount);
        const int* flatConn = reinterpret_cast<const int*>(tets.data());
        if (!connectivity_in_bounds(flatConn, tets.size(), 4, pts.size())) {
            return {false, actorCount(), 0, 0, 0, 0};
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
            mountVolumeWireframeActor(grid);

            ++completed;
            if (onChunk) {
                onChunk({true, actorCount(), srcPts, srcPts, chunkCount, completed});
            }
        }

        return {true, actorCount(), srcPts, srcPts, chunkCount, completed};
    }

    RenderStats addFlowResult(const XQFlowResultPayload& payload)
    {
        const XQFlowResult& result = payload.result();
        const std::vector<FlowSegment>& segments = result.segments();
        if (segments.empty()) {
            return {false, actorCount(), 0};
        }

        // The flow result carries no geometry, so render a schematic: one
        // polyline with a point per segment laid out along segmentId (x axis),
        // coloured by that segment's last-time pressure. Deliberately minimal --
        // re-associating the path geometry is out of scope.
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
        lut->SetHueRange(0.667, 0.0); // blue (low) -> red (high)
        lut->SetTableRange(pressureRange[0], upper);
        lut->Build();

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(polyline);
        mapper->SetScalarModeToUsePointData();
        mapper->SetColorModeToMapScalars();
        mapper->SetLookupTable(lut);
        mapper->SetScalarRange(pressureRange[0], upper);

        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetLineWidth(3.0);

        renderer_->AddActor(actor);
        renderer_->ResetCamera();

        return {true, actorCount(), static_cast<long long>(segments.size())};
    }

    RenderStats addSegmentationMask(const XQSegmentationMaskPayload& payload)
    {
        const XQSegmentationMask& mask = payload.mask();
        if (!mask.is_valid()) {
            return {false, actorCount(), 0};
        }

        const int dimX = mask.dimensionX();
        const int dimY = mask.dimensionY();
        const int dimZ = mask.dimensionZ();

        // Geometry maps voxel index -> world position; without it fall back to
        // raw voxel coordinates so the mask is still visible.
        const bool hasGeometry = mask.hasGeometry();
        ImageGeometry geometry = {};
        if (hasGeometry) {
            geometry = mask.geometry();
        }

        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        // Pre-count non-zero voxels, allocate once, then fill by SetPoint --
        // avoids the repeated realloc that per-point InsertNextPoint incurs.
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
            return {false, actorCount(), 0};
        }

        vtkSmartPointer<vtkPolyData> cloud = vtkSmartPointer<vtkPolyData>::New();
        cloud->SetPoints(points);

        vtkNew<vtkVertexGlyphFilter> glyph;
        glyph->SetInputData(cloud);

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(glyph->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.9, 0.3, 0.3);
        actor->GetProperty()->SetPointSize(3.0);

        renderer_->AddActor(actor);
        renderer_->ResetCamera();

        return {true, actorCount(), static_cast<long long>(foreground)};
    }

    RenderStats renderOffscreenToRgba(int width,
                                      int height,
                                      std::vector<unsigned char>* outRgba)
    {
        if (width <= 0 || height <= 0) {
            return {false, actorCount(), 0};
        }
        if (outRgba != nullptr) {
            outRgba->clear();
        }

        renderer_->ResetCamera();

        vtkNew<vtkRenderWindow> renderWindow;
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(width, height);
        renderWindow->AddRenderer(renderer_);

        vtkNew<vtkWindowToImageFilter> capture;
        capture->SetInput(renderWindow);
        capture->SetInputBufferTypeToRGB();
        capture->ReadFrontBufferOff();

        try {
            renderWindow->Render();
            capture->Modified();
            capture->Update();
        } catch (...) {
            // Detach the renderer before the window dies so it can be reused.
            renderWindow->RemoveRenderer(renderer_);
            return {false, actorCount(), 0};
        }

        vtkImageData* output = capture->GetOutput();
        int actualDimensions[3] = {0, 0, 0};
        if (output != nullptr) {
            output->GetDimensions(actualDimensions);
        }

        const bool ok =
            output != nullptr && output->GetScalarPointer() != nullptr
            && actualDimensions[0] == width && actualDimensions[1] == height;

        // Detach so the renderer (owned by Impl, reused across renders) outlives
        // this throwaway window.
        renderWindow->RemoveRenderer(renderer_);

        RenderStats stats = {ok, actorCount(), 0};
        if (!ok || outRgba == nullptr) {
            return stats;
        }

        const int componentCount = output->GetNumberOfScalarComponents();
        const unsigned char* rgb =
            static_cast<const unsigned char*>(output->GetScalarPointer());
        if (componentCount < 3 || rgb == nullptr) {
            return {false, actorCount(), 0};
        }

        const std::size_t pixelCount =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        outRgba->resize(pixelCount * 4U);
        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const std::size_t source = pixel * static_cast<std::size_t>(componentCount);
            const std::size_t target = pixel * 4U;
            (*outRgba)[target] = rgb[source];
            (*outRgba)[target + 1U] = rgb[source + 1U];
            (*outRgba)[target + 2U] = rgb[source + 2U];
            (*outRgba)[target + 3U] = 255U;
        }

        return stats;
    }

    void* vtkRendererHandle() const
    {
        return renderer_.Get();
    }

private:
    static double estimate_tube_radius(const std::vector<Point3>& points)
    {
        // A small fraction of the polyline's bounding-box diagonal, so the tube
        // is visible at any scale without a hard-coded world size.
        double lo[3] = {points[0].x, points[0].y, points[0].z};
        double hi[3] = {points[0].x, points[0].y, points[0].z};
        for (const Point3& p : points) {
            lo[0] = std::min(lo[0], p.x);
            lo[1] = std::min(lo[1], p.y);
            lo[2] = std::min(lo[2], p.z);
            hi[0] = std::max(hi[0], p.x);
            hi[1] = std::max(hi[1], p.y);
            hi[2] = std::max(hi[2], p.z);
        }
        const double dx = hi[0] - lo[0];
        const double dy = hi[1] - lo[1];
        const double dz = hi[2] - lo[2];
        const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double radius = diagonal * 0.02;
        return radius > 0.0 ? radius : 0.5;
    }

    static void voxel_to_world(const ImageGeometry& g, int x, int y, int z, double world[3])
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

    vtkSmartPointer<vtkRenderer> renderer_;
};

XQSceneRenderer::XQSceneRenderer()
    : impl_(new Impl())
{
}

XQSceneRenderer::~XQSceneRenderer() = default;

void XQSceneRenderer::clear()
{
    impl_->clear();
}

RenderStats XQSceneRenderer::addImageSlice(const XQImageVolume& img,
                                           const XQMemoryImageBufferHandle* buffer,
                                           int axis,
                                           int slice)
{
    return impl_->addImageSlice(img, buffer, axis, slice);
}

RenderStats XQSceneRenderer::addPath(const XQPathPayload& path)
{
    return impl_->addPath(path);
}

RenderStats XQSceneRenderer::addSurface(const XQTriangleSurfaceGeometryHandle& surf,
                                        const LodOptions& lod)
{
    return impl_->addSurface(surf, lod);
}

RenderStats XQSceneRenderer::addSurfaceProgressive(const IGeometrySource& source,
                                                   const ChunkUploadSpec& spec,
                                                   const ChunkProgressFn& onChunk)
{
    return impl_->addSurfaceProgressive(source, spec, onChunk);
}

RenderStats XQSceneRenderer::addVolumeMesh(const XQTetVolumeMeshHandle& mesh)
{
    return impl_->addVolumeMesh(mesh);
}

RenderStats XQSceneRenderer::addVolumeMeshProgressive(const IGeometrySource& source,
                                                      const ChunkUploadSpec& spec,
                                                      const ChunkProgressFn& onChunk)
{
    return impl_->addVolumeMeshProgressive(source, spec, onChunk);
}

RenderStats XQSceneRenderer::addFlowResult(const XQFlowResultPayload& flow)
{
    return impl_->addFlowResult(flow);
}

RenderStats XQSceneRenderer::addSegmentationMask(const XQSegmentationMaskPayload& mask)
{
    return impl_->addSegmentationMask(mask);
}

RenderStats XQSceneRenderer::renderOffscreenToRgba(int width,
                                                   int height,
                                                   std::vector<unsigned char>* outRgba)
{
    return impl_->renderOffscreenToRgba(width, height, outRgba);
}

void* XQSceneRenderer::vtkRendererHandle() const
{
    return impl_->vtkRendererHandle();
}

} // namespace xq
