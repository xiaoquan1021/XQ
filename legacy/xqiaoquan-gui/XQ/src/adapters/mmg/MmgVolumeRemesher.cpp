// MMG3D-backed volume remesher (design §2b). The kernel header is included ONLY
// here; the public header (MmgVolumeRemesher.h) carries zero MMG types (AC7).
//
// Flow: TetMeshResult (TetGen fill stage) -> MMG5_pMesh (points 1-based, tets
// 1-based, boundary triangles with ref=faceId) -> MMG3D_mmg3dlib under
// hmin/hmax size control and nosurf=1 (fix the boundary) -> back to a fresh
// XQTetVolumeMeshHandle + boundary-face records. faceId rides the triangle ref
// channel in and out. MMG emits no cell-face adjacency, so tetIndex/localFace
// are left -1 here and rebuilt by TetGenThenMmg.
//
// MMG vertex / element / triangle numbering is 1-based; XQ is 0-based. Every
// crossing adds (+1) on the way in and subtracts (-1) on the way out.

#include "mmg/mmg3d/libmmg3d.h"

#include "adapters/mmg/MmgVolumeRemesher.h"

#include "core/GeometryTypes.h"
#include "core/XQTetVolumeMeshHandle.h"

#include <memory>
#include <string>

namespace xq {
namespace {

TetMeshResult fail(const std::string& message)
{
    TetMeshResult result;
    result.ok = false;
    result.message = message;
    return result;
}

} // namespace

TetMeshResult MmgVolumeRemesher::remesh(const TetMeshResult& initial, const TetMeshParams& params)
{
    // Entry validation: MMG needs a real volume mesh plus its boundary triangles
    // (faceId carriers). A bare surface or an empty mesh is rejected, never
    // silently passed through.
    if (!initial.ok) {
        return fail("MmgVolumeRemesher: initial result is not ok");
    }
    if (!initial.mesh) {
        return fail("MmgVolumeRemesher: initial mesh is null");
    }
    if (initial.boundaryFaces.empty()) {
        return fail("MmgVolumeRemesher: initial result has no boundary faces");
    }

    const XQTetVolumeMeshHandle& src = *initial.mesh;
    const int np = static_cast<int>(src.pointCount());
    const int ne = static_cast<int>(src.tetCount());
    const int nt = static_cast<int>(initial.boundaryFaces.size());
    if (np == 0 || ne == 0) {
        return fail("MmgVolumeRemesher: initial mesh has no points or tets");
    }

    MMG5_pMesh mesh = nullptr;
    MMG5_pSol met = nullptr;
    MMG3D_Init_mesh(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);

    // Allocate the mesh: np points, ne tets, no prisms, nt boundary triangles,
    // no quads, no edges.
    if (MMG3D_Set_meshSize(mesh, np, ne, 0, nt, 0, 0) != 1) {
        MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);
        return fail("MmgVolumeRemesher: MMG3D_Set_meshSize failed");
    }

    // Points: XQ 0-based i -> MMG 1-based (i+1). ref 0 (no point reference).
    for (int i = 0; i < np; ++i) {
        const Point3& p = src.point(static_cast<std::size_t>(i));
        if (MMG3D_Set_vertex(mesh, p.x, p.y, p.z, 0, i + 1) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Set_vertex failed");
        }
    }

    // Tets: vertex indices +1 (0-based -> 1-based), position i+1. ref 0.
    for (int t = 0; t < ne; ++t) {
        const XQTetVolumeMeshHandle::Tet& cell = src.tet(static_cast<std::size_t>(t));
        if (MMG3D_Set_tetrahedron(mesh, cell[0] + 1, cell[1] + 1, cell[2] + 1, cell[3] + 1, 0,
                                  t + 1) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Set_tetrahedron failed");
        }
    }

    // Boundary triangles: ref carries the faceId. Vertices +1, position i+1.
    for (int i = 0; i < nt; ++i) {
        const TetBoundaryFace& bf = initial.boundaryFaces[static_cast<std::size_t>(i)];
        if (MMG3D_Set_triangle(mesh, bf.tri[0] + 1, bf.tri[1] + 1, bf.tri[2] + 1, bf.faceId,
                               i + 1) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Set_triangle failed");
        }
    }

    // nosurf=1: forbid surface modifications so the input boundary (and its
    // faceId refs) is preserved exactly -> faceId conservation.
    MMG3D_Set_iparameter(mesh, met, MMG3D_IPARAM_nosurf, 1);

    // Size control. targetEdgeLength -> hmax, minEdgeLength -> hmin. Zero leaves
    // the kernel default (no override).
    if (params.targetEdgeLength > 0.0) {
        MMG3D_Set_dparameter(mesh, met, MMG3D_DPARAM_hmax, params.targetEdgeLength);
    }
    if (params.minEdgeLength > 0.0) {
        MMG3D_Set_dparameter(mesh, met, MMG3D_DPARAM_hmin, params.minEdgeLength);
    }

    const int ier = MMG3D_mmg3dlib(mesh, met);
    std::string warning;
    if (ier == MMG5_STRONGFAILURE) {
        MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);
        return fail("MmgVolumeRemesher: MMG3D_mmg3dlib returned STRONGFAILURE");
    }
    if (ier == MMG5_LOWFAILURE) {
        // The mesh is usable but MMG could not fully meet the size/quality
        // request. Keep it; surface this as a warning in the message.
        warning = "MmgVolumeRemesher: MMG3D_mmg3dlib returned LOWFAILURE (mesh kept)";
    }

    // Read back the remeshed mesh size.
    int outNp = 0, outNe = 0, outNprism = 0, outNt = 0, outNquad = 0, outNa = 0;
    if (MMG3D_Get_meshSize(mesh, &outNp, &outNe, &outNprism, &outNt, &outNquad, &outNa) != 1) {
        MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);
        return fail("MmgVolumeRemesher: MMG3D_Get_meshSize failed");
    }
    if (outNe == 0) {
        MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);
        return fail("MmgVolumeRemesher: remeshed mesh has no tets");
    }

    auto outMesh = std::make_shared<XQTetVolumeMeshHandle>();

    // Points: MMG 1-based, fetched one at a time. Discard ref/isCorner/isRequired
    // flags; only coordinates matter for the XQ handle.
    for (int i = 0; i < outNp; ++i) {
        double c0 = 0.0, c1 = 0.0, c2 = 0.0;
        int ref = 0, isCorner = 0, isRequired = 0;
        if (MMG3D_Get_vertex(mesh, &c0, &c1, &c2, &ref, &isCorner, &isRequired) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Get_vertex failed");
        }
        outMesh->addPoint(Point3{c0, c1, c2});
    }

    // Tets: MMG returns 1-based vertex indices; subtract 1 to land back in XQ
    // 0-based space.
    for (int t = 0; t < outNe; ++t) {
        int v0 = 0, v1 = 0, v2 = 0, v3 = 0, ref = 0, isRequired = 0;
        if (MMG3D_Get_tetrahedron(mesh, &v0, &v1, &v2, &v3, &ref, &isRequired) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Get_tetrahedron failed");
        }
        outMesh->addTet(v0 - 1, v1 - 1, v2 - 1, v3 - 1);
    }

    // Boundary faces: ref -> faceId, vertices -1 -> XQ 0-based. tetIndex/localFace
    // stay -1 (rebuilt by the combining class).
    TetMeshResult result;
    result.boundaryFaces.reserve(static_cast<std::size_t>(outNt));
    for (int i = 0; i < outNt; ++i) {
        int v0 = 0, v1 = 0, v2 = 0, ref = 0, isRequired = 0;
        if (MMG3D_Get_triangle(mesh, &v0, &v1, &v2, &ref, &isRequired) != 1) {
            MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met,
                           MMG5_ARG_end);
            return fail("MmgVolumeRemesher: MMG3D_Get_triangle failed");
        }
        TetBoundaryFace bf;
        bf.tri = {v0 - 1, v1 - 1, v2 - 1};
        bf.faceId = ref;
        bf.tetIndex = -1;
        bf.localFace = -1;
        result.boundaryFaces.push_back(bf);
    }

    MMG3D_Free_all(MMG5_ARG_start, MMG5_ARG_ppMesh, &mesh, MMG5_ARG_ppMet, &met, MMG5_ARG_end);

    result.ok = true;
    result.message = warning;
    result.mesh = outMesh;
    return result;
}

} // namespace xq
