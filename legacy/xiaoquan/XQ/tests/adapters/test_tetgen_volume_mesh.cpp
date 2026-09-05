// AC1~AC4 for the TetGen-backed ITetMesher (design §8). CHECK-macro style, never
// assert: side-effecting calls are pulled into variables before any predicate so
// a Release /DNDEBUG strip cannot turn a real check into a no-op (memory
// no-side-effect-in-assert).
//
// Geometry helpers (makeCurvedTube, pointInSurface, tetSignedVolume, tetShape)
// are lifted from the validated probe (.trellis/workspace/ocean/tetgen-probe/
// probe.cpp); the curved tube's 180deg arc pulls the vertex centroid OUTSIDE the
// lumen, which is exactly where the star-fan fallback produces out-of-domain
// tets and the PLC kernel does not.

#include "adapters/tetgen/TetGenTetMesher.h"

#include "core/GeometryTypes.h"
#include "core/XQMesh.h"
#include "core/XQSurfaceModel.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/meshing/ITetMesher.h"
#include "services/meshing/VolumeMeshService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <set>
#include <vector>

using xq::Point3;
using xq::add;
using xq::cross;
using xq::dot;
using xq::norm;
using xq::scale;
using xq::sub;

namespace {

// ------------------------------------------------------------------ geometry --

bool rayTri(const Point3& orig, const Point3& dir, const Point3& v0, const Point3& v1,
            const Point3& v2, double& tOut)
{
    const double eps = 1e-12;
    Point3 e1 = sub(v1, v0);
    Point3 e2 = sub(v2, v0);
    Point3 pvec = cross(dir, e2);
    double det = dot(e1, pvec);
    if (std::fabs(det) < eps) {
        return false;
    }
    double inv = 1.0 / det;
    Point3 tvec = sub(orig, v0);
    double u = dot(tvec, pvec) * inv;
    if (u < -1e-9 || u > 1.0 + 1e-9) {
        return false;
    }
    Point3 qvec = cross(tvec, e1);
    double v = dot(dir, qvec) * inv;
    if (v < -1e-9 || u + v > 1.0 + 1e-9) {
        return false;
    }
    double t = dot(e2, qvec) * inv;
    if (t <= 1e-9) {
        return false;
    }
    tOut = t;
    return true;
}

// Even-odd point-in-closed-surface, majority over random rays.
bool pointInSurface(const xq::XQTriangleSurfaceGeometryHandle& s, const Point3& p)
{
    static std::mt19937 rng(1234567u);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    int votesInside = 0;
    const int rays = 5;
    const std::size_t triCount = s.triangleCount();
    for (int r = 0; r < rays; ++r) {
        Point3 dir{uni(rng), uni(rng), uni(rng)};
        double len = norm(dir);
        if (len < 1e-6) {
            dir = {1.0, 0.0, 0.0};
        } else {
            dir = scale(dir, 1.0 / len);
        }
        int crossings = 0;
        for (std::size_t i = 0; i < triCount; ++i) {
            const auto& t = s.triangle(i);
            double tt;
            if (rayTri(p, dir, s.point(t[0]), s.point(t[1]), s.point(t[2]), tt)) {
                ++crossings;
            }
        }
        if (crossings % 2 == 1) {
            ++votesInside;
        }
    }
    return votesInside * 2 > rays;
}

double tetSignedVolume(const Point3& a, const Point3& b, const Point3& c, const Point3& d)
{
    return dot(cross(sub(b, a), sub(c, a)), sub(d, a)) / 6.0;
}

double tetShape(const Point3& a, const Point3& b, const Point3& c, const Point3& d)
{
    double v = std::fabs(tetSignedVolume(a, b, c, d));
    const Point3 pp[4] = {a, b, c, d};
    double sumL2 = 0.0;
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            double e = norm(sub(pp[i], pp[j]));
            sumL2 += e * e;
            ++n;
        }
    }
    double meanL2 = sumL2 / n;
    double denom = std::pow(meanL2, 1.5);
    if (denom < 1e-30) {
        return 0.0;
    }
    return v / denom * (6.0 * std::sqrt(2.0));
}

// Programmatic curved tube (probe::makeCurvedTube): arc 180deg pulls the vertex
// centroid outside the lumen. wall=1 / inlet=2 / outlet=3.
xq::XQTriangleSurfaceGeometryHandle makeCurvedTube(double R, double r, int M, int N, double arcDeg)
{
    xq::XQTriangleSurfaceGeometryHandle s;
    const double kPi = 3.14159265358979323846;
    const double arc = arcDeg * kPi / 180.0;
    std::vector<std::vector<int>> ringIdx(M, std::vector<int>(N));
    for (int i = 0; i < M; ++i) {
        double theta = arc * static_cast<double>(i) / static_cast<double>(M - 1);
        Point3 center{R * std::cos(theta), R * std::sin(theta), 0.0};
        Point3 radial{std::cos(theta), std::sin(theta), 0.0};
        Point3 zaxis{0.0, 0.0, 1.0};
        for (int k = 0; k < N; ++k) {
            double a = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(N);
            Point3 off = add(scale(radial, r * std::cos(a)), scale(zaxis, r * std::sin(a)));
            ringIdx[i][k] = s.addPoint(add(center, off));
        }
    }
    for (int i = 0; i + 1 < M; ++i) {
        for (int k = 0; k < N; ++k) {
            int k1 = (k + 1) % N;
            int a = ringIdx[i][k], b = ringIdx[i][k1];
            int c = ringIdx[i + 1][k], d = ringIdx[i + 1][k1];
            s.addTriangle(a, b, d, 1);
            s.addTriangle(a, d, c, 1);
        }
    }
    auto capRing = [&](const std::vector<int>& ring, int faceId, bool reverse) {
        Point3 c{0, 0, 0};
        for (int idx : ring) {
            c = add(c, s.point(static_cast<std::size_t>(idx)));
        }
        c = scale(c, 1.0 / static_cast<double>(ring.size()));
        int ci = s.addPoint(c);
        for (int k = 0; k < N; ++k) {
            int k1 = (k + 1) % N;
            if (!reverse) {
                s.addTriangle(ring[k], ring[k1], ci, faceId);
            } else {
                s.addTriangle(ring[k1], ring[k], ci, faceId);
            }
        }
    };
    capRing(ringIdx[0], 2, false);
    capRing(ringIdx[M - 1], 3, true);
    return s;
}

Point3 centroidOf(const xq::XQTriangleSurfaceGeometryHandle& s)
{
    Point3 c{0, 0, 0};
    const std::size_t n = s.pointCount();
    for (std::size_t i = 0; i < n; ++i) {
        c = add(c, s.point(i));
    }
    return scale(c, 1.0 / static_cast<double>(n));
}

} // namespace

int main()
{
    const double R = 10.0, r = 2.0;
    const int M = 7, N = 16;
    const double arcDeg = 180.0;

    xq::XQTriangleSurfaceGeometryHandle surface = makeCurvedTube(R, r, M, N, arcDeg);

    // Sanity: this is the flip scenario (vertex centroid outside the lumen).
    const Point3 surfCentroid = centroidOf(surface);
    const bool centroidInside = pointInSurface(surface, surfCentroid);
    if (centroidInside) {
        std::fprintf(stderr, "FAIL setup: curved-tube centroid is inside; not a flip scenario\n");
        return 1;
    }

    // ---- AC1: TetGen kernel quality (no flips, no out-of-domain, minShape>0.1) ----
    xq::TetGenTetMesher mesher;
    xq::ResidentSurfaceSource surfaceSource(
        std::shared_ptr<const xq::XQTriangleSurfaceGeometryHandle>(
            std::shared_ptr<const void>(), &surface));
    const xq::TetMeshResult res = mesher.tetrahedralize(surfaceSource, xq::TetMeshParams{});
    if (!res.ok) {
        std::fprintf(stderr, "FAIL AC1: tetrahedralize not ok: %s\n", res.message.c_str());
        return 1;
    }
    if (res.mesh == nullptr) {
        std::fprintf(stderr, "FAIL AC1: result mesh is null\n");
        return 1;
    }
    const xq::XQTetVolumeMeshHandle& tets = *res.mesh;
    const std::size_t tetCount = tets.tetCount();
    if (tetCount == 0) {
        std::fprintf(stderr, "FAIL AC1: tetCount == 0\n");
        return 1;
    }

    int pos = 0, neg = 0;
    for (std::size_t t = 0; t < tetCount; ++t) {
        const auto& cell = tets.tet(t);
        const double v = tetSignedVolume(tets.point(cell[0]), tets.point(cell[1]),
                                         tets.point(cell[2]), tets.point(cell[3]));
        if (v > 0) {
            ++pos;
        } else {
            ++neg;
        }
    }
    const int majPos = (pos >= neg) ? 1 : 0;
    int flipped = 0;
    int outOfDomain = 0;
    double minShape = 1e30;
    for (std::size_t t = 0; t < tetCount; ++t) {
        const auto& cell = tets.tet(t);
        const Point3 A = tets.point(cell[0]);
        const Point3 B = tets.point(cell[1]);
        const Point3 C = tets.point(cell[2]);
        const Point3 D = tets.point(cell[3]);
        const double v = tetSignedVolume(A, B, C, D);
        if (((v > 0) ? 1 : 0) != majPos) {
            ++flipped;
        }
        const Point3 cen = scale(add(add(A, B), add(C, D)), 0.25);
        if (!pointInSurface(surface, cen)) {
            ++outOfDomain;
        }
        const double sh = std::fabs(tetShape(A, B, C, D));
        if (sh < minShape) {
            minShape = sh;
        }
    }
    if (flipped != 0) {
        std::fprintf(stderr, "FAIL AC1: %d flipped tets (mixed sign)\n", flipped);
        return 1;
    }
    if (outOfDomain != 0) {
        std::fprintf(stderr, "FAIL AC1: %d out-of-domain tets\n", outOfDomain);
        return 1;
    }
    if (!(minShape > 0.1)) {
        std::fprintf(stderr, "FAIL AC1: minShape %.4e not > 0.1\n", minShape);
        return 1;
    }

    // ---- AC2: same input star-fan (mesher=nullptr) has out-of-domain tets ----
    // Proves the production PLC path is strictly better on the flip scenario.
    std::vector<xq::ModelFace> faces;
    const xq::VolumeMeshService::Result starRes = xq::VolumeMeshService::buildVolumeMesh(
        surface, faces, xq::VolumeMeshService::Params{}, nullptr);
    if (!starRes.ok() || starRes.mesh == nullptr) {
        std::fprintf(stderr, "FAIL AC2: star-fan build did not produce a mesh\n");
        return 1;
    }
    const std::shared_ptr<xq::XQTetVolumeMeshHandle> starTets = starRes.mesh->volumeTets();
    if (starTets == nullptr || starTets->tetCount() == 0) {
        std::fprintf(stderr, "FAIL AC2: star-fan mesh has no tets\n");
        return 1;
    }
    int starOutOfDomain = 0;
    const std::size_t starCount = starTets->tetCount();
    for (std::size_t t = 0; t < starCount; ++t) {
        const auto& cell = starTets->tet(t);
        const Point3 A = starTets->point(cell[0]);
        const Point3 B = starTets->point(cell[1]);
        const Point3 C = starTets->point(cell[2]);
        const Point3 D = starTets->point(cell[3]);
        const Point3 cen = scale(add(add(A, B), add(C, D)), 0.25);
        if (!pointInSurface(surface, cen)) {
            ++starOutOfDomain;
        }
    }
    if (starOutOfDomain == 0) {
        std::fprintf(stderr,
                     "FAIL AC2: star-fan had no out-of-domain tets; the contrast (TetGen=0 "
                     "vs star>0) does not hold\n");
        return 1;
    }

    // ---- AC3: output boundary faceId set == input {1,2,3}, no 0, not mixed ----
    std::set<int> inputFaceIds;
    for (std::size_t i = 0; i < surface.triangleCount(); ++i) {
        inputFaceIds.insert(surface.triangleFaceId(i));
    }
    std::set<int> outputFaceIds;
    for (const xq::TetBoundaryFace& bf : res.boundaryFaces) {
        outputFaceIds.insert(bf.faceId);
    }
    const std::set<int> expected{1, 2, 3};
    if (inputFaceIds != expected) {
        std::fprintf(stderr, "FAIL AC3: input faceId set is not {1,2,3}\n");
        return 1;
    }
    if (outputFaceIds.count(0) != 0) {
        std::fprintf(stderr, "FAIL AC3: output boundary faces contain faceId 0 (lost marker)\n");
        return 1;
    }
    if (outputFaceIds != expected) {
        std::fprintf(stderr, "FAIL AC3: output faceId set != input {1,2,3}\n");
        return 1;
    }

    // ---- AC4: each (tetIndex, localFace) -> that tet's local face vertex set == tri ----
    static const int kLocalFace[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
    int checked = 0;
    for (const xq::TetBoundaryFace& bf : res.boundaryFaces) {
        ++checked;
        if (bf.tetIndex < 0 || static_cast<std::size_t>(bf.tetIndex) >= tetCount) {
            std::fprintf(stderr, "FAIL AC4: boundary face has invalid tetIndex %d\n", bf.tetIndex);
            return 1;
        }
        if (bf.localFace < 0 || bf.localFace > 3) {
            std::fprintf(stderr, "FAIL AC4: boundary face has invalid localFace %d\n", bf.localFace);
            return 1;
        }
        std::array<int, 3> triKey{bf.tri[0], bf.tri[1], bf.tri[2]};
        std::sort(triKey.begin(), triKey.end());
        const auto& cell = tets.tet(static_cast<std::size_t>(bf.tetIndex));
        std::array<int, 3> faceKey{cell[kLocalFace[bf.localFace][0]],
                                   cell[kLocalFace[bf.localFace][1]],
                                   cell[kLocalFace[bf.localFace][2]]};
        std::sort(faceKey.begin(), faceKey.end());
        if (faceKey != triKey) {
            std::fprintf(stderr,
                         "FAIL AC4: tet %d local face %d vertex set != boundary tri vertex set\n",
                         bf.tetIndex, bf.localFace);
            return 1;
        }
    }
    if (checked == 0) {
        std::fprintf(stderr, "FAIL AC4: no boundary faces to check\n");
        return 1;
    }

    std::printf("PASS test_tetgen_volume_mesh: tets=%zu boundaryFaces=%d minShape=%.4e "
                "(star out-of-domain=%d, TetGen out-of-domain=0)\n",
                tetCount, checked, minShape, starOutOfDomain);
    return 0;
}
