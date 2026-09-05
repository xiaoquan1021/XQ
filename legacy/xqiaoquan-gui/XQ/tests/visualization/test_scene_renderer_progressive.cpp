// M9b-C progressive/chunked upload tests. Headless (offscreen) like
// test_scene_renderer: build a multi-region surface, drive addSurfaceProgressive
// with a small chunk size, and assert first-chunk-visible-before-complete (AC1),
// equivalence to the one-shot addSurface (AC2), multi-actor stats semantics
// (AC3), cell->faceId segment offsets (AC4), and no dangling after the source is
// released (AC6). CHECK-macro style; never asserts side-effecting calls.

#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/IGeometrySource.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/source/ResidentTetSource.h"
#include "visualization/ChunkPlan.h"
#include "visualization/XQSceneRenderer.h"

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

bool any_non_black(const std::vector<unsigned char>& rgba)
{
    for (std::size_t i = 0; i + 2 < rgba.size(); i += 4) {
        if (rgba[i] != 0 || rgba[i + 1] != 0 || rgba[i + 2] != 0) {
            return true;
        }
    }
    return false;
}

// An (n-1)x(n-1)x2 triangle grid on z=0, faceId banded along x.
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
            const int band = (x * regions) / (n - 1);
            const int faceId = band + 1;
            h->addTriangle(idx(x, y), idx(x + 1, y), idx(x + 1, y + 1), faceId);
            h->addTriangle(idx(x, y), idx(x + 1, y + 1), idx(x, y + 1), faceId);
        }
    }
    return h;
}

// A small structured tet block: a row of `cells` cubes, each split into 6 tets.
std::shared_ptr<xq::XQTetVolumeMeshHandle> makeTetRow(int cells)
{
    auto h = std::make_shared<xq::XQTetVolumeMeshHandle>();
    // points: (cells+1) x 2 x 2 lattice
    auto pid = [](int i, int j, int k) { return (i * 2 + j) * 2 + k; };
    for (int i = 0; i <= cells; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 2; ++k) {
                h->addPoint({static_cast<double>(i), static_cast<double>(j),
                             static_cast<double>(k)});
            }
        }
    }
    // 6 Freudenthal tets per cube (indices into the 8 corners of cube i).
    const int corner[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                              {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
    const int tetIdx[6][4] = {{0, 1, 3, 7}, {0, 1, 7, 5}, {0, 5, 7, 4},
                              {0, 3, 2, 7}, {0, 2, 6, 7}, {0, 6, 4, 7}};
    auto gpid = [&](int cubeBaseI, int c) {
        const int i = cubeBaseI + corner[c][0];
        const int j = corner[c][1];
        const int k = corner[c][2];
        return (i * 2 + j) * 2 + k;
    };
    (void)pid;
    for (int i = 0; i < cells; ++i) {
        for (int t = 0; t < 6; ++t) {
            h->addTet(gpid(i, tetIdx[t][0]), gpid(i, tetIdx[t][1]),
                      gpid(i, tetIdx[t][2]), gpid(i, tetIdx[t][3]));
        }
    }
    return h;
}

class BadConnectivitySource final : public xq::IGeometrySource {
public:
    explicit BadConnectivitySource(bool withTet)
        : withTet_(withTet)
        , keepalive_(std::make_shared<int>(0))
    {
        points_.push_back({0.0, 0.0, 0.0});
        points_.push_back({1.0, 0.0, 0.0});
        points_.push_back({0.0, 1.0, 0.0});
        if (withTet_) {
            points_.push_back({0.0, 0.0, 1.0});
            tets_.push_back(xq::SourceTet{0, 1, 2, 4}); // 4 is out of range
        } else {
            triangles_.push_back(xq::SourceTriangle{0, 1, 3}); // 3 is out of range
            faceIds_.push_back(1);
        }
    }

    xq::GeometryMeta meta() const override
    {
        xq::GeometryMeta meta;
        meta.valid = true;
        meta.pointCount = points_.size();
        meta.triangleCount = triangles_.size();
        meta.tetCount = tets_.size();
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
            keepalive_, xq::ReadSpan<xq::SourceTriangle>(triangles_.data(), triangles_.size()),
            keepalive_, xq::ReadSpan<int>(faceIds_.data(), faceIds_.size()));
    }

    xq::GeometryLease<xq::SourceTet> acquire_tetrahedra() const override
    {
        return xq::GeometryLease<xq::SourceTet>::borrow(
            keepalive_, xq::ReadSpan<xq::SourceTet>(tets_.data(), tets_.size()));
    }

private:
    bool withTet_ = false;
    std::shared_ptr<const void> keepalive_;
    std::vector<xq::Point3> points_;
    std::vector<xq::SourceTriangle> triangles_;
    std::vector<int> faceIds_;
    std::vector<xq::SourceTet> tets_;
};

// AC1/AC3: chunked upload fires N callbacks; the first already has an actor and
// completedChunkCount < chunkCount with a non-black render; final stats report
// source point count, actorCount == chunkCount, all chunks completed.
void test_progressive_first_chunk_visible()
{
    auto surf = makeGridSurface(40, 4); // 3042 triangles
    const long long srcPts = static_cast<long long>(surf->pointCount());
    xq::ResidentSurfaceSource src(surf);

    xq::XQSceneRenderer renderer;
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 500; // 3042 tris -> 7 chunks

    int callbacks = 0;
    bool firstChunkPartialAndVisible = false;
    auto onChunk = [&](const xq::RenderStats& cum) {
        ++callbacks;
        if (callbacks == 1) {
            std::vector<unsigned char> rgba;
            xq::RenderStats r = renderer.renderOffscreenToRgba(64, 64, &rgba);
            firstChunkPartialAndVisible =
                cum.completedChunkCount == 1 && cum.chunkCount > 1
                && cum.actorCount >= 1 && r.ok && any_non_black(rgba);
        }
    };

    xq::RenderStats s = renderer.addSurfaceProgressive(src, spec, onChunk);
    check(s.ok, "progressive surface ok");
    check(s.chunkCount == 7, "3042 tris / 500 -> 7 chunks");
    check(callbacks == s.chunkCount, "onChunk fired once per chunk");
    check(firstChunkPartialAndVisible,
          "first chunk: >=1 actor, partial, non-black render before completion (AC1)");
    check(s.pointCount == srcPts, "progressive pointCount == source total (AC3)");
    check(s.actorCount == s.chunkCount, "actorCount == chunkCount (AC3)");
    check(s.completedChunkCount == s.chunkCount, "all chunks completed (AC3)");
}

// AC2: progressive upload renders the same geometry as the one-shot addSurface
// (both non-black, comparable coverage), with consistent cross-chunk colour.
void test_progressive_equivalent_to_oneshot()
{
    auto surf = makeGridSurface(40, 4);
    xq::ResidentSurfaceSource src(surf);

    auto coverage = [](const std::vector<unsigned char>& rgba) {
        std::size_t n = 0;
        for (std::size_t i = 0; i + 2 < rgba.size(); i += 4) {
            if (rgba[i] != 0 || rgba[i + 1] != 0 || rgba[i + 2] != 0) ++n;
        }
        return n;
    };

    xq::XQSceneRenderer oneShot;
    oneShot.addSurface(*surf);
    std::vector<unsigned char> rgbaA;
    oneShot.renderOffscreenToRgba(128, 96, &rgbaA);

    xq::XQSceneRenderer progressive;
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 400;
    progressive.addSurfaceProgressive(src, spec);
    std::vector<unsigned char> rgbaB;
    progressive.renderOffscreenToRgba(128, 96, &rgbaB);

    check(any_non_black(rgbaA) && any_non_black(rgbaB), "both renders non-black");
    const std::size_t covA = coverage(rgbaA);
    const std::size_t covB = coverage(rgbaB);
    // Same geometry/camera: coverage should be close (allow 15% for per-chunk
    // normal AutoOrient differences). Not a bit-equality claim.
    const std::size_t hi = covA > covB ? covA : covB;
    const std::size_t lo = covA > covB ? covB : covA;
    check(hi > 0 && (hi - lo) * 100 <= hi * 15,
          "progressive coverage within 15% of one-shot (AC2 equivalence)");
}

// AC4: the renderer colours a chunk's local cell j with the global faceId
// faceIds[range.begin + j]. The segment-offset mapping is only correct if the
// chunk plan tiles the cells exactly; that invariant is asserted directly here
// against the same plan_chunks the renderer uses (a wrong stride / dropped cell
// would leave a faceId uncovered or double-counted). The faceId span itself is
// also checked to be parallel to the triangles (one id per cell).
void test_chunk_faceid_segment_offset()
{
    auto surf = makeGridSurface(30, 3);
    xq::ResidentSurfaceSource src(surf);
    auto tl = src.acquire_triangles();
    const auto& tris = tl.view().triangles;
    const auto& faceIds = tl.view().faceIds;

    check(faceIds.size() == tris.size(),
          "faceIds parallel to triangles (one id per cell)");

    // Reproduce the renderer's plan and verify each source cell's faceId is
    // mapped by exactly one chunk's segment offset begin+j -> faceIds[begin+j].
    const std::size_t maxPerChunk = 200;
    const std::vector<xq::ChunkRange> chunks =
        xq::plan_chunks(faceIds.size(), maxPerChunk);
    std::vector<int> mappedFrom(faceIds.size(), -1);
    bool offsetsOk = true;
    for (const xq::ChunkRange& range : chunks) {
        for (std::size_t j = 0; j < range.end - range.begin; ++j) {
            const std::size_t g = range.begin + j; // segment offset the renderer uses
            if (g >= faceIds.size() || mappedFrom[g] != -1) {
                offsetsOk = false; // out of range or already mapped (overlap)
            } else {
                mappedFrom[g] = static_cast<int>(faceIds[g]);
            }
        }
    }
    bool everyCellMapped = true;
    for (std::size_t g = 0; g < faceIds.size(); ++g) {
        if (mappedFrom[g] != static_cast<int>(faceIds[g])) everyCellMapped = false;
    }
    check(faceIds.size() > 0, "source has triangles");
    check(offsetsOk, "each cell mapped by exactly one chunk segment offset (AC4)");
    check(everyCellMapped,
          "chunk-local cell j -> faceIds[begin+j] covers every cell once (AC4)");
}

// AC6: after progressive upload, release the source/handle and re-render. Each
// chunk copied its data into VTK-owned arrays (copy-on-upload), so the actors do
// not dangle.
void test_progressive_no_dangling_after_release()
{
    xq::XQSceneRenderer renderer;
    {
        auto surf = makeGridSurface(30, 3);
        xq::ResidentSurfaceSource src(surf);
        xq::ChunkUploadSpec spec;
        spec.maxCellsPerChunk = 300;
        xq::RenderStats s = renderer.addSurfaceProgressive(src, spec);
        check(s.ok && s.actorCount == s.chunkCount, "progressive add ok before release");
        // surf + src destroyed here.
    }
    std::vector<unsigned char> rgba;
    xq::RenderStats r = renderer.renderOffscreenToRgba(64, 64, &rgba);
    check(r.ok && any_non_black(rgba),
          "render after source release is still non-black (no dangling, AC6)");
}

// AC1/AC3 (tet): progressive volume mesh chunks into multiple wireframe actors.
void test_progressive_volume_mesh()
{
    auto tet = makeTetRow(20); // 120 tets
    const long long srcPts = static_cast<long long>(tet->pointCount());
    xq::ResidentTetSource src(tet);

    xq::XQSceneRenderer renderer;
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 30; // 120 tets -> 4 chunks

    int callbacks = 0;
    auto onChunk = [&](const xq::RenderStats&) { ++callbacks; };
    xq::RenderStats s = renderer.addVolumeMeshProgressive(src, spec, onChunk);
    check(s.ok, "progressive volume ok");
    check(s.chunkCount == 4, "120 tets / 30 -> 4 chunks");
    check(callbacks == s.chunkCount, "tet onChunk fired once per chunk");
    check(s.pointCount == srcPts, "tet progressive pointCount == source total");
    check(s.actorCount == s.chunkCount, "tet actorCount == chunkCount");

    std::vector<unsigned char> rgba;
    xq::RenderStats r = renderer.renderOffscreenToRgba(64, 64, &rgba);
    check(r.ok && any_non_black(rgba), "tet progressive render non-black");
}

void test_progressive_rejects_invalid_surface_connectivity()
{
    BadConnectivitySource src(false);
    xq::XQSceneRenderer renderer;
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 1;

    int callbacks = 0;
    xq::RenderStats s =
        renderer.addSurfaceProgressive(src, spec, [&](const xq::RenderStats&) {
            ++callbacks;
        });
    check(!s.ok, "progressive surface rejects out-of-range connectivity");
    check(s.actorCount == 0, "invalid progressive surface adds no actor");
    check(s.chunkCount == 0 && s.completedChunkCount == 0,
          "invalid progressive surface reports no chunks completed");
    check(callbacks == 0, "invalid progressive surface does not call onChunk");
}

void test_progressive_rejects_invalid_tet_connectivity()
{
    BadConnectivitySource src(true);
    xq::XQSceneRenderer renderer;
    xq::ChunkUploadSpec spec;
    spec.maxCellsPerChunk = 1;

    int callbacks = 0;
    xq::RenderStats s =
        renderer.addVolumeMeshProgressive(src, spec, [&](const xq::RenderStats&) {
            ++callbacks;
        });
    check(!s.ok, "progressive tet rejects out-of-range connectivity");
    check(s.actorCount == 0, "invalid progressive tet adds no actor");
    check(s.chunkCount == 0 && s.completedChunkCount == 0,
          "invalid progressive tet reports no chunks completed");
    check(callbacks == 0, "invalid progressive tet does not call onChunk");
}

} // namespace

int main()
{
    test_progressive_first_chunk_visible();
    test_progressive_equivalent_to_oneshot();
    test_chunk_faceid_segment_offset();
    test_progressive_no_dangling_after_release();
    test_progressive_volume_mesh();
    test_progressive_rejects_invalid_surface_connectivity();
    test_progressive_rejects_invalid_tet_connectivity();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all progressive upload checks passed\n");
    return 0;
}
