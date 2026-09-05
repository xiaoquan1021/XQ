// scale-probe — throwaway spike driver (task 06-30-scale-probe).
//
// Only compiles under -DXQ_ENABLE_SCALE_PROBE=ON. Drives the M8b-1
// IVoxelSource / IGeometrySource / ReadLease interfaces against real-scale
// synthetic loads over a real Win32 mmap backing store, and prints the numbers
// the interface-shape REPORT is built from. Disposable.
//
// Run:  scale_probe [small|large] [tmpdir]
//   small (default) = ~1M tris / ~0.5M tets / 128^3 voxels (smoke)
//   large           = ~20M tris / ~10M tets / 512^3 voxels (real target)

#include "MappedGeometrySource.h"
#include "MappedVoxelSource.h"
#include "SyntheticLoad.h"
#include "EvictHarness.h"

#include "core/GeometryTypes.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/source/IGeometrySource.h"
#include "core/source/IVoxelSource.h"
#include "visualization/XQSceneRenderer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

namespace {

double ms(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

std::size_t peakWorkingSetMiB()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        return static_cast<std::size_t>(pmc.PeakWorkingSetSize >> 20);
    }
    return 0;
}

std::string blobAbsPath(const std::string& root, const xq::BufferRef& ref)
{
    return (fs::path(root) / ref.relPath).string();
}

// ----- AC2 correctness: mapped source span must equal the generator source ----

bool checkVoxelRoundTrip(const std::string& root, const xq::probe::VoxelBlob& vb,
                         const xq::probe::LoadSpec& spec)
{
    using namespace xq;
    using namespace xq::probe;
    MappedVoxelSource src(blobAbsPath(root, vb.voxels), vb.dims, vb.voxels.sha256,
                          IntegrityMode::FullVerify);
    if (!src.valid()) {
        std::printf("  [AC2 voxel] FAIL: map failed err=%lu\n", src.lastError());
        return false;
    }
    VoxelLease lease = src.acquire_whole();
    if (!lease.view().valid) {
        std::printf("  [AC2 voxel] FAIL: acquire_whole invalid (integrity?)\n");
        return false;
    }
    const VoxelView& v = lease.view();
    const int dx = vb.dims[0], dy = vb.dims[1], dz = vb.dims[2];
    (void)dz;
    const int cx = dx / 2, cy = dy / 2, cz = dz / 2;
    auto genVal = [&](int x, int y, int z) -> std::uint8_t {
        const int rx = x - cx, ry = y - cy, rz = z - cz;
        const std::uint32_t r2 = static_cast<std::uint32_t>(rx * rx + ry * ry + rz * rz);
        std::uint32_t n = r2 ^ spec.seed ^ static_cast<std::uint32_t>(z * 73856093);
        n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU; n ^= n >> 16;
        return static_cast<std::uint8_t>((r2 ^ (n >> 24)) & 0xFFu);
    };
    const std::size_t step = (v.voxelCount() / 100000) ? (v.voxelCount() / 100000) : 1;
    std::size_t mism = 0, checked = 0;
    for (std::size_t i = 0; i < v.voxelCount(); i += step) {
        const int z = static_cast<int>(i / (static_cast<std::size_t>(dx) * dy));
        const int rem = static_cast<int>(i % (static_cast<std::size_t>(dx) * dy));
        const int y = rem / dx;
        const int x = rem % dx;
        const std::uint8_t got = static_cast<std::uint8_t>(v.scalarAt(i, 0));
        if (got != genVal(x, y, z)) {
            ++mism;
        }
        ++checked;
    }
    std::printf("  [AC2 voxel] checked=%zu mismatches=%zu => %s\n",
                checked, mism, mism == 0 ? "PASS" : "FAIL");
    return mism == 0;
}

bool checkGeometryRoundTrip(const std::string& root, const xq::probe::GeometryBlobs& gb)
{
    using namespace xq;
    using namespace xq::probe;

    MappedGeometrySource::Inputs si;
    si.pointsPath = blobAbsPath(root, gb.points);
    si.pointCount = gb.pointCount;
    si.trisPath = blobAbsPath(root, gb.tris);
    si.triCount = gb.triCount;
    si.faceIdPath = blobAbsPath(root, gb.faceId);
    MappedGeometrySource surf(si);

    MappedGeometrySource::Inputs ti;
    ti.tetPointsPath = blobAbsPath(root, gb.volPoints);
    ti.tetPointCount = gb.volPointCount;
    ti.tetsPath = blobAbsPath(root, gb.tets);
    ti.tetCount = gb.tetCount;
    MappedGeometrySource tet(ti);

    if (!surf.valid() || !tet.valid()) {
        std::printf("  [AC2 geom] FAIL: map failed\n");
        return false;
    }

    bool ok = true;

    {
        GeometryLease<Point3> lease = surf.acquire_points();
        if (lease.span().size() != gb.pointCount) {
            std::printf("  [AC2 points] FAIL: size %zu != %zu\n",
                        lease.span().size(), gb.pointCount);
            ok = false;
        } else {
            const Point3& p0 = lease.span()[0];
            const double r = std::sqrt(p0.x * p0.x + p0.y * p0.y);
            const bool good = r > 9.9 && r < 10.1;
            std::printf("  [AC2 points] n=%zu  p0=(%.4f,%.4f,%.4f) r=%.4f => %s\n",
                        lease.span().size(), p0.x, p0.y, p0.z, r, good ? "PASS" : "FAIL");
            ok = ok && good;
        }
    }

    {
        TriangleLease lease = surf.acquire_triangles();
        const auto& tv = lease.view();
        const bool sized = tv.triangles.size() == gb.triCount
                           && tv.faceIds.size() == gb.triCount;
        bool faceOk = true;
        const std::size_t step = (gb.triCount / 50000) ? (gb.triCount / 50000) : 1;
        for (std::size_t t = 0; t < gb.triCount && faceOk; t += step) {
            if (tv.faceIds[t] != static_cast<int>(t % 64u)) faceOk = false;
        }
        std::printf("  [AC2 tris] tris=%zu faceIds=%zu faceId-pattern=%s => %s\n",
                    tv.triangles.size(), tv.faceIds.size(), faceOk ? "ok" : "BAD",
                    (sized && faceOk) ? "PASS" : "FAIL");
        ok = ok && sized && faceOk;
    }

    {
        GeometryLease<SourceTet> lease = tet.acquire_tetrahedra();
        const bool sized = lease.span().size() == gb.tetCount;
        const SourceTet& t0 = lease.span()[0];
        const bool distinct = t0[0] != t0[1] && t0[1] != t0[2] && t0[2] != t0[3];
        std::printf("  [AC2 tets] n=%zu  t0=(%d,%d,%d,%d) => %s\n",
                    lease.span().size(), t0[0], t0[1], t0[2], t0[3],
                    (sized && distinct) ? "PASS" : "FAIL");
        ok = ok && sized && distinct;
    }

    return ok;
}

// ----- AC3 baselines: build resident handles, time VTK upload -----------------

// Which surface upload path to exercise. PeakWorkingSetSize is monotonic within
// a process, so a fair full-vs-LOD peak comparison needs each mode in its OWN
// process (run the probe once per mode). `All` keeps the old single-process
// timing report (peaks not comparable across modes within it).
enum class UploadMode { All, Full, Medium, Far };

void vtkUploadBaseline(const std::string& root, const xq::probe::GeometryBlobs& gb,
                       UploadMode mode)
{
    using namespace xq;
    using namespace xq::probe;

    MappedGeometrySource::Inputs si;
    si.pointsPath = blobAbsPath(root, gb.points);
    si.pointCount = gb.pointCount;
    si.trisPath = blobAbsPath(root, gb.tris);
    si.triCount = gb.triCount;
    si.faceIdPath = blobAbsPath(root, gb.faceId);
    MappedGeometrySource surf(si);
    if (!surf.valid()) {
        std::printf("  [AC3 vtk] skip: surface map failed\n");
        return;
    }

    const auto t0 = clk::now();
    XQTriangleSurfaceGeometryHandle surfHandle;
    {
        GeometryLease<Point3> pl = surf.acquire_points();
        for (std::size_t i = 0; i < pl.span().size(); ++i) {
            surfHandle.addPoint(pl.span()[i]);
        }
        TriangleLease tl = surf.acquire_triangles();
        const auto& tv = tl.view();
        for (std::size_t i = 0; i < tv.triangles.size(); ++i) {
            const SourceTriangle& tr = tv.triangles[i];
            surfHandle.addTriangle(tr[0], tr[1], tr[2], tv.faceIds[i]);
        }
    }
    const auto t1 = clk::now();

    auto runUpload = [&](const char* label, const LodOptions& lod) {
        XQSceneRenderer renderer;
        const auto u0 = clk::now();
        RenderStats rs = renderer.addSurface(surfHandle, lod);
        const auto u1 = clk::now();
        std::printf("  [AC3 vtk %s] build-handle=%.1fms  addSurface=%.1fms  ok=%d "
                    "pts=%lld uploaded=%lld (%.1f%%)  peakWS=%zu MiB\n",
                    label, ms(t0, t1), ms(u0, u1), rs.ok ? 1 : 0, rs.pointCount,
                    rs.uploadedPointCount,
                    rs.pointCount > 0 ? 100.0 * static_cast<double>(rs.uploadedPointCount)
                                            / static_cast<double>(rs.pointCount)
                                      : 0.0,
                    peakWorkingSetMiB());
    };

    LodOptions full;             // LOD off => M9a full upload
    LodOptions med;  med.enabled = true; med.fixedLevel = 1; // medium, faceId kept
    LodOptions farLod;  farLod.enabled = true; farLod.fixedLevel = 2; // far, flat ('far' is a windows.h macro)

    if (mode == UploadMode::Full || mode == UploadMode::All) {
        runUpload("full  ", full);
    }
    if (mode == UploadMode::Medium || mode == UploadMode::All) {
        runUpload("medium", med);
    }
    if (mode == UploadMode::Far || mode == UploadMode::All) {
        runUpload("far   ", farLod);
    }
}

// ----- integrity A vs B comparison (R5) ---------------------------------------

void integrityComparison(const std::string& root, const xq::probe::VoxelBlob& vb)
{
    using namespace xq;
    using namespace xq::probe;
    const std::string path = blobAbsPath(root, vb.voxels);

    {
        MappedVoxelSource a(path, vb.dims, vb.voxels.sha256, IntegrityMode::FullVerify);
        const auto t0 = clk::now();
        VoxelLease l = a.acquire_slab(vb.dims[2] / 2);
        const auto t1 = clk::now();
        std::printf("  [R5 A full-verify] acquire_slab: %.1fms  bytesHashed=%llu (of %zu)  valid=%d\n",
                    ms(t0, t1), (unsigned long long)a.stats().bytesHashed, vb.voxelCount,
                    l.view().valid ? 1 : 0);
    }
    {
        MappedVoxelSource b(path, vb.dims, vb.voxels.sha256, IntegrityMode::SegmentedMerkle);
        const auto t0 = clk::now();
        VoxelLease l = b.acquire_slab(vb.dims[2] / 2);
        const auto t1 = clk::now();
        std::printf("  [R5 B segmented]   acquire_slab: %.1fms  bytesHashed=%llu (of %zu)  segs=%llu  valid=%d\n",
                    ms(t0, t1), (unsigned long long)b.stats().bytesHashed, vb.voxelCount,
                    (unsigned long long)b.stats().segmentsChecked, l.view().valid ? 1 : 0);
    }
}

} // namespace

int main(int argc, char** argv)
{
    using namespace xq::probe;

    std::setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered: survive a crash mid-run

    Scale scale = Scale::Small;
    if (argc > 1 && std::string(argv[1]) == "large") {
        scale = Scale::Large;
    }
    std::string tmp = (argc > 2) ? argv[2]
                                 : (fs::temp_directory_path() / "xq_scale_probe").string();
    fs::create_directories(tmp);

    // Optional 3rd arg = isolated upload mode (full|medium|far). When set, run
    // ONLY that surface upload and report its process peak working set, so the
    // AC-B2 full-vs-LOD peak comparison is fair (PeakWorkingSetSize is monotonic
    // within a process; comparing modes requires one process each). Without it,
    // the probe runs the full AC1-AC4 sweep as before.
    UploadMode uploadMode = UploadMode::All;
    bool isolatedUpload = false;
    if (argc > 3) {
        const std::string m = argv[3];
        if (m == "full") { uploadMode = UploadMode::Full; isolatedUpload = true; }
        else if (m == "medium") { uploadMode = UploadMode::Medium; isolatedUpload = true; }
        else if (m == "far") { uploadMode = UploadMode::Far; isolatedUpload = true; }
    }

    const LoadSpec spec = specFor(scale);
    std::printf("=== scale-probe (%s)  assetRoot=%s%s ===\n",
                scale == Scale::Large ? "large" : "small", tmp.c_str(),
                isolatedUpload ? (std::string("  uploadMode=") + argv[3]).c_str() : "");
    std::printf("  mem(start) peakWS=%zu MiB\n", peakWorkingSetMiB());

    auto tg0 = clk::now();
    GeometryBlobs gb = generateGeometry(tmp, spec);
    VoxelBlob vb = generateVoxels(tmp, spec);
    auto tg1 = clk::now();
    std::printf("  [R1 gen] %.0fms  tris=%zu tets=%zu voxels=%zu  mem peakWS=%zu MiB\n",
                ms(tg0, tg1), gb.triCount, gb.tetCount, vb.voxelCount, peakWorkingSetMiB());

    // Isolated upload mode: only the surface upload of interest, then stop. This
    // keeps the process peak attributable to that one upload path (AC-B2).
    if (isolatedUpload) {
        std::printf("-- AC-B2 isolated surface upload --\n");
        vtkUploadBaseline(tmp, gb, uploadMode);
        std::printf("=== done (isolated %s)  peakWS=%zu MiB ===\n",
                    argv[3], peakWorkingSetMiB());
        return 0;
    }

    std::printf("-- AC2 round-trip --\n");
    const bool ac2voxel = checkVoxelRoundTrip(tmp, vb, spec);
    const bool ac2geom = checkGeometryRoundTrip(tmp, gb);
    const bool ac2 = ac2voxel && ac2geom;

    std::printf("-- AC3 baselines --\n");
    {
        const auto t0 = clk::now();
        MappedVoxelSource v(blobAbsPath(tmp, vb.voxels), vb.dims, vb.voxels.sha256,
                            IntegrityMode::FullVerify);
        xq::VoxelLease l = v.acquire_whole();
        const auto t1 = clk::now();
        std::printf("  [AC3 mmap voxel open+verify+acquire_whole] %.1fms  ok=%d  mem peakWS=%zu MiB\n",
                    ms(t0, t1), l.view().valid ? 1 : 0, peakWorkingSetMiB());
    }
    vtkUploadBaseline(tmp, gb, UploadMode::All);

    std::printf("-- R5 integrity A vs B (single slab) --\n");
    integrityComparison(tmp, vb);

    std::printf("-- AC4 evict harness --\n");
    EvictResult er = runEvictHarness(blobAbsPath(tmp, vb.voxels), vb.dims, vb.voxels.sha256);
    std::printf("  %s\n  %s\n", er.experimentA.c_str(), er.experimentB.c_str());

    std::printf("=== done. AC2=%s  mem peakWS=%zu MiB ===\n",
                ac2 ? "PASS" : "FAIL", peakWorkingSetMiB());
    return ac2 ? 0 : 1;
}
