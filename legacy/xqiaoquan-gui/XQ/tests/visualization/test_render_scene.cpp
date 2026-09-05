// B1a XQRenderScene discrete-invariant tests. Pure VTK, no GL / offscreen
// context needed (the scene only assembles renderers + actors; no render is
// driven here). Bare main + fail() style, mirroring test_scene_renderer.
//
// IRON RULE (memory: no-sideeffect-in-assert): side-effecting calls
// (setVolume / upsertNode / setSliceIndex / ...) are made first and their
// results stored, THEN asserted -- never inside an assert/condition expression,
// so a /DNDEBUG build cannot delete them into a false green.

#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQContourGroupPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQSegmentationMask.h>
#include <core/XQSegmentationMaskPayload.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <core/source/IGeometrySource.h>
#include <core/source/ResidentSurfaceSource.h>
#include <visualization/XQRenderScene.h>

#include <vtkCamera.h>
#include <vtkRenderer.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

xq::ImageGeometry make_geometry(int nx, int ny, int nz)
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = nx;
    geometry.dimensions[1] = ny;
    geometry.dimensions[2] = nz;
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 1.0;
    geometry.origin[0] = 0.0;
    geometry.origin[1] = 0.0;
    geometry.origin[2] = 0.0;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            geometry.direction[row][column] = row == column ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

// Unit tetrahedron with its 4 triangular faces (consistent winding).
xq::XQTriangleSurfaceGeometryHandle make_tet_surface()
{
    xq::XQTriangleSurfaceGeometryHandle surface;
    surface.addPoint({0.0, 0.0, 0.0});
    surface.addPoint({1.0, 0.0, 0.0});
    surface.addPoint({0.0, 1.0, 0.0});
    surface.addPoint({0.0, 0.0, 1.0});
    surface.addTriangle(0, 2, 1, 1);
    surface.addTriangle(0, 1, 3, 1);
    surface.addTriangle(0, 3, 2, 1);
    surface.addTriangle(1, 2, 3, 2);
    return surface;
}

// A Float32, single-component image volume of the given dimensions, plus a
// matching contiguous Float32 buffer (exercises the memcpy fast path).
xq::XQImageVolume make_float_volume(int nx, int ny, int nz)
{
    xq::XQImageVolume volume;
    volume.setGeometry(make_geometry(nx, ny, nz));
    volume.setScalarType(xq::ScalarType::Float32);
    volume.setComponentCount(1);
    volume.setIntensityRange({0.0, 255.0});
    volume.setWindowCenter(0.0);
    volume.setWindowWidth(0.0); // force full-window init from intensity range
    return volume;
}

// A control-point path node.
xq::XQDataNode make_path_node(xq::NodeId id)
{
    xq::XQPath path;
    std::vector<xq::PathControlPoint> controls = {
        {{0.0, 0.0, 0.0}}, {{2.0, 0.0, 0.0}}, {{2.0, 2.0, 0.0}}, {{2.0, 2.0, 2.0}}};
    path.setControlPoints(controls);
    auto payload = std::make_shared<xq::XQPathPayload>(path);
    return xq::XQDataNode(id, xq::XQDomainType::Path, "path", payload);
}

// A surface-model node carrying a tetrahedron triangle geometry.
xq::XQDataNode make_surface_node(xq::NodeId id)
{
    auto geometry = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>(make_tet_surface());
    xq::XQSurfaceModel model;
    model.setTriangleGeometry(geometry);
    auto payload = std::make_shared<xq::XQSurfaceModelPayload>(std::move(model));
    return xq::XQDataNode(id, xq::XQDomainType::SurfaceModel, "surface", payload);
}

// A mesh node carrying a single-tet volume mesh (exercises the mesh
// default-hidden path).
xq::XQDataNode make_mesh_node(xq::NodeId id)
{
    auto tets = std::make_shared<xq::XQTetVolumeMeshHandle>();
    tets->addPoint({0.0, 0.0, 0.0});
    tets->addPoint({1.0, 0.0, 0.0});
    tets->addPoint({0.0, 1.0, 0.0});
    tets->addPoint({0.0, 0.0, 1.0});
    tets->addTet(0, 1, 2, 3);
    xq::XQMesh mesh;
    mesh.setVolumeTets(tets);
    auto payload = std::make_shared<xq::XQMeshPayload>(std::move(mesh));
    return xq::XQDataNode(id, xq::XQDomainType::Mesh, "mesh", payload);
}

// A segmentation-mask node with a small foreground block.
xq::XQDataNode make_mask_node(xq::NodeId id)
{
    const int dims[3] = {6, 6, 4};
    xq::XQSegmentationMask mask(dims);
    mask.setGeometry(make_geometry(6, 6, 4));
    for (int z = 1; z <= 2; ++z) {
        for (int y = 1; y <= 2; ++y) {
            for (int x = 1; x <= 2; ++x) {
                mask.setLabelAt(mask.voxelIndex(x, y, z), 1);
            }
        }
    }
    auto payload = std::make_shared<xq::XQSegmentationMaskPayload>(mask);
    return xq::XQDataNode(id, xq::XQDomainType::SegmentationMask, "mask", payload);
}

// A 3-segment flow-result node.
xq::XQDataNode make_flow_node(xq::NodeId id)
{
    xq::XQFlowResult result;
    std::vector<double> times = {0.0, 0.5, 1.0};
    result.setTimes(times);
    for (int s = 0; s < 3; ++s) {
        xq::FlowSegment segment;
        segment.segmentId = s;
        segment.arcLengthStart = static_cast<double>(s);
        segment.arcLengthEnd = static_cast<double>(s + 1);
        result.addSegment(segment);
    }
    std::vector<std::vector<double>> q = {{1.0, 1.0, 1.0}, {2.0, 2.0, 2.0}, {3.0, 3.0, 3.0}};
    std::vector<std::vector<double>> p = {
        {10.0, 11.0, 12.0}, {8.0, 9.0, 10.0}, {6.0, 7.0, 8.0}};
    std::vector<std::vector<double>> a = {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}};
    result.setSeries(q, p, a);
    auto payload = std::make_shared<xq::XQFlowResultPayload>(result);
    return xq::XQDataNode(id, xq::XQDomainType::FlowResult, "flow", payload);
}

// An empty-payload node (Unknown domain, null payload): not renderable.
xq::XQDataNode make_empty_node(xq::NodeId id)
{
    return xq::XQDataNode(id, xq::XQDomainType::SurfaceModel, "empty", nullptr);
}

// A closed square contour centred at (2,2,z) in the z-plane, so its centroid sits
// exactly on world z (with identity direction / unit spacing / zero origin the
// slice at index k has world z==k, so this contour is "hit" by the Axial slice at
// k==z).
xq::XQContour make_square_contour(int contourId, double z)
{
    xq::XQContour c;
    c.contourId = xq::NodeId(static_cast<xq::NodeId::ValueType>(contourId));
    c.pathArcLength = z;
    c.frame = {};
    c.type = xq::ContourType::Manual;
    c.closed = true;
    c.points = {{1.0, 1.0, z}, {3.0, 1.0, z}, {3.0, 3.0, z}, {1.0, 3.0, z}};
    return c;
}

// A contour-group node with two closed contours, one on z=1 and one on z=2.
xq::XQDataNode make_contour_node(xq::NodeId id)
{
    xq::XQContourGroup group;
    group.setId(id);
    group.addContour(make_square_contour(1, 1.0));
    group.addContour(make_square_contour(2, 2.0));
    auto payload = std::make_shared<xq::XQContourGroupPayload>(std::move(group));
    return xq::XQDataNode(id, xq::XQDomainType::ContourGroup, "contours", payload);
}

// A multi-region triangle grid for progressive tests (faceId banded along x).
std::shared_ptr<xq::XQTriangleSurfaceGeometryHandle> make_grid_surface(int n, int regions)
{
    auto h = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            h->addPoint({static_cast<double>(x), static_cast<double>(y), 0.0});
        }
    }
    auto idx = [n](int x, int y) { return y * n + x; };
    for (int y = 0; y < n - 1; ++y) {
        for (int x = 0; x < n - 1; ++x) {
            const int band = (x * regions) / (n - 1);
            const int faceId = band + 1;
            h->addTriangle(idx(x, y), idx(x + 1, y), idx(x + 1, y + 1), faceId);
            h->addTriangle(idx(x, y), idx(x + 1, y + 1), idx(x, y + 1), faceId);
        }
    }
    return h;
}

// Source whose triangle connectivity references an out-of-range point index.
class BadConnectivitySource final : public xq::IGeometrySource {
public:
    BadConnectivitySource()
        : keepalive_(std::make_shared<int>(0))
    {
        points_.push_back({0.0, 0.0, 0.0});
        points_.push_back({1.0, 0.0, 0.0});
        points_.push_back({0.0, 1.0, 0.0});
        triangles_.push_back(xq::SourceTriangle{0, 1, 3}); // 3 is out of range
        faceIds_.push_back(1);
    }

    xq::GeometryMeta meta() const override
    {
        xq::GeometryMeta meta;
        meta.valid = true;
        meta.pointCount = points_.size();
        meta.triangleCount = triangles_.size();
        meta.tetCount = 0;
        return meta;
    }

    xq::GeometryLease<xq::Point3> acquire_points() const override
    {
        return xq::GeometryLease<xq::Point3>::borrow(
            keepalive_, xq::ReadSpan<xq::Point3>(points_.data(), points_.size()));
    }

    xq::TriangleLease acquire_triangles() const override
    {
        return xq::TriangleLease::borrow_borrowed_faceids(
            keepalive_,
            xq::ReadSpan<xq::SourceTriangle>(triangles_.data(), triangles_.size()),
            keepalive_, xq::ReadSpan<int>(faceIds_.data(), faceIds_.size()));
    }

    xq::GeometryLease<xq::SourceTet> acquire_tetrahedra() const override
    {
        return xq::GeometryLease<xq::SourceTet>::borrow(
            keepalive_, xq::ReadSpan<xq::SourceTet>(tets_.data(), tets_.size()));
    }

private:
    std::shared_ptr<const void> keepalive_;
    std::vector<xq::Point3> points_;
    std::vector<xq::SourceTriangle> triangles_;
    std::vector<int> faceIds_;
    std::vector<xq::SourceTet> tets_;
};

// 1. no-volume initial state
void test_no_volume_initial_state()
{
    xq::XQRenderScene scene;
    check(!scene.hasVolume(), "1: no volume -> hasVolume false");
    for (int axis = 0; axis < 3; ++axis) {
        check(scene.sliceCount(axis) == 0, "1: no volume -> sliceCount 0");
        check(scene.sliceIndex(axis) == -1, "1: no volume -> sliceIndex -1");
    }
    // setSliceIndex on an empty scene is a no-op (must not crash).
    scene.setSliceIndex(2, 3);
    check(scene.sliceIndex(2) == -1, "1: setSliceIndex on empty scene is a no-op");
}

// 2. setVolume of an 8x6x4 float volume
void test_set_volume()
{
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(8) * 6 * 4 * sizeof(float), 0);
    // fill with a deterministic float pattern
    float* f = reinterpret_cast<float*>(bytes.data());
    for (std::size_t i = 0; i < static_cast<std::size_t>(8) * 6 * 4; ++i) {
        f[i] = static_cast<float>(i % 200);
    }
    const int dims[3] = {8, 6, 4};
    xq::XQMemoryImageBufferHandle buffer(xq::ScalarType::Float32, dims, 1, std::move(bytes));
    const bool bufferValid = buffer.is_valid();
    check(bufferValid, "2: float buffer is valid");

    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, &buffer);
    check(ok, "2: setVolume returns true");
    check(scene.hasVolume(), "2: hasVolume true after setVolume");
    check(scene.sliceCount(0) == 8, "2: sliceCount axis0 == 8");
    check(scene.sliceCount(1) == 6, "2: sliceCount axis1 == 6");
    check(scene.sliceCount(2) == 4, "2: sliceCount axis2 == 4");
    check(scene.sliceIndex(0) == 4, "2: initial sliceIndex axis0 == midpoint (4)");
    check(scene.sliceIndex(1) == 3, "2: initial sliceIndex axis1 == midpoint (3)");
    check(scene.sliceIndex(2) == 2, "2: initial sliceIndex axis2 == midpoint (2)");

    double window = 0.0;
    double level = 0.0;
    scene.windowLevel(&window, &level);
    // full window from intensity range [0,255]: window 255, level 127.5
    check(window == 255.0, "2: windowLevel window == full range (255)");
    check(level == 127.5, "2: windowLevel level == range midpoint (127.5)");
}

// 2c. B4c slice-camera fit: after setVolume, each slice view's parallel scale is
// tightened to exactly half the image extent along that view's up axis, so the
// slice fills the window vertically instead of a narrow central strip. The 8x6x4
// unit-spacing / zero-origin volume has world bounds x=[0,7] y=[0,5] z=[0,3], so
// the up-axis extents are: Axial up=y -> 5, Sagittal up=z -> 3, Coronal up=z ->
// 3. Expected parallel scale = 0.5 * extent.
void test_slice_camera_fit()
{
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "2c: setVolume ok");

    auto parallelScale = [&scene](xq::ViewId view) -> double {
        vtkRenderer* r =
            static_cast<vtkRenderer*>(scene.vtkRendererHandle(view));
        if (r == nullptr || r->GetActiveCamera() == nullptr) {
            return -1.0;
        }
        return r->GetActiveCamera()->GetParallelScale();
    };

    const double tol = 1e-6;
    check(std::abs(parallelScale(xq::ViewId::Axial) - 2.5) < tol,
          "2c: Axial parallel scale == 0.5 * y-extent (2.5)");
    check(std::abs(parallelScale(xq::ViewId::Sagittal) - 1.5) < tol,
          "2c: Sagittal parallel scale == 0.5 * z-extent (1.5)");
    check(std::abs(parallelScale(xq::ViewId::Coronal) - 1.5) < tol,
          "2c: Coronal parallel scale == 0.5 * z-extent (1.5)");
}

// 3. setSliceIndex read-back + clamping
void test_slice_index_clamp()
{
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "3: setVolume (null buffer) ok");

    scene.setSliceIndex(2, 1);
    check(scene.sliceIndex(2) == 1, "3: setSliceIndex(2,1) reads back 1");
    scene.setSliceIndex(2, 99);
    check(scene.sliceIndex(2) == 3, "3: setSliceIndex(2,99) clamps to 3");
    scene.setSliceIndex(0, -5);
    check(scene.sliceIndex(0) == 0, "3: setSliceIndex(0,-5) clamps to 0");
}

// 4. memcpy fast-path correctness by type-agnostic contract: same dims/geometry
//    with Float32 vs Int16 buffers behave identically for sliceCount/windowLevel.
void test_type_agnostic_contract()
{
    const int nx = 8;
    const int ny = 6;
    const int nz = 4;
    const int dims[3] = {nx, ny, nz};
    const std::size_t voxels = static_cast<std::size_t>(nx) * ny * nz;

    xq::XQImageVolume volumeF = make_float_volume(nx, ny, nz);
    std::vector<std::uint8_t> bytesF(voxels * sizeof(float), 0);
    xq::XQMemoryImageBufferHandle bufferF(
        xq::ScalarType::Float32, dims, 1, std::move(bytesF));
    xq::XQRenderScene sceneF;
    const bool okF = sceneF.setVolume(volumeF, &bufferF);
    double windowF = 0.0;
    double levelF = 0.0;
    sceneF.windowLevel(&windowF, &levelF);

    xq::XQImageVolume volumeI;
    volumeI.setGeometry(make_geometry(nx, ny, nz));
    volumeI.setScalarType(xq::ScalarType::Int16);
    volumeI.setComponentCount(1);
    volumeI.setIntensityRange({0.0, 255.0});
    volumeI.setWindowCenter(0.0);
    volumeI.setWindowWidth(0.0);
    std::vector<std::uint8_t> bytesI(voxels * sizeof(std::int16_t), 0);
    xq::XQMemoryImageBufferHandle bufferI(
        xq::ScalarType::Int16, dims, 1, std::move(bytesI));
    xq::XQRenderScene sceneI;
    const bool okI = sceneI.setVolume(volumeI, &bufferI);
    double windowI = 0.0;
    double levelI = 0.0;
    sceneI.windowLevel(&windowI, &levelI);

    check(okF && okI, "4: setVolume ok for both Float32 and Int16 buffers");
    check(sceneF.sliceCount(0) == sceneI.sliceCount(0)
              && sceneF.sliceCount(1) == sceneI.sliceCount(1)
              && sceneF.sliceCount(2) == sceneI.sliceCount(2),
          "4: sliceCount type-agnostic (Float32 == Int16)");
    check(windowF == windowI && levelF == levelI,
          "4: windowLevel type-agnostic (Float32 == Int16)");
}

// 5. upsertNode(path) + idempotent re-upsert
void test_upsert_path_idempotent()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(1);
    xq::XQDataNode node = make_path_node(id);
    const xq::RenderStats s1 = scene.upsertNode(id, node);
    check(s1.ok, "5: upsertNode(path) ok");
    check(scene.hasNode(id), "5: hasNode true after upsert");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1, "5: path -> 1 node actor");

    const xq::RenderStats s2 = scene.upsertNode(id, node);
    check(s2.ok, "5: re-upsert same id ok");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1,
          "5: re-upsert same id does not double the actor count");
}

// 6. surface/mask/flow upsert + cumulative actor count
void test_upsert_surface_mask_flow()
{
    xq::XQRenderScene scene;
    const xq::NodeId sid(10);
    const xq::NodeId mid(11);
    const xq::NodeId fid(12);

    const xq::RenderStats sSurf = scene.upsertNode(sid, make_surface_node(sid));
    check(sSurf.ok, "6: upsertNode(surface) ok");
    check(sSurf.pointCount == 4, "6: surface pointCount == 4");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1, "6: after surface -> 1 actor");

    const xq::RenderStats sMask = scene.upsertNode(mid, make_mask_node(mid));
    check(sMask.ok, "6: upsertNode(mask) ok");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 2, "6: after mask -> 2 actors");

    const xq::RenderStats sFlow = scene.upsertNode(fid, make_flow_node(fid));
    check(sFlow.ok, "6: upsertNode(flow) ok");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 3, "6: after flow -> 3 actors");
}

// 7. non-renderable (empty payload) node
void test_non_renderable()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(20);
    const xq::RenderStats s = scene.upsertNode(id, make_empty_node(id));
    check(!s.ok, "7: empty-payload node -> ok false");
    check(!scene.hasNode(id), "7: empty-payload node -> hasNode false");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 0, "7: empty node -> no actor");
}

// 8. setNodeVisible read-back + unknown id
void test_set_visible()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(30);
    const xq::RenderStats s = scene.upsertNode(id, make_surface_node(id));
    check(s.ok, "8: surface added");

    scene.setNodeVisible(id, false);
    check(!scene.nodeVisible(id), "8: setNodeVisible(false) -> nodeVisible false");
    scene.setNodeVisible(id, true);
    check(scene.nodeVisible(id), "8: setNodeVisible(true) -> nodeVisible true");

    const xq::NodeId unknown(999);
    scene.setNodeVisible(unknown, false); // must not crash
    check(!scene.nodeVisible(unknown), "8: unknown id nodeVisible false");
}

// 9. removeNode / clearNodes
void test_remove_and_clear()
{
    xq::XQRenderScene scene;
    const xq::NodeId a(40);
    const xq::NodeId b(41);
    const xq::RenderStats sa = scene.upsertNode(a, make_surface_node(a));
    const xq::RenderStats sb = scene.upsertNode(b, make_path_node(b));
    check(sa.ok && sb.ok, "9: two nodes added");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 2, "9: 2 actors before remove");

    scene.removeNode(a);
    check(!scene.hasNode(a), "9: removeNode -> hasNode false");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1, "9: actor count falls to 1");

    scene.clearNodes();
    check(!scene.hasNode(b), "9: clearNodes -> hasNode false");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 0, "9: clearNodes -> 0 actors");
}

// 10. progressive surface upload with a forced multi-chunk plan
void test_progressive_surface()
{
    auto surf = make_grid_surface(40, 4); // 3042 triangles
    xq::ResidentSurfaceSource src(surf);

    xq::XQRenderScene scene;
    const xq::NodeId id(50);
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 500; // 3042 tris -> 7 chunks

    int callbacks = 0;
    int lastCompleted = 0;
    bool monotonic = true;
    auto onChunk = [&](const xq::RenderStats& cum) {
        ++callbacks;
        if (cum.completedChunkCount != lastCompleted + 1) {
            monotonic = false;
        }
        lastCompleted = cum.completedChunkCount;
    };

    const xq::RenderStats s =
        scene.upsertNodeProgressive(id, src, xq::GeoKind::Surface, spec, onChunk);
    check(s.ok, "10: progressive surface ok");
    check(s.chunkCount > 1, "10: chunkCount > 1 (multi-chunk)");
    check(s.completedChunkCount == s.chunkCount, "10: all chunks completed");
    check(callbacks == s.chunkCount, "10: onChunk fired once per chunk");
    check(monotonic, "10: onChunk completedChunkCount increments monotonically");
    check(scene.hasNode(id), "10: progressive node present");
    check(scene.uploadedPointCount(id) > 0, "10: uploadedPointCount > 0");
}

// 11. invalid-connectivity progressive source
void test_progressive_invalid_connectivity()
{
    xq::XQRenderScene scene;
    // seed the scene with one valid node so we can prove the actor count is
    // unchanged by the rejected upsert.
    const xq::NodeId seed(60);
    const xq::RenderStats sSeed = scene.upsertNode(seed, make_surface_node(seed));
    check(sSeed.ok, "11: seed node added");
    const int before = scene.nodeActorCount(xq::ViewId::Volume3D);

    BadConnectivitySource bad;
    const xq::NodeId id(61);
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 1;
    int callbacks = 0;
    const xq::RenderStats s = scene.upsertNodeProgressive(
        id, bad, xq::GeoKind::Surface, spec, [&](const xq::RenderStats&) { ++callbacks; });

    check(!s.ok, "11: invalid connectivity -> ok false");
    check(s.chunkCount == 0, "11: invalid connectivity -> chunkCount 0");
    check(callbacks == 0, "11: invalid connectivity -> onChunk never called");
    check(!scene.hasNode(id), "11: invalid connectivity -> node not added");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == before,
          "11: invalid connectivity -> actor count unchanged");
}

// 12. setNodeOpacity / setNodeColor unknown-id safety + no disturbance
void test_opacity_color_safety()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(70);
    const xq::RenderStats s = scene.upsertNode(id, make_surface_node(id));
    check(s.ok, "12: surface added");

    const xq::NodeId unknown(998);
    scene.setNodeOpacity(unknown, 0.5); // must not crash
    scene.setNodeColor(unknown, 1.0, 0.0, 0.0); // must not crash

    scene.setNodeOpacity(id, 0.5);
    scene.setNodeColor(id, 0.2, 0.4, 0.6);
    check(scene.nodeVisible(id), "12: known-id opacity/color leaves node visible");
    check(scene.hasNode(id), "12: known-id opacity/color leaves node present");
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1,
          "12: opacity/color does not change actor count");
}

// True when |a-b| is within a tight floating tolerance.
bool approx(double a, double b)
{
    const double d = a - b;
    return (d < 0 ? -d : d) <= 1e-6;
}

// 13. crosshair line->axis mapping, axis colouring, and world<->index inverse
//     (B2a). The mapping table is the single source of truth shared by build /
//     update / the drag probes.
void test_crosshair_axis_color_inverse()
{
    // (a) line->axis mapping is static (no volume needed): full six-tuple table.
    check(xq::XQRenderScene::crosshairLineAxis(2, 0) == 0,
          "13: Axial line0 -> axis 0 (Sagittal)");
    check(xq::XQRenderScene::crosshairLineAxis(2, 1) == 1,
          "13: Axial line1 -> axis 1 (Coronal)");
    check(xq::XQRenderScene::crosshairLineAxis(0, 0) == 1,
          "13: Sagittal line0 -> axis 1 (Coronal)");
    check(xq::XQRenderScene::crosshairLineAxis(0, 1) == 2,
          "13: Sagittal line1 -> axis 2 (Axial)");
    check(xq::XQRenderScene::crosshairLineAxis(1, 0) == 0,
          "13: Coronal line0 -> axis 0 (Sagittal)");
    check(xq::XQRenderScene::crosshairLineAxis(1, 1) == 2,
          "13: Coronal line1 -> axis 2 (Axial)");

    xq::XQRenderScene scene;

    // (b) no volume -> crosshairLineColor false (no actor built yet).
    double rgb[3] = {-1.0, -1.0, -1.0};
    const bool colorNoVol = scene.crosshairLineColor(2, 0, rgb);
    check(!colorNoVol, "13: no volume -> crosshairLineColor false");

    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "13: setVolume ok");

    // (c) axis colours: Axial line0 = Sagittal green, line1 = Coronal blue;
    //     Sagittal line1 = Axial red.
    double green[3] = {0.0, 0.0, 0.0};
    const bool gotGreen = scene.crosshairLineColor(2, 0, green);
    check(gotGreen, "13: crosshairLineColor(2,0) present");
    check(approx(green[0], 0.235) && approx(green[1], 0.612) && approx(green[2], 0.290),
          "13: Axial line0 colour == Sagittal green");

    double blue[3] = {0.0, 0.0, 0.0};
    const bool gotBlue = scene.crosshairLineColor(2, 1, blue);
    check(gotBlue, "13: crosshairLineColor(2,1) present");
    check(approx(blue[0], 0.235) && approx(blue[1], 0.424) && approx(blue[2], 0.769),
          "13: Axial line1 colour == Coronal blue");

    double red[3] = {0.0, 0.0, 0.0};
    const bool gotRed = scene.crosshairLineColor(0, 1, red);
    check(gotRed, "13: crosshairLineColor(0,1) present");
    check(approx(red[0], 0.769) && approx(red[1], 0.235) && approx(red[2], 0.235),
          "13: Sagittal line1 colour == Axial red");

    // (d) sliceWorldCoord / worldToSliceIndex are inverse for every axis at a
    //     spread of indices (identity direction, unit spacing, zero origin).
    for (int axis = 0; axis < 3; ++axis) {
        const int count = scene.sliceCount(axis);
        const int probes[3] = {0, count / 2, count - 1};
        for (int p = 0; p < 3; ++p) {
            const int idx = probes[p];
            scene.setSliceIndex(axis, idx);
            const int applied = scene.sliceIndex(axis);
            const double world = scene.sliceWorldCoord(axis);
            const int back = scene.worldToSliceIndex(axis, world);
            check(back == applied, "13: worldToSliceIndex(sliceWorldCoord) round-trips");
        }
    }

    // (e) out-of-range world coordinates clamp to [0, count-1].
    const int lowClamp = scene.worldToSliceIndex(2, -100.0);
    check(lowClamp == 0, "13: world below range clamps to 0");
    const int highClamp = scene.worldToSliceIndex(2, 999.0);
    check(highClamp == scene.sliceCount(2) - 1, "13: world above range clamps to count-1");

    // (f) no-volume probes: worldToSliceIndex -> -1, sliceWorldCoord -> 0.0.
    xq::XQRenderScene empty;
    check(empty.worldToSliceIndex(2, 0.0) == -1, "13: no volume -> worldToSliceIndex -1");
    check(empty.sliceWorldCoord(2) == 0.0, "13: no volume -> sliceWorldCoord 0.0");
}

// 14. B2c: surface single anatomical green + resident per-slice cut actors.
void test_surface_green_and_slice_cuts()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(80);
    const xq::RenderStats s = scene.upsertNode(id, make_surface_node(id));
    check(s.ok, "14: surface added");

    // (a) model wears the anatomical green (0.25, 0.80, 0.35).
    double rgb[3] = {-1.0, -1.0, -1.0};
    const bool gotColor = scene.nodeColor(id, rgb);
    check(gotColor, "14: nodeColor present for surface");
    check(approx(rgb[0], 0.25) && approx(rgb[1], 0.80) && approx(rgb[2], 0.35),
          "14: surface colour == anatomical green");

    // (b) before a volume, cut actors already exist (built at addSurface; plane
    //     positions become meaningful once a volume lands). This test asserts the
    //     "actor built up front" implementation choice (one per slice view).
    check(scene.nodeSliceCutCount(id, xq::ViewId::Axial) == 1,
          "14: no volume -> Axial cut actor present");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Sagittal) == 1,
          "14: no volume -> Sagittal cut actor present");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Coronal) == 1,
          "14: no volume -> Coronal cut actor present");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Volume3D) == 0,
          "14: 3D view -> no cut actor");

    // (c) with a volume: still exactly one cut actor per slice view, none in 3D.
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    const bool okVol = scene.setVolume(volume, nullptr);
    check(okVol, "14: setVolume ok");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Axial) == 1,
          "14: with volume -> Axial cut actor == 1");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Sagittal) == 1,
          "14: with volume -> Sagittal cut actor == 1");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Coronal) == 1,
          "14: with volume -> Coronal cut actor == 1");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Volume3D) == 0,
          "14: with volume -> 3D view cut actor == 0");

    // (d) hiding the node hides the model (probe reflects real actor visibility;
    //     cut-actor visibility follows by construction and is verified on real
    //     hardware -- no headless probe for it, per the brief).
    scene.setNodeVisible(id, false);
    check(!scene.nodeVisible(id), "14: setNodeVisible(false) -> nodeVisible false");
    scene.setNodeVisible(id, true);
    check(scene.nodeVisible(id), "14: setNodeVisible(true) -> nodeVisible true");

    // (e) removing the node clears its cut actors from every slice view.
    scene.removeNode(id);
    check(scene.nodeSliceCutCount(id, xq::ViewId::Axial) == 0,
          "14: removeNode -> Axial cut count 0");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Sagittal) == 0,
          "14: removeNode -> Sagittal cut count 0");
    check(scene.nodeSliceCutCount(id, xq::ViewId::Coronal) == 0,
          "14: removeNode -> Coronal cut count 0");
}

// 15. B2c: a mesh node is hidden on first sight; a user toggle survives re-upsert.
void test_mesh_default_hidden()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(90);
    const xq::RenderStats s1 = scene.upsertNode(id, make_mesh_node(id));
    check(s1.ok, "15: mesh added");
    check(!scene.nodeVisible(id), "15: mesh hidden by default on first sight");

    scene.setNodeVisible(id, true);
    check(scene.nodeVisible(id), "15: setNodeVisible(true) shows the mesh");

    // Re-upsert (payload rebuild) keeps the user's visibility choice.
    const xq::RenderStats s2 = scene.upsertNode(id, make_mesh_node(id));
    check(s2.ok, "15: mesh re-upsert ok");
    check(scene.nodeVisible(id), "15: re-upsert preserves the shown state");

    // A surface node, by contrast, is visible on first sight.
    const xq::NodeId sid(91);
    const xq::RenderStats sSurf = scene.upsertNode(sid, make_surface_node(sid));
    check(sSurf.ok, "15: surface added");
    check(scene.nodeVisible(sid), "15: surface visible by default on first sight");
}

// 16. B2d: the three 3D slice planes have an independent visibility flag that
//     survives a volume reload and never disturbs the 2D slice probes.
void test_image_planes_3d_visible()
{
    // (a) fresh scene: default true; no-volume set/get is a safe no-op (no crash).
    xq::XQRenderScene empty;
    check(empty.imagePlanesVisible3d(), "16: default imagePlanesVisible3d true");
    empty.setImagePlanesVisible3d(false);
    check(!empty.imagePlanesVisible3d(), "16: no-volume set false reads back false");
    empty.setImagePlanesVisible3d(true);
    check(empty.imagePlanesVisible3d(), "16: no-volume set true reads back true");

    // (b) with a volume: baseline 2D probes captured, then hide the 3D planes.
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "16: setVolume ok");
    check(scene.imagePlanesVisible3d(), "16: default true after setVolume");

    const int count0 = scene.sliceCount(0);
    const int count1 = scene.sliceCount(1);
    const int count2 = scene.sliceCount(2);
    const int index0 = scene.sliceIndex(0);
    const int index1 = scene.sliceIndex(1);
    const int index2 = scene.sliceIndex(2);

    scene.setImagePlanesVisible3d(false);
    check(!scene.imagePlanesVisible3d(), "16: set false reads back false");

    // (c) hiding the 3D planes leaves the 2D slice probes untouched.
    check(scene.sliceCount(0) == count0 && scene.sliceCount(1) == count1
              && scene.sliceCount(2) == count2,
          "16: 3D plane hide leaves sliceCount unchanged");
    check(scene.sliceIndex(0) == index0 && scene.sliceIndex(1) == index1
              && scene.sliceIndex(2) == index2,
          "16: 3D plane hide leaves sliceIndex unchanged");

    // (d) a volume reload re-applies the current flag (false) to rebuilt planes.
    const bool okReload = scene.setVolume(volume, nullptr);
    check(okReload, "16: setVolume reload ok");
    check(!scene.imagePlanesVisible3d(), "16: reload preserves hidden 3D planes");
}

// 16b: B3b master image visibility. setImageVisible hides the whole volume image
//      (2D slices + 3D planes); it is an independent flag from imagePlanesVisible3d
//      and both survive a volume reload. The effective 3D-plane visibility is the
//      AND of the two flags, asserted here at the flag level (headless, no pixels).
void test_image_visible_master()
{
    // (a) fresh scene: default true; no-volume set/get is a safe no-op.
    xq::XQRenderScene empty;
    check(empty.imageVisible(), "16b: default imageVisible true");
    empty.setImageVisible(false);
    check(!empty.imageVisible(), "16b: no-volume set false reads back false");
    empty.setImageVisible(true);
    check(empty.imageVisible(), "16b: no-volume set true reads back true");

    // (b) with a volume: the two flags are independent.
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    check(scene.setVolume(volume, nullptr), "16b: setVolume ok");
    check(scene.imageVisible(), "16b: default true after setVolume");
    check(scene.imagePlanesVisible3d(), "16b: planes3d default true after setVolume");

    scene.setImageVisible(false);
    check(!scene.imageVisible(), "16b: set false reads back false");
    // Turning the 3D-plane flag on while the master image is hidden must not flip
    // the master flag: the composite (imageVisible && imagePlanes3dVisible) still
    // resolves to hidden.
    scene.setImagePlanesVisible3d(true);
    check(scene.imagePlanesVisible3d(),
          "16b: planes3d flag reads back true independently");
    check(!scene.imageVisible(),
          "16b: planes3d toggle leaves the master image hidden");

    // (c) both flags survive a volume reload.
    check(scene.setVolume(volume, nullptr), "16b: setVolume reload ok");
    check(!scene.imageVisible(), "16b: reload preserves hidden master image");
    check(scene.imagePlanesVisible3d(), "16b: reload preserves planes3d flag");

    // (d) re-showing the master image reads back true.
    scene.setImageVisible(true);
    check(scene.imageVisible(), "16b: re-show reads back true");
}

// 17. B2d render-side upload anchor: upsertNode reports the exact source point
//     count as uploadedPointCount, proving the whole geometry reached the
//     renderer (the real-.vtp 84542 anchor is covered by the app-layer test; here
//     a synthetic surface of a known count locks the render-side invariant).
void test_upload_point_anchor()
{
    xq::XQTriangleSurfaceGeometryHandle surface = make_tet_surface();
    const long long srcPoints = static_cast<long long>(surface.pointCount());

    auto geometry = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>(surface);
    xq::XQSurfaceModel model;
    model.setTriangleGeometry(geometry);
    auto payload = std::make_shared<xq::XQSurfaceModelPayload>(std::move(model));
    const xq::NodeId id(100);
    xq::XQDataNode node(id, xq::XQDomainType::SurfaceModel, "anchor", payload);

    xq::XQRenderScene scene;
    const xq::RenderStats s = scene.upsertNode(id, node);
    const long long uploaded = scene.uploadedPointCount(id);
    check(s.ok, "17: surface upsert ok");
    check(s.uploadedPointCount == srcPoints,
          "17: RenderStats.uploadedPointCount == source point count");
    check(uploaded == srcPoints,
          "17: uploadedPointCount(id) == source point count");
}

// 18. B2b: world point -> voxel index (identity direction, unit spacing volume):
//     exact round-trip, clamping, and the no-volume contract.
void test_world_to_voxel_index()
{
    xq::XQRenderScene empty;
    double world0[3] = {1.0, 2.0, 3.0};
    int i = -1;
    int j = -1;
    int k = -1;
    const bool okEmpty = empty.worldToVoxelIndex(world0, &i, &j, &k);
    check(!okEmpty, "18: no volume -> worldToVoxelIndex false");

    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool okVolume = scene.setVolume(volume, nullptr);
    check(okVolume, "18: setVolume ok");

    double world1[3] = {2.0, 3.0, 1.0};
    const bool ok1 = scene.worldToVoxelIndex(world1, &i, &j, &k);
    check(ok1, "18: in-volume point resolves");
    check(i == 2 && j == 3 && k == 1, "18: unit-spacing world == voxel index");

    double world2[3] = {99.0, 99.0, 99.0};
    const bool ok2 = scene.worldToVoxelIndex(world2, &i, &j, &k);
    check(ok2, "18: out-of-range point still resolves");
    check(i == 7 && j == 5 && k == 3, "18: out-of-range clamps to max index");

    double world3[3] = {-9.0, -9.0, -9.0};
    const bool ok3 = scene.worldToVoxelIndex(world3, &i, &j, &k);
    check(ok3, "18: negative point still resolves");
    check(i == 0 && j == 0 && k == 0, "18: negative clamps to index 0");
}

// 19. B2b: the resident seed marker shows on setSeedMarker, hides on
//     clearSeedMarker, resets hidden on a volume reload, and is a no-op with no
//     volume.
void test_seed_marker()
{
    xq::XQRenderScene empty;
    check(!empty.seedMarkerVisible(), "19: fresh scene -> marker hidden");
    empty.setSeedMarker(1, 1, 1); // no volume -> no-op, must not crash
    check(!empty.seedMarkerVisible(), "19: no-volume setSeedMarker is a no-op");

    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "19: setVolume ok");
    check(!scene.seedMarkerVisible(), "19: fresh volume -> marker hidden");

    scene.setSeedMarker(2, 3, 1);
    check(scene.seedMarkerVisible(), "19: setSeedMarker -> visible");

    scene.clearSeedMarker();
    check(!scene.seedMarkerVisible(), "19: clearSeedMarker -> hidden");

    scene.setSeedMarker(1, 1, 1);
    check(scene.seedMarkerVisible(), "19: marker visible again before reload");
    const bool okReload = scene.setVolume(volume, nullptr);
    check(okReload, "19: setVolume reload ok");
    check(!scene.seedMarkerVisible(), "19: volume reload resets marker hidden");
}

// 21. Path control-point glyph markers: count tracks setPathControlPoints /
//     clearPathControlPoints, a fresh volume starts empty, and a no-volume set is
//     a harmless no-op (no built actors -> nothing to fill).
void test_path_control_markers()
{
    xq::XQRenderScene empty;
    check(empty.pathControlPointCount() == 0, "21: fresh scene -> 0 control points");
    const std::vector<std::array<double, 3>> two = {{{1.0, 2.0, 1.0}},
                                                    {{3.0, 4.0, 2.0}}};
    empty.setPathControlPoints(two); // no volume -> no-op, must not crash
    check(empty.pathControlPointCount() == 0,
          "21: no-volume setPathControlPoints is a no-op");

    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    xq::XQRenderScene scene;
    const bool ok = scene.setVolume(volume, nullptr);
    check(ok, "21: setVolume ok");
    check(scene.pathControlPointCount() == 0, "21: fresh volume -> 0 control points");

    scene.setPathControlPoints(two);
    check(scene.pathControlPointCount() == 2, "21: setPathControlPoints({p1,p2}) -> 2");

    const std::vector<std::array<double, 3>> three = {
        {{1.0, 1.0, 1.0}}, {{2.0, 2.0, 1.0}}, {{3.0, 3.0, 2.0}}};
    scene.setPathControlPoints(three);
    check(scene.pathControlPointCount() == 3, "21: replace with 3 -> 3");

    scene.clearPathControlPoints();
    check(scene.pathControlPointCount() == 0, "21: clearPathControlPoints -> 0");

    scene.setPathControlPoints(two);
    check(scene.pathControlPointCount() == 2, "21: set 2 again before reload");

    // Slice-distance filter: a 2D view only shows points near its slice plane.
    // Geometry is origin 0, spacing 1 (band = 1 per axis). Put the Axial (z) slice
    // at z=1; a point at z=1 sits on it, a point at z=3 is two slices away and must
    // be filtered out of the Axial view. The total count stays 2 either way.
    scene.setSliceIndex(2, 1); // Axial slice plane -> world z = 1
    const std::vector<std::array<double, 3>> zSplit = {{{1.0, 1.0, 1.0}},  // on z=1
                                                       {{1.0, 1.0, 3.0}}}; // off (z=3)
    scene.setPathControlPoints(zSplit);
    check(scene.pathControlPointCount() == 2, "21: filtered set still counts 2 total");
    check(scene.pathControlVisibleCount(2) == 1,
          "21: Axial view shows only the on-slice point (z=1)");
    scene.setSliceIndex(2, 3); // move Axial plane to world z = 3
    check(scene.pathControlVisibleCount(2) == 1,
          "21: moving Axial slice to z=3 now shows the other point");

    scene.setPathControlPoints(two);
    const bool okReload = scene.setVolume(volume, nullptr);
    check(okReload, "21: setVolume reload ok");
    check(scene.pathControlPointCount() == 0, "21: volume reload resets to 0");
}

// 20. B3: contour-group node -> 3D actor + distance-filtered slice overlays.
void test_contour_group()
{
    xq::XQRenderScene scene;
    const xq::NodeId id(110);
    const xq::RenderStats s = scene.upsertNode(id, make_contour_node(id));
    check(s.ok, "20: contour-group upsert ok");
    check(scene.hasNode(id), "20: hasNode true after contour upsert");
    // Two contours, 4 points each -> 8 group points reported.
    check(s.pointCount == 8, "20: contour group pointCount == total points (8)");
    // The 3D closed-polyline actor counts as one Volume3D node actor.
    check(scene.nodeActorCount(xq::ViewId::Volume3D) == 1,
          "20: contour group -> 1 Volume3D actor");

    // No volume yet -> no slice plane, so no overlay hits in any slice view.
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Axial) == 0,
          "20: no volume -> no Axial overlay hit");
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Volume3D) == 0,
          "20: 3D view never reports a contour overlay");

    // With an 8x6x4 unit-spacing volume, the Axial slice at index k has world z==k.
    xq::XQImageVolume volume = make_float_volume(8, 6, 4);
    const bool okVol = scene.setVolume(volume, nullptr);
    check(okVol, "20: setVolume ok");

    // Slice at z=1 hits the z=1 contour (centroid within spacing/2), not the z=2 one.
    scene.setSliceIndex(2, 1);
    check(scene.sliceIndex(2) == 1, "20: Axial slice set to 1");
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Axial) == 1,
          "20: Axial slice at z=1 -> one contour hit");

    // Slice at z=3 hits neither contour (both are >= spacing/2 away).
    scene.setSliceIndex(2, 3);
    check(scene.sliceIndex(2) == 3, "20: Axial slice set to 3");
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Axial) == 0,
          "20: Axial slice at z=3 -> no contour hit");

    // Slice at z=2 hits the z=2 contour.
    scene.setSliceIndex(2, 2);
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Axial) == 1,
          "20: Axial slice at z=2 -> one contour hit");

    // An unknown id reports no overlay.
    const xq::NodeId unknown(997);
    check(scene.nodeContourOverlayCount(unknown, xq::ViewId::Axial) == 0,
          "20: unknown id -> no contour overlay");

    // Visibility toggles the contour group like any other node.
    scene.setNodeVisible(id, false);
    check(!scene.nodeVisible(id), "20: setNodeVisible(false) -> nodeVisible false");
    scene.setNodeVisible(id, true);
    check(scene.nodeVisible(id), "20: setNodeVisible(true) -> nodeVisible true");

    // Removing the node clears its overlays from every slice view.
    scene.removeNode(id);
    check(!scene.hasNode(id), "20: removeNode -> hasNode false");
    check(scene.nodeContourOverlayCount(id, xq::ViewId::Axial) == 0,
          "20: removeNode -> no Axial overlay");
}

} // namespace

int main()
{
    test_no_volume_initial_state();
    test_set_volume();
    test_slice_camera_fit();
    test_slice_index_clamp();
    test_type_agnostic_contract();
    test_upsert_path_idempotent();
    test_upsert_surface_mask_flow();
    test_non_renderable();
    test_set_visible();
    test_remove_and_clear();
    test_progressive_surface();
    test_progressive_invalid_connectivity();
    test_opacity_color_safety();
    test_crosshair_axis_color_inverse();
    test_surface_green_and_slice_cuts();
    test_mesh_default_hidden();
    test_image_planes_3d_visible();
    test_image_visible_master();
    test_upload_point_anchor();
    test_world_to_voxel_index();
    test_seed_marker();
    test_path_control_markers();
    test_contour_group();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all render scene checks passed\n");
    return 0;
}
