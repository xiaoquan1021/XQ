#ifndef XQ_ADAPTERS_TETGEN_TET_GEN_TET_MESHER_H
#define XQ_ADAPTERS_TETGEN_TET_GEN_TET_MESHER_H

#include "core/meshing/ITetMesher.h"

namespace xq {

// ITetMesher implementation backed by the vendored TetGen 1.5.1 (+SV outsubfaces
// fix) PLC constrained tetrahedralization (design §2a). Each input surface
// triangle becomes a TetGen facet hard constraint (-p), so the input surface is
// always the output boundary; faceId rides the facetmarker -> trifacemarker
// channel; per-boundary-face cell-level connectivity (tetIndex/localFace) comes
// from TetGen's -nn adjtetlist with zero geometric queries.
//
// The public header carries ZERO TetGen types: tetgen.h appears only in the
// .cpp (AC7), so xq_core / xq_services never link the kernel.
class TetGenTetMesher : public ITetMesher {
public:
    TetMeshResult tetrahedralize(const IGeometrySource& surface,
                                 const TetMeshParams& params) override;
};

} // namespace xq

#endif // XQ_ADAPTERS_TETGEN_TET_GEN_TET_MESHER_H
