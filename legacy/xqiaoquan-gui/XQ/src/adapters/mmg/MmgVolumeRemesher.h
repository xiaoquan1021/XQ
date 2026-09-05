#ifndef XQ_ADAPTERS_MMG_MMG_VOLUME_REMESHER_H
#define XQ_ADAPTERS_MMG_MMG_VOLUME_REMESHER_H

#include "core/meshing/ITetMesher.h"

namespace xq {

// MMG3D-backed volume remesher (design §2b). MMG3D is a pure remesher: it takes
// an EXISTING volume mesh (points + tets + boundary triangles carrying faceId in
// the triangle ref) and improves it under hmin/hmax size control, keeping the
// boundary fixed (MMG3D_IPARAM_nosurf=1) so the faceId marker is conserved
// through the triangle ref channel. It cannot tetrahedralize a bare surface
// (memory mmg-cannot-tetrahedralize-from-surface); the caller feeds it the
// TetGen fill stage's TetMeshResult.
//
// This class does NOT implement ITetMesher (it takes a TetMeshResult, not a
// surface). The TetGen->MMG two-stage ITetMesher is TetGenThenMmg.
//
// The public header carries ZERO MMG types: libmmg3d.h appears only in the .cpp
// (AC7), so xq_core / xq_services never link the kernel.
class MmgVolumeRemesher {
public:
    // Remeshes the initial volume mesh in place (returns a fresh result). The
    // returned boundaryFaces carry tri (0-based point indices into the new mesh)
    // and faceId; tetIndex/localFace are left -1 here and rebuilt by the
    // combining class (MMG emits no adjtetlist).
    TetMeshResult remesh(const TetMeshResult& initial, const TetMeshParams& params);
};

} // namespace xq

#endif // XQ_ADAPTERS_MMG_MMG_VOLUME_REMESHER_H
