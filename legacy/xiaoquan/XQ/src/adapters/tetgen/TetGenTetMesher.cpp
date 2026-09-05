// TetGen-backed ITetMesher (design §2a). The kernel header is included ONLY
// here; the public header (TetGenTetMesher.h) carries zero TetGen types (AC7).
//
// Flow mirrors the validated probe (.trellis/workspace/ocean/tetgen-probe/
// probe.cpp::runTetGen): XQ surface -> tetgenio in (one facet per triangle,
// facetmarker = faceId) -> tetrahedralize("pq..a..nnQ", &in, &out) -> XQ tet
// mesh + boundary-face records (faceId from trifacemarker, tetIndex/localFace
// from -nn adjtetlist). The -Y switch is deliberately NOT used: it produces
// boundary slivers in production (memory tetgen-plc-volume-mesh); marker
// conservation does not need it.

#ifndef TETLIBRARY
#define TETLIBRARY
#endif
#include "tetgen.h"

#include "adapters/tetgen/TetGenTetMesher.h"

#include "core/GeometryTypes.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/source/IGeometrySource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace xq {
namespace {

TetMeshResult fail(const std::string& message)
{
    TetMeshResult result;
    result.ok = false;
    result.message = message;
    return result;
}

// maxvol for the TetGen -a switch. With an explicit targetEdgeLength we invert
// the volume of a regular tet of that edge (a^3 / (6*sqrt(2))). Otherwise we
// derive a target edge from the input bounding-box diagonal so a default
// TetMeshParams{} still yields a reasonable mesh that scales with the geometry
// (no hard-coded absolute number).
double resolveMaxVolume(const ReadSpan<Point3>& points, const TetMeshParams& params)
{
    const double sqrt2 = std::sqrt(2.0);
    if (params.targetEdgeLength > 0.0) {
        const double a = params.targetEdgeLength;
        return (a * a * a) / (6.0 * sqrt2);
    }
    Point3 lo = points[0];
    Point3 hi = lo;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Point3& p = points[i];
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
        hi.z = std::max(hi.z, p.z);
    }
    const double diag = norm(sub(hi, lo));
    const double a = diag / 15.0; // ~15 elements across the diagonal: coarse but real
    return (a * a * a) / (6.0 * sqrt2);
}

} // namespace

TetMeshResult TetGenTetMesher::tetrahedralize(const IGeometrySource& surface,
                                              const TetMeshParams& params)
{
    // Acquire the surface through the Source contract (M9a): contiguous point /
    // triangle / faceId views, one virtual dispatch each. The leases pin the
    // backing data for this whole call.
    GeometryLease<Point3> pointLease = surface.acquire_points();
    TriangleLease triLease = surface.acquire_triangles();
    const ReadSpan<Point3>& points = pointLease.span();
    const TriangleView& triView = triLease.view();
    const ReadSpan<SourceTriangle>& tris = triView.triangles;
    const ReadSpan<int>& faceIds = triView.faceIds;

    // Preprocessing. Closedness/manifoldness is validated by VolumeMeshService
    // before it ever calls a mesher; here we only guard empty/invalid input.
    const int numPoints = static_cast<int>(points.size());
    const int numTris = static_cast<int>(tris.size());
    if (numPoints == 0 || numTris == 0) {
        return fail("TetGenTetMesher: input surface is empty");
    }

    // XQ surface -> tetgenio in. One facet per triangle (PLC hard constraint);
    // facetmarker = triangle faceId so the marker channel conserves it.
    tetgenio in, out;
    in.firstnumber = 0;
    in.numberofpoints = numPoints;
    in.pointlist = new REAL[3 * numPoints];
    // Point3 is a packed double[3]; the contiguous point span copies straight
    // into the REAL[3n] TetGen expects (REAL == double).
    static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3]");
    std::memcpy(in.pointlist, points.data(), static_cast<std::size_t>(numPoints) * sizeof(Point3));
    in.numberoffacets = numTris;
    in.facetlist = new tetgenio::facet[numTris];
    in.facetmarkerlist = new int[numTris];
    for (int i = 0; i < numTris; ++i) {
        const SourceTriangle& tri = tris[static_cast<std::size_t>(i)];
        tetgenio::facet& f = in.facetlist[i];
        f.numberofpolygons = 1;
        f.polygonlist = new tetgenio::polygon[1];
        f.numberofholes = 0;
        f.holelist = nullptr;
        tetgenio::polygon& pg = f.polygonlist[0];
        pg.numberofvertices = 3;
        pg.vertexlist = new int[3];
        pg.vertexlist[0] = tri[0];
        pg.vertexlist[1] = tri[1];
        pg.vertexlist[2] = tri[2];
        in.facetmarkerlist[i] = faceIds[static_cast<std::size_t>(i)];
    }

    // Switches: p=PLC, q=min radius-edge ratio, a=max volume, nn=adjtetlist
    // (cell-level boundary connectivity), Q=quiet. No Y (boundary slivers).
    const double minRatio = params.minRadiusEdgeRatio > 0.0 ? params.minRadiusEdgeRatio : 1.414;
    const double maxVol = resolveMaxVolume(points, params);
    char sw[128];
    std::snprintf(sw, sizeof(sw), "pq%.4fa%.6fnnQ", minRatio, maxVol);

    try {
        // Global TetGen entry point; qualify with :: so name lookup does not
        // bind to this member function (also named tetrahedralize).
        ::tetrahedralize(sw, &in, &out);
    } catch (int code) {
        return fail("TetGenTetMesher: tetrahedralize threw code " + std::to_string(code));
    } catch (...) {
        return fail("TetGenTetMesher: tetrahedralize threw unknown error");
    }

    if (out.numberoftetrahedra == 0 || out.tetrahedronlist == nullptr) {
        return fail("TetGenTetMesher: tetrahedralization produced no tets");
    }

    // out -> XQ. pointlist (may include Steiner points) -> addPoint;
    // tetrahedronlist[4k..] -> addTet.
    auto mesh = std::make_shared<XQTetVolumeMeshHandle>();
    for (int i = 0; i < out.numberofpoints; ++i) {
        mesh->addPoint(Point3{out.pointlist[3 * i + 0],
                              out.pointlist[3 * i + 1],
                              out.pointlist[3 * i + 2]});
    }
    for (int t = 0; t < out.numberoftetrahedra; ++t) {
        mesh->addTet(out.tetrahedronlist[4 * t + 0],
                     out.tetrahedronlist[4 * t + 1],
                     out.tetrahedronlist[4 * t + 2],
                     out.tetrahedronlist[4 * t + 3]);
    }

    // Boundary faces: trifacelist -> tri, trifacemarkerlist -> faceId,
    // adjtetlist (non-hull side) -> tetIndex, then find the local face of that
    // tet whose vertex set == the boundary tri's vertices -> localFace.
    // The 4 local faces of a tet, each opposite a corner vertex.
    static const int kLocalFace[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
    TetMeshResult result;
    result.boundaryFaces.reserve(static_cast<std::size_t>(out.numberoftrifaces));
    for (int i = 0; i < out.numberoftrifaces; ++i) {
        TetBoundaryFace bf;
        const int fa = out.trifacelist[3 * i + 0];
        const int fb = out.trifacelist[3 * i + 1];
        const int fc = out.trifacelist[3 * i + 2];
        bf.tri = {fa, fb, fc};
        bf.faceId = out.trifacemarkerlist ? out.trifacemarkerlist[i] : 0;

        // adjtetlist: a -1 means the hull (outside) side; take the real tet.
        int tet = -1;
        if (out.adjtetlist) {
            const int t0 = out.adjtetlist[2 * i + 0];
            const int t1 = out.adjtetlist[2 * i + 1];
            if (t0 >= 0) {
                tet = t0;
            } else if (t1 >= 0) {
                tet = t1;
            }
        }
        bf.tetIndex = tet;

        if (tet >= 0) {
            std::array<int, 3> face = {fa, fb, fc};
            std::sort(face.begin(), face.end());
            const int* cell = &out.tetrahedronlist[4 * tet];
            for (int lf = 0; lf < 4; ++lf) {
                std::array<int, 3> lk = {cell[kLocalFace[lf][0]],
                                         cell[kLocalFace[lf][1]],
                                         cell[kLocalFace[lf][2]]};
                std::sort(lk.begin(), lk.end());
                if (lk == face) {
                    bf.localFace = lf;
                    break;
                }
            }
        }
        result.boundaryFaces.push_back(bf);
    }

    result.ok = true;
    result.message.clear();
    result.mesh = mesh;
    return result;
}

} // namespace xq
