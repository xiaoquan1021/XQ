// Two-stage production ITetMesher: TetGen PLC fill -> MMG3D remesh (design §2c).
// Both kernel headers stay out of this translation unit; this file only talks to
// the two adapter public headers (zero kernel types, AC7).
//
// Stage 1 (TetGenTetMesher): surface -> boundary-faithful initial volume mesh
//   with faceId + cell-level connectivity.
// Stage 2 (MmgVolumeRemesher): quality remesh under size control, boundary (and
//   faceId) fixed. MMG emits no adjtetlist, so the remeshed boundaryFaces come
//   back with tetIndex/localFace == -1.
// Stage 3 (here): rebuild tetIndex/localFace from the remeshed mesh by indexing
//   every tet's four local faces; the face shared by exactly one tet is a
//   boundary face, and the boundary tri's sorted vertex set keys straight into
//   that index.

#include "adapters/mmg/TetGenThenMmg.h"

#include "adapters/mmg/MmgVolumeRemesher.h"
#include "adapters/tetgen/TetGenTetMesher.h"
#include "core/XQTetVolumeMeshHandle.h"

#include <algorithm>
#include <array>
#include <map>

namespace xq {

TetMeshResult TetGenThenMmg::tetrahedralize(const IGeometrySource& surface,
                                            const TetMeshParams& params)
{
    // Stage 1: TetGen PLC fill. Pass failures through untouched.
    TetMeshResult initial = TetGenTetMesher().tetrahedralize(surface, params);
    if (!initial.ok) {
        return initial;
    }

    // Stage 2: MMG3D quality remesh. Pass failures through untouched.
    TetMeshResult remeshed = MmgVolumeRemesher().remesh(initial, params);
    if (!remeshed.ok || !remeshed.mesh) {
        return remeshed;
    }

    // Stage 3: rebuild per-boundary-face cell-level connectivity. The 4 local
    // faces of a tet, each opposite a corner vertex (same convention as the
    // TetGen adapter).
    static const int kLocalFace[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};

    // Index every local face by its sorted vertex set -> (tetIndex, localFace,
    // count). A face seen by exactly one tet is on the boundary.
    struct FaceOwner {
        int tetIndex = -1;
        int localFace = -1;
        int count = 0;
    };
    std::map<std::array<int, 3>, FaceOwner> faceMap;
    const XQTetVolumeMeshHandle& mesh = *remeshed.mesh;
    const std::size_t tetCount = mesh.tetCount();
    for (std::size_t t = 0; t < tetCount; ++t) {
        const XQTetVolumeMeshHandle::Tet& cell = mesh.tet(t);
        for (int lf = 0; lf < 4; ++lf) {
            std::array<int, 3> key = {cell[kLocalFace[lf][0]], cell[kLocalFace[lf][1]],
                                      cell[kLocalFace[lf][2]]};
            std::sort(key.begin(), key.end());
            FaceOwner& owner = faceMap[key];
            owner.tetIndex = static_cast<int>(t);
            owner.localFace = lf;
            ++owner.count;
        }
    }

    // For each remeshed boundary face, look up its sorted tri in the index. Only
    // a face owned by exactly one tet (count == 1) is a genuine boundary face;
    // fill its tetIndex/localFace.
    for (TetBoundaryFace& bf : remeshed.boundaryFaces) {
        std::array<int, 3> key = {bf.tri[0], bf.tri[1], bf.tri[2]};
        std::sort(key.begin(), key.end());
        auto it = faceMap.find(key);
        if (it != faceMap.end() && it->second.count == 1) {
            bf.tetIndex = it->second.tetIndex;
            bf.localFace = it->second.localFace;
        }
    }

    return remeshed;
}

} // namespace xq
