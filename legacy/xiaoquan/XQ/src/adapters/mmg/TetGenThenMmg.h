#ifndef XQ_ADAPTERS_MMG_TET_GEN_THEN_MMG_H
#define XQ_ADAPTERS_MMG_TET_GEN_THEN_MMG_H

#include "core/meshing/ITetMesher.h"

namespace xq {

// Two-stage production ITetMesher (design §2c): TetGen PLC fill stage produces a
// boundary-faithful initial volume mesh, then MMG3D remeshes it for quality
// under size control while keeping the boundary (and faceId) fixed. This is the
// pairing required because MMG3D cannot tetrahedralize a bare surface (memory
// mmg-cannot-tetrahedralize-from-surface) and TetGen alone does not give the
// size-driven quality of an MMG remesh pass.
//
// After the MMG stage this class rebuilds the per-boundary-face cell-level
// connectivity (tetIndex/localFace) from the remeshed mesh, since MMG emits no
// face-adjacency table.
//
// The public header carries ZERO kernel types: both kernel headers stay in the
// respective adapter .cpp files (AC7).
class TetGenThenMmg : public ITetMesher {
public:
    TetMeshResult tetrahedralize(const IGeometrySource& surface,
                                 const TetMeshParams& params) override;
};

} // namespace xq

#endif // XQ_ADAPTERS_MMG_TET_GEN_THEN_MMG_H
