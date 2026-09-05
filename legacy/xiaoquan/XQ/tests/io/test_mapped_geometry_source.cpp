#include "core/GeometryTypes.h"
#include "core/source/IGeometrySource.h"
#include "io/blob/MerkleSidecar.h"
#include "io/blob/Sha256.h"
#include "io/source/MappedGeometrySource.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cout << "check failed: " << what << std::endl;
        ++g_failures;
    }
}

fs::path freshDir()
{
    const fs::path dir = fs::temp_directory_path() / "xq_mapped_geometry_source_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

void writeBytes(const fs::path& p, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream f(p.string(), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

// Canonical little-endian encoders matching the writer's blob format.
void putF64(std::vector<std::uint8_t>& out, double v)
{
    std::uint8_t buf[8];
    std::memcpy(buf, &v, 8); // x86 little-endian
    out.insert(out.end(), buf, buf + 8);
}
void putI32(std::vector<std::uint8_t>& out, int v)
{
    std::int32_t v32 = static_cast<std::int32_t>(v);
    std::uint8_t buf[4];
    std::memcpy(buf, &v32, 4);
    out.insert(out.end(), buf, buf + 4);
}

// --- a small known surface: 4 points, 2 triangles, 2 faceIds ----------------
const xq::Point3 kPoints[4] = {
    {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
const int kTris[2][3] = {{0, 1, 2}, {0, 2, 3}};
const int kFaceIds[2] = {7, 9};

// --- a small known tet mesh: 5 points, 2 tets -------------------------------
const xq::Point3 kVolPoints[5] = {
    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
const int kTets[2][4] = {{0, 1, 2, 3}, {1, 2, 3, 4}};

std::vector<std::uint8_t> pointsBlob()
{
    std::vector<std::uint8_t> b;
    for (const auto& p : kPoints) { putF64(b, p.x); putF64(b, p.y); putF64(b, p.z); }
    return b;
}
std::vector<std::uint8_t> trisBlob()
{
    std::vector<std::uint8_t> b;
    for (const auto& t : kTris) { putI32(b, t[0]); putI32(b, t[1]); putI32(b, t[2]); }
    return b;
}
std::vector<std::uint8_t> faceIdBlob()
{
    std::vector<std::uint8_t> b;
    for (int f : kFaceIds) putI32(b, f);
    return b;
}
std::vector<std::uint8_t> volPointsBlob()
{
    std::vector<std::uint8_t> b;
    for (const auto& p : kVolPoints) { putF64(b, p.x); putF64(b, p.y); putF64(b, p.z); }
    return b;
}
std::vector<std::uint8_t> tetsBlob()
{
    std::vector<std::uint8_t> b;
    for (const auto& t : kTets) { putI32(b, t[0]); putI32(b, t[1]); putI32(b, t[2]); putI32(b, t[3]); }
    return b;
}

// AC1: surface source -> points/tris/faceId span == source, zero-copy.
void test_surface_matches_source(const fs::path& dir)
{
    const fs::path pp = dir / "points.bin";
    const fs::path tp = dir / "tris.bin";
    const fs::path fp = dir / "faceId.bin";
    writeBytes(pp, pointsBlob());
    writeBytes(tp, trisBlob());
    writeBytes(fp, faceIdBlob());

    xq::MappedGeometrySource::Inputs in;
    in.points = {pp.string(), 4, {}};
    in.tris = {tp.string(), 2, {}};
    in.faceId = {fp.string(), 0, {}}; // count taken from tris
    xq::MappedGeometrySource src(in);
    check(src.valid(), "surface source valid");

    const xq::GeometryMeta m = src.meta();
    check(m.valid && m.pointCount == 4 && m.triangleCount == 2 && m.tetCount == 0,
          "surface meta correct");

    auto pl = src.acquire_points();
    const auto& ps = pl.span();
    bool psame = ps.size() == 4;
    for (std::size_t i = 0; psame && i < 4; ++i) {
        psame = ps[i].x == kPoints[i].x && ps[i].y == kPoints[i].y
                && ps[i].z == kPoints[i].z;
    }
    check(psame, "points span == source (AC1)");

    auto tl = src.acquire_triangles();
    const auto& tv = tl.view();
    bool tsame = tv.triangles.size() == 2 && tv.faceIds.size() == 2;
    for (std::size_t i = 0; tsame && i < 2; ++i) {
        tsame = tv.triangles[i][0] == kTris[i][0] && tv.triangles[i][1] == kTris[i][1]
                && tv.triangles[i][2] == kTris[i][2] && tv.faceIds[i] == kFaceIds[i];
    }
    check(tsame, "tris + faceId span == source (AC1)");

    // Zero-copy: tet acquire on a surface-only source is empty (partial source).
    check(src.acquire_tetrahedra().span().empty(),
          "surface-only source has empty tet lease (AC2 partial)");
}

// AC1: tet source -> volPoints/tets span == source.
void test_tet_matches_source(const fs::path& dir)
{
    const fs::path vp = dir / "volPoints.bin";
    const fs::path tp = dir / "tets.bin";
    writeBytes(vp, volPointsBlob());
    writeBytes(tp, tetsBlob());

    xq::MappedGeometrySource::Inputs in;
    in.volPoints = {vp.string(), 5, {}};
    in.tets = {tp.string(), 2, {}};
    xq::MappedGeometrySource src(in);
    check(src.valid(), "tet source valid");

    const xq::GeometryMeta m = src.meta();
    check(m.valid && m.pointCount == 5 && m.tetCount == 2 && m.triangleCount == 0,
          "tet meta correct");

    auto pl = src.acquire_points();
    const auto& ps = pl.span();
    bool psame = ps.size() == 5;
    for (std::size_t i = 0; psame && i < 5; ++i) {
        psame = ps[i].x == kVolPoints[i].x && ps[i].y == kVolPoints[i].y
                && ps[i].z == kVolPoints[i].z;
    }
    check(psame, "tet volPoints span == source (AC1)");

    auto tl = src.acquire_tetrahedra();
    const auto& ts = tl.span();
    bool tsame = ts.size() == 2;
    for (std::size_t i = 0; tsame && i < 2; ++i) {
        tsame = ts[i][0] == kTets[i][0] && ts[i][1] == kTets[i][1]
                && ts[i][2] == kTets[i][2] && ts[i][3] == kTets[i][3];
    }
    check(tsame, "tets span == source (AC1)");

    // Surface acquire on a tet-only source is empty.
    check(src.acquire_triangles().view().triangles.empty(),
          "tet-only source has empty triangle lease (AC2 partial)");
}

// AC2: bad size / missing file / wrong count -> invalid source.
void test_invalid_inputs(const fs::path& dir)
{
    // Size mismatch: declare 5 points but write only 4.
    {
        const fs::path pp = dir / "bad_points.bin";
        const fs::path tp = dir / "bad_tris.bin";
        const fs::path fp = dir / "bad_faceId.bin";
        writeBytes(pp, pointsBlob()); // 4 points
        writeBytes(tp, trisBlob());
        writeBytes(fp, faceIdBlob());
        xq::MappedGeometrySource::Inputs in;
        in.points = {pp.string(), 5, {}}; // claims 5 -> size != 5*3*8
        in.tris = {tp.string(), 2, {}};
        in.faceId = {fp.string(), 0, {}};
        xq::MappedGeometrySource src(in);
        check(!src.valid(), "size-mismatch source invalid (AC2)");
    }
    // Missing file.
    {
        xq::MappedGeometrySource::Inputs in;
        in.points = {(dir / "nope_points.bin").string(), 4, {}};
        in.tris = {(dir / "nope_tris.bin").string(), 2, {}};
        in.faceId = {(dir / "nope_faceId.bin").string(), 0, {}};
        xq::MappedGeometrySource src(in);
        check(!src.valid(), "missing-file source invalid (AC2)");
    }
    // Empty inputs (no role) -> invalid.
    {
        xq::MappedGeometrySource::Inputs in;
        xq::MappedGeometrySource src(in);
        check(!src.valid(), "empty source invalid (AC2)");
    }
}

// FullVerify memoization: first acquire hashes the blob once; re-acquire does not.
void test_fullverify_memo(const fs::path& dir)
{
    const fs::path pp = dir / "memo_points.bin";
    const fs::path tp = dir / "memo_tris.bin";
    const fs::path fp = dir / "memo_faceId.bin";
    writeBytes(pp, pointsBlob());
    writeBytes(tp, trisBlob());
    writeBytes(fp, faceIdBlob());

    xq::MappedGeometrySource::Inputs in;
    in.points = {pp.string(), 4, {}};
    in.tris = {tp.string(), 2, {}};
    in.faceId = {fp.string(), 0, {}};
    in.mode = xq::MappedGeometrySource::IntegrityMode::FullVerify;
    xq::MappedGeometrySource src(in);
    check(src.valid(), "memo source valid");

    (void)src.acquire_points();
    const auto s1 = src.stats();
    check(s1.bytesHashed > 0, "FullVerify hashes points blob on first acquire");
    (void)src.acquire_points();
    const auto s2 = src.stats();
    check(s2.bytesHashed == s1.bytesHashed, "FullVerify memoized: no re-hash (2nd acquire)");
}

// AC2 (assurance): under SegmentedMerkle a tampered blob with a clean sidecar
// fails the lazy check -> acquire returns empty. Proves the verify path is live
// (not elided), the false-green guard for geometry integrity.
void test_segmented_tamper(const fs::path& dir)
{
    const std::vector<std::uint8_t> clean = pointsBlob(); // 96 bytes
    const fs::path pp = dir / "seg_points.bin";
    const fs::path mp = dir / "seg_points.merkle";
    writeBytes(pp, clean);
    check(xq::write_sidecar(clean, 32, mp.string()), "write points sidecar");

    // tris/faceId also need sidecars under SegmentedMerkle.
    const std::vector<std::uint8_t> trisB = trisBlob();
    const std::vector<std::uint8_t> fB = faceIdBlob();
    const fs::path tp = dir / "seg_tris.bin";
    const fs::path tmp = dir / "seg_tris.merkle";
    const fs::path fpath = dir / "seg_faceId.bin";
    const fs::path fmp = dir / "seg_faceId.merkle";
    writeBytes(tp, trisB);
    writeBytes(fpath, fB);
    check(xq::write_sidecar(trisB, 32, tmp.string()), "write tris sidecar");
    check(xq::write_sidecar(fB, 32, fmp.string()), "write faceId sidecar");

    // Tamper the points blob (clean sidecar reused -> lazy check must fail).
    std::vector<std::uint8_t> tampered = clean;
    tampered[10] ^= 0x01;
    writeBytes(pp, tampered);

    xq::MappedGeometrySource::Inputs in;
    in.points = {pp.string(), 4, mp.string()};
    in.tris = {tp.string(), 2, tmp.string()};
    in.faceId = {fpath.string(), 0, fmp.string()};
    in.mode = xq::MappedGeometrySource::IntegrityMode::SegmentedMerkle;
    xq::MappedGeometrySource src(in);
    // Root verifies against clean sidecar so the source maps; mismatch is caught
    // lazily when the tampered points blob is touched.
    check(src.valid(), "tampered-points source still maps (clean sidecar root)");
    check(src.acquire_points().span().empty(),
          "acquire on tampered segment -> empty lease (integrity live)");
    // tris is clean -> still acquirable.
    check(!src.acquire_triangles().view().triangles.empty(),
          "clean tris blob still acquirable under SegmentedMerkle");
}

// FullVerify with a content-address anchor (BufferRef.sha256): a correct anchor
// passes; a blob tampered on disk under the original anchor must fail the verify
// so acquire returns empty. This is the regression guard for the FullVerify
// integrity fix (previously the digest was computed then discarded -> tamper
// undetected). Surface is all-or-nothing, so a failed points verify also blocks
// the source from yielding data.
void test_fullverify_anchor(const fs::path& dir)
{
    const std::vector<std::uint8_t> pts = pointsBlob();
    const std::vector<std::uint8_t> tris = trisBlob();
    const std::vector<std::uint8_t> fids = faceIdBlob();
    const std::string ptsSha = xq::Sha256::hashHex(pts);
    const std::string trisSha = xq::Sha256::hashHex(tris);
    const std::string fidsSha = xq::Sha256::hashHex(fids);

    const fs::path pp = dir / "anchor_points.bin";
    const fs::path tp = dir / "anchor_tris.bin";
    const fs::path fp = dir / "anchor_faceId.bin";
    writeBytes(pp, pts);
    writeBytes(tp, tris);
    writeBytes(fp, fids);

    auto makeInputs = [&]() {
        xq::MappedGeometrySource::Inputs in;
        in.points = {pp.string(), 4, {}, ptsSha};
        in.tris = {tp.string(), 2, {}, trisSha};
        in.faceId = {fp.string(), 0, {}, fidsSha}; // count from tris
        in.mode = xq::MappedGeometrySource::IntegrityMode::FullVerify;
        return in;
    };

    // Correct anchors -> acquire yields real data.
    {
        xq::MappedGeometrySource src(makeInputs());
        check(src.valid(), "anchored source valid (correct sha)");
        check(src.acquire_points().span().size() == 4,
              "FullVerify with matching anchor -> points acquirable");
    }

    // Tamper the points blob on disk but keep the original (now-stale) anchor:
    // the whole-blob digest no longer matches -> verify fails -> empty lease.
    {
        std::vector<std::uint8_t> tampered = pts;
        tampered[10] ^= 0x01;
        writeBytes(pp, tampered);

        xq::MappedGeometrySource src(makeInputs()); // anchor still ptsSha (stale)
        check(src.acquire_points().span().empty(),
              "FullVerify anchor mismatch (tampered blob) -> empty lease");
    }

    // Restore the clean blob so the shared dir is left consistent.
    writeBytes(pp, pts);
}

} // namespace

int main()
{
    const fs::path dir = freshDir();

    test_surface_matches_source(dir);
    test_tet_matches_source(dir);
    test_invalid_inputs(dir);
    test_fullverify_memo(dir);
    test_segmented_tamper(dir);
    test_fullverify_anchor(dir);

    std::error_code ec;
    fs::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all mapped geometry source checks passed" << std::endl;
    return 0;
}
