#ifndef XQ_CORE_MESHING_I_TET_MESHER_H
#define XQ_CORE_MESHING_I_TET_MESHER_H

#include "core/XQTetVolumeMeshHandle.h"
#include "core/source/IGeometrySource.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace xq {

// Tetrahedralization parameters. All zero == kernel defaults (no override). The
// concrete kernel (TetGen/MMG, in a future adapter) maps these onto its own
// switches; this struct stays a pure XQ value type so xq_services never links a
// kernel.
struct TetMeshParams {
    double targetEdgeLength = 0.0;   // hmax / TetGen -a target edge length; 0 = kernel default
    double minEdgeLength = 0.0;      // hmin (MMG remesh stage); 0 = kernel default
    double minRadiusEdgeRatio = 0.0; // TetGen -q radius/edge ratio; 0 = kernel default
};

// One boundary-face record: the input triangle's faceId plus the volume cell it
// belongs to and the local face index within that cell. tetIndex/localFace come
// straight from the kernel's face-adjacency output (e.g. TetGen -nn adjtetlist),
// so the service fills MeshBoundaryFace with zero geometric queries.
struct TetBoundaryFace {
    std::array<int, 3> tri{{0, 0, 0}}; // boundary triangle's three point indices (into result.mesh points)
    int faceId = 0;                    // input triangle faceId, conserved through the kernel's marker channel
    int tetIndex = -1;                 // owning volume cell index (into result.mesh tets)
    int localFace = -1;                // 0..3: which local face of that tet is this boundary face
};

// Result of a tetrahedralization: the volume mesh plus the boundary-face records
// (faceId + cell-level connectivity). ok == false carries a message and a null
// mesh.
struct TetMeshResult {
    bool ok = false;
    std::string message;
    std::shared_ptr<XQTetVolumeMeshHandle> mesh; // tets; null unless ok
    std::vector<TetBoundaryFace> boundaryFaces;  // boundary faces + faceId + cell-level connectivity
};

// Closed triangle surface (each triangle carrying a faceId) -> tet volume mesh
// (boundary-face faceId + cell-level connectivity). The surface is consumed
// through the read-only IGeometrySource contract (M9a): the mesher acquires
// points + triangles (with the parallel faceId span) as contiguous views, so a
// TetGen-style kernel can feed REAL[3n]/marker arrays without per-element handle
// access. Pure XQ types in and out; a kernel-backed implementation lives in an
// adapter and is injected into VolumeMeshService. Mirrors the M6 AiService
// backend pattern.
class ITetMesher {
public:
    virtual ~ITetMesher() = default;

    virtual TetMeshResult tetrahedralize(const IGeometrySource& surface,
                                         const TetMeshParams& params) = 0;
};

} // namespace xq

#endif // XQ_CORE_MESHING_I_TET_MESHER_H
