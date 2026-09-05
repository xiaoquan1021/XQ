// AC5 for the TetGen->MMG two-stage ITetMesher (design §8). CHECK-macro style,
// never assert: side-effecting calls are pulled into variables before any
// predicate so a Release /DNDEBUG strip cannot turn a real check into a no-op
// (memory no-side-effect-in-assert).
//
// Geometry helpers (makeCurvedTube, pointInSurface, tetSignedVolume, tetShape)
// are lifted from test_tetgen_volume_mesh.cpp (the validated probe). The curved
// tube's 180deg arc pulls the vertex centroid OUTSIDE the lumen, which is the
// flip scenario the PLC + remesh path must handle.

#include "adapters/mmg/TetGenThenMmg.h"

#include "core/GeometryTypes.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/meshing/ITetMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
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

// Programmatic curved tube: arc 180deg pulls the vertex centroid outside the
// lumen. wall=1 / inlet=2 / outlet=3.
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

// minShape over all tets of a mesh (matches the AC1 quality probe).
double meshMinShape(const xq::XQTetVolumeMeshHandle& tets)
{
    double minShape = 1e30;
    const std::size_t tetCount = tets.tetCount();
    for (std::size_t t = 0; t < tetCount; ++t) {
        const auto& cell = tets.tet(t);
        const double sh = std::fabs(tetShape(tets.point(cell[0]), tets.point(cell[1]),
                                             tets.point(cell[2]), tets.point(cell[3])));
        if (sh < minShape) {
            minShape = sh;
        }
    }
    return minShape;
}

} // namespace

int main()
{
    const double R = 10.0, r = 2.0;
    const int M = 7, N = 16;
    const double arcDeg = 180.0;

    xq::XQTriangleSurfaceGeometryHandle surface = makeCurvedTube(R, r, M, N, arcDeg);
    xq::ResidentSurfaceSource surfaceSource(
        std::shared_ptr<const xq::XQTriangleSurfaceGeometryHandle>(
            std::shared_ptr<const void>(), &surface));

    // ---- AC5.1: TetGen->MMG produces a real volume mesh ----
    xq::TetGenThenMmg mesher;
    xq::TetMeshParams params;
    params.targetEdgeLength = 1.0;
    const xq::TetMeshResult res = mesher.tetrahedralize(surfaceSource, params);
    if (!res.ok) {
        std::fprintf(stderr, "FAIL AC5.1: tetrahedralize not ok: %s\n", res.message.c_str());
        return 1;
    }
    if (res.mesh == nullptr) {
        std::fprintf(stderr, "FAIL AC5.1: result mesh is null\n");
        return 1;
    }
    const xq::XQTetVolumeMeshHandle& tets = *res.mesh;
    const std::size_t tetCount = tets.tetCount();
    if (tetCount == 0) {
        std::fprintf(stderr, "FAIL AC5.1: tetCount == 0\n");
        return 1;
    }

    // ---- AC5.2: remesh tet count tracks hmax (smaller hmax -> more tets) ----
    xq::TetMeshParams fine;
    fine.targetEdgeLength = 1.0;
    xq::TetMeshParams coarse;
    coarse.targetEdgeLength = 2.0;
    const xq::TetMeshResult resFine = mesher.tetrahedralize(surfaceSource, fine);
    const xq::TetMeshResult resCoarse = mesher.tetrahedralize(surfaceSource, coarse);
    if (!resFine.ok || resFine.mesh == nullptr) {
        std::fprintf(stderr, "FAIL AC5.2: fine remesh not ok: %s\n", resFine.message.c_str());
        return 1;
    }
    if (!resCoarse.ok || resCoarse.mesh == nullptr) {
        std::fprintf(stderr, "FAIL AC5.2: coarse remesh not ok: %s\n", resCoarse.message.c_str());
        return 1;
    }
    const std::size_t fineCount = resFine.mesh->tetCount();
    const std::size_t coarseCount = resCoarse.mesh->tetCount();
    if (fineCount == coarseCount) {
        std::fprintf(stderr,
                     "FAIL AC5.2: tet count did not track hmax (fine=%zu coarse=%zu, equal)\n",
                     fineCount, coarseCount);
        return 1;
    }
    if (!(fineCount > coarseCount)) {
        std::fprintf(stderr,
                     "FAIL AC5.2: smaller hmax did not yield more tets (fine=%zu coarse=%zu)\n",
                     fineCount, coarseCount);
        return 1;
    }

    // ---- AC5.3: quality not degraded (minShape > 0.1) ----
    const double minShape = meshMinShape(tets);
    if (!(minShape > 0.1)) {
        std::fprintf(stderr, "FAIL AC5.3: minShape %.4e not > 0.1\n", minShape);
        return 1;
    }

    // ---- AC5.4: faceId conservation (output set == input {1,2,3}, no 0) ----
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
        std::fprintf(stderr, "FAIL AC5.4: input faceId set is not {1,2,3}\n");
        return 1;
    }
    if (outputFaceIds.count(0) != 0) {
        std::fprintf(stderr, "FAIL AC5.4: output boundary faces contain faceId 0 (lost marker)\n");
        return 1;
    }
    if (outputFaceIds != expected) {
        std::fprintf(stderr, "FAIL AC5.4: output faceId set != input {1,2,3}\n");
        return 1;
    }

    // ---- AC5.5: cell-level connectivity rebuilt correctly ----
    static const int kLocalFace[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
    int checked = 0;
    for (const xq::TetBoundaryFace& bf : res.boundaryFaces) {
        ++checked;
        if (bf.tetIndex < 0 || static_cast<std::size_t>(bf.tetIndex) >= tetCount) {
            std::fprintf(stderr, "FAIL AC5.5: boundary face has invalid tetIndex %d\n",
                         bf.tetIndex);
            return 1;
        }
        if (bf.localFace < 0 || bf.localFace > 3) {
            std::fprintf(stderr, "FAIL AC5.5: boundary face has invalid localFace %d\n",
                         bf.localFace);
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
                         "FAIL AC5.5: tet %d local face %d vertex set != boundary tri vertex set\n",
                         bf.tetIndex, bf.localFace);
            return 1;
        }
    }
    if (checked == 0) {
        std::fprintf(stderr, "FAIL AC5.5: no boundary faces to check\n");
        return 1;
    }

    std::printf("PASS test_mmg_volume_mesh: tets=%zu boundaryFaces=%d minShape=%.4e "
                "(hmax=1.0 -> %zu tets, hmax=2.0 -> %zu tets)\n",
                tetCount, checked, minShape, fineCount, coarseCount);
    return 0;
}
