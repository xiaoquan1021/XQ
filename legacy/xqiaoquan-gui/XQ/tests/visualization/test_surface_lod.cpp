// M9b-B LOD/decimation tests. Headless (offscreen) like test_scene_renderer:
// build a multi-region surface, drive addSurface with fixed LOD levels, and
// assert deterministic upload-count reduction + faceId preservation at the
// medium level. CHECK-macro style, never asserts side-effecting calls.

#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/ReadSpan.h"
#include "core/source/ResidentSurfaceSource.h"
#include "visualization/SurfaceLodBuilder.h"
#include "visualization/XQSceneRenderer.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <cstdio>
#include <future>
#include <map>
#include <memory>
#include <set>
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

// A grid surface tessellated into a (n-1)x(n-1)x2 triangle mesh on the z=0
// plane, partitioned into `regions` faceId bands along x. Enough triangles that
// QuadricDecimation can actually reduce. Returns a shared handle.
std::shared_ptr<xq::XQTriangleSurfaceGeometryHandle> makeGridSurface(int n, int regions)
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
            // faceId by x-band so adjacent columns share a region (decimatable).
            const int band = (x * regions) / (n - 1);
            const int faceId = band + 1; // 1-based, distinct per band
            h->addTriangle(idx(x, y), idx(x + 1, y), idx(x + 1, y + 1), faceId);
            h->addTriangle(idx(x, y), idx(x + 1, y + 1), idx(x, y + 1), faceId);
        }
    }
    return h;
}

// AC-B2/B3: fixed medium/far levels upload fewer points than full; pointCount
// stays the source count; render stays non-degenerate (actor present).
void test_fixed_level_reduces_upload()
{
    auto surf = makeGridSurface(40, 4); // 40x40 grid -> 2*39*39 = 3042 triangles
    const long long srcPts = static_cast<long long>(surf->pointCount());

    xq::XQSceneRenderer renderer;

    // Full (LOD off).
    xq::RenderStats full = renderer.addSurface(*surf);
    check(full.ok && full.actorCount == 1, "full add ok");
    check(full.pointCount == srcPts, "full pointCount == source");
    check(full.uploadedPointCount == srcPts, "full uploaded == source");

    // Medium (fixedLevel 1).
    renderer.clear();
    xq::LodOptions med;
    med.enabled = true;
    med.fixedLevel = 1;
    xq::RenderStats m = renderer.addSurface(*surf, med);
    check(m.ok && m.actorCount == 1, "medium add ok");
    check(m.pointCount == srcPts, "medium pointCount still == source (contract)");
    check(m.uploadedPointCount > 0 && m.uploadedPointCount <= srcPts,
          "medium uploaded <= source");
    check(m.uploadedPointCount < srcPts, "medium uploaded strictly < full (reduced)");

    // Far (fixedLevel 2) reduces at least as much as medium.
    renderer.clear();
    xq::LodOptions far;
    far.enabled = true;
    far.fixedLevel = 2;
    xq::RenderStats f = renderer.addSurface(*surf, far);
    check(f.ok && f.actorCount == 1, "far add ok");
    check(f.uploadedPointCount <= m.uploadedPointCount, "far uploaded <= medium");
    check(f.pointCount == srcPts, "far pointCount still == source");
}

// Build a vtkPolyData (points + triangles + faceId cell scalars) from a handle,
// mirroring XQSceneRenderer::build_surface (which is private). Used to drive
// SurfaceLodBuilder directly for the per-cell faceId invariant.
vtkSmartPointer<vtkPolyData> toPolyData(const xq::XQTriangleSurfaceGeometryHandle& h)
{
    vtkNew<vtkPoints> pts;
    pts->SetDataTypeToDouble();
    for (const auto& p : h.points()) {
        pts->InsertNextPoint(p.x, p.y, p.z);
    }
    vtkNew<vtkCellArray> tris;
    vtkNew<vtkFloatArray> faceIds;
    faceIds->SetName("faceId");
    faceIds->SetNumberOfComponents(1);
    const auto& triVec = h.triangles();
    for (std::size_t i = 0; i < triVec.size(); ++i) {
        tris->InsertNextCell(3);
        tris->InsertCellPoint(triVec[i][0]);
        tris->InsertCellPoint(triVec[i][1]);
        tris->InsertCellPoint(triVec[i][2]);
        faceIds->InsertNextValue(static_cast<float>(h.triangleFaceId(i)));
    }
    vtkSmartPointer<vtkPolyData> pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(pts);
    pd->SetPolys(tris);
    pd->GetCellData()->SetScalars(faceIds);
    return pd;
}

// AC-B4: medium level preserves faceId per region (no cross-region bleed) and
// every output cell's faceId is one that exists in the source. Inspected via
// SurfaceLodBuilder's cell->faceId map directly.
void test_medium_preserves_faceid()
{
    auto surf = makeGridSurface(30, 3); // 3 regions
    xq::ResidentSurfaceSource src(surf);
    auto tl = src.acquire_triangles();
    const auto& faceSpan = tl.view().faceIds;

    std::set<int> srcFaceIds;
    for (std::size_t i = 0; i < faceSpan.size(); ++i) {
        srcFaceIds.insert(faceSpan[i]);
    }
    check(srcFaceIds.size() == 3, "source has 3 faceId regions");

    vtkSmartPointer<vtkPolyData> full = toPolyData(*surf);
    const xq::LodLevels levels = xq::SurfaceLodBuilder::buildSync(full, faceSpan);
    check(levels.ok, "buildSync ok");

    // Level 0 identity map.
    check(levels.level[0].faceMap.valid
              && levels.level[0].faceMap.cellToFaceId.size()
                     == static_cast<std::size_t>(full->GetNumberOfCells()),
          "level0 faceMap identity length");

    // Level 1 (medium): faceMap valid, length == its triangle count, and every
    // entry is a source faceId (region grouping never invents/blends an id).
    const xq::LodLevel& med = levels.level[1];
    check(med.faceMap.valid, "medium faceMap valid");
    check(med.faceMap.cellToFaceId.size()
              == static_cast<std::size_t>(med.triangleCount),
          "medium faceMap length == triangle count");
    bool allInSource = true;
    for (int fid : med.faceMap.cellToFaceId) {
        if (srcFaceIds.find(fid) == srcFaceIds.end()) {
            allInSource = false;
            break;
        }
    }
    check(allInSource, "medium every cell faceId is a source faceId (no bleed)");
    check(med.triangleCount < levels.level[0].triangleCount,
          "medium has fewer triangles than full (decimated)");

    // Level 2 (far): faceId dropped.
    check(!levels.level[2].faceMap.valid, "far level drops faceId");
}

// AC-B1 (async): buildAsync returns immediately (does not block on decimation)
// and yields the same level structure as buildSync once the future resolves.
void test_build_async_matches_sync()
{
    auto surf = makeGridSurface(40, 4);
    xq::ResidentSurfaceSource src(surf);
    auto tl = src.acquire_triangles();
    const auto& faceSpan = tl.view().faceIds;

    vtkSmartPointer<vtkPolyData> full = toPolyData(*surf);
    const xq::LodLevels sync = xq::SurfaceLodBuilder::buildSync(full, faceSpan);

    // Async: launch and assert the call returns a not-yet-necessarily-ready
    // future without having computed inline. We can't reliably assert timing in
    // a unit test, but we CAN assert the future owns its inputs: release the
    // lease/source right after launching, then resolve.
    std::future<xq::LodLevels> fut =
        xq::SurfaceLodBuilder::buildAsync(full, faceSpan);
    check(fut.valid(), "buildAsync returns a valid future");

    const xq::LodLevels async = fut.get();
    check(async.ok, "async buildSync ok");
    check(async.level[0].triangleCount == sync.level[0].triangleCount,
          "async full triangleCount matches sync");
    check(async.level[1].triangleCount == sync.level[1].triangleCount,
          "async medium triangleCount matches sync");
    check(async.level[2].triangleCount == sync.level[2].triangleCount,
          "async far triangleCount matches sync");
    check(async.level[1].faceMap.cellToFaceId == sync.level[1].faceMap.cellToFaceId,
          "async medium cell->faceId matches sync");
}

// AC-B1 (async ownership): buildAsync deep-copies its inputs, so the future
// still resolves correctly after the source/lease that produced `full` and the
// faceId span are destroyed before .get().
void test_build_async_owns_inputs()
{
    std::future<xq::LodLevels> fut;
    long long expectFullTris = 0;
    {
        auto surf = makeGridSurface(30, 3);
        xq::ResidentSurfaceSource src(surf);
        auto tl = src.acquire_triangles();
        vtkSmartPointer<vtkPolyData> full = toPolyData(*surf);
        expectFullTris = full->GetNumberOfCells();
        fut = xq::SurfaceLodBuilder::buildAsync(full, tl.view().faceIds);
        // surf, src, tl, full all destroyed here, before fut.get().
    }
    const xq::LodLevels async = fut.get();
    check(async.ok, "async resolves after inputs destroyed");
    check(async.level[0].triangleCount == expectFullTris,
          "async full triangleCount survives input destruction");
    check(async.level[1].faceMap.valid && !async.level[2].faceMap.valid,
          "async level faceMap validity survives input destruction");
}

// AC-F / interactive: the vtkLODActor assembly path mounts one actor and renders
// non-degenerate headless (vtkLODActor goes full-res without an interactor, so
// the still level is visible). uploadedPointCount reports the still (full) level.
void test_interactive_lod_actor_assembles()
{
    auto surf = makeGridSurface(40, 4);
    const long long srcPts = static_cast<long long>(surf->pointCount());

    xq::XQSceneRenderer renderer;
    xq::LodOptions inter;
    inter.enabled = true;
    inter.interactive = true;
    xq::RenderStats s = renderer.addSurface(*surf, inter);
    check(s.ok && s.actorCount == 1, "interactive LOD actor mounts one actor");
    check(s.pointCount == srcPts, "interactive pointCount == source");
    check(s.uploadedPointCount == srcPts,
          "interactive still level uploaded == full (headless full-res)");

    std::vector<unsigned char> rgba;
    xq::RenderStats r = renderer.renderOffscreenToRgba(64, 64, &rgba);
    check(r.ok, "interactive LOD actor renders offscreen ok");
    bool anyNonBg = false;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (rgba[i] > 20 || rgba[i + 1] > 20 || rgba[i + 2] > 30) {
            anyNonBg = true;
            break;
        }
    }
    check(anyNonBg, "interactive LOD actor render is non-black");
}

} // namespace

int main()
{
    test_fixed_level_reduces_upload();
    test_medium_preserves_faceid();
    test_build_async_matches_sync();
    test_build_async_owns_inputs();
    test_interactive_lod_actor_assembles();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all surface LOD checks passed\n");
    return 0;
}
