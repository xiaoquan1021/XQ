#ifndef XQ_SERVICES_MESHING_VOLUME_MESH_SERVICE_H
#define XQ_SERVICES_MESHING_VOLUME_MESH_SERVICE_H

#include "core/NodeId.h"
#include "core/XQMesh.h"
#include "core/XQSurfaceModel.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>
#include <vector>

namespace xq {

class XQTriangleSurfaceGeometryHandle;
class XQScene;
class ITetMesher;

// Pure domain service that turns a closed triangle surface into a tetrahedral
// volume mesh, with zero external dependencies (no TetGen/MMG yet -- those are
// deferred to a future adapters kernel). Like the other services it computes
// results and (for scene insertion) returns a command; it never mutates the
// scene and holds no scene state.
//
// Algorithm: centroid star tetrahedralization. The closed surface's vertex
// centroid C is added as a point and each surface triangle (a,b,c) spawns a tet
// (a,b,c,C). tetCount == surface triangle count. This is the volume
// generalization of ModelingService's centroid-fan capping. It is correct only
// for domains star-shaped about C (a curved aorta whose centroid lies near the
// wall produces inverted/degenerate tets); the quality summary exposes the
// worst element (incl. flipped sign) rather than discarding it. A real
// TetGen/MMG kernel is left as future work (recorded as tech debt).
//
// Closedness is required: every undirected edge must be shared by exactly two
// triangles, else the service returns NotClosed and produces nothing.
class VolumeMeshService {
public:
    enum class Status {
        Ok,
        NotClosed,      // surface is not a closed 2-manifold
        InvalidSurface, // surface geometry is empty / invalid
        NullScene,      // scene pointer was null
    };

    struct Params {
        // Reserved for future tetrahedralization controls (target edge length,
        // grading, kernel selection). First version takes no parameters.
    };

    struct Result {
        Status status;
        std::shared_ptr<XQMesh> mesh; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    struct CommandResult {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    // Builds a tet volume mesh from a closed triangle surface. faces supplies
    // the boundary-face metadata (kind/capId/name) looked up by faceId; each
    // surface triangle's tet inherits the triangle's tagged faceId so the
    // boundary association is preserved end-to-end. Fails with NotClosed when
    // the surface is not a closed 2-manifold and InvalidSurface when empty.
    //
    // mesher is an optional injected kernel (e.g. a TetGen/MMG adapter). When
    // null the service uses its built-in centroid star tetrahedralization
    // (default, unchanged). When non-null the service delegates the volume
    // tetrahedralization to the kernel and fills the boundary faces (incl.
    // cell-level localFaces) from the kernel's result; a kernel failure maps to
    // InvalidSurface. The closedness check runs before either path.
    static Result buildVolumeMesh(const XQTriangleSurfaceGeometryHandle& surface,
                                  const std::vector<ModelFace>& faces,
                                  const Params& params,
                                  ITetMesher* mesher = nullptr);

    // Builds the command that inserts the volume mesh node, linking it as
    // derived from sourceNodeId (the surface model / surface mesh node). The
    // caller assigns newMeshId. Failure statuses are passed through. mesher is
    // forwarded to buildVolumeMesh (null == star fallback).
    static CommandResult buildVolumeMeshCommand(XQScene* scene,
                                                const NodeId& newMeshId,
                                                const std::string& name,
                                                const XQTriangleSurfaceGeometryHandle& surface,
                                                const std::vector<ModelFace>& faces,
                                                const NodeId& sourceNodeId,
                                                const Params& params,
                                                ITetMesher* mesher = nullptr);

    // Copies the boundary-face metadata (faceId/name/kind/capId) from one mesh
    // to another. cellIds are NOT carried across (they index different cell
    // lists); `to` keeps whatever cellIds it already holds for matching faces,
    // or empty cellIds for faces it does not yet have. Pure data transfer.
    static void transferBoundaryFaces(const XQMesh& from, XQMesh& to);
};

} // namespace xq

#endif // XQ_SERVICES_MESHING_VOLUME_MESH_SERVICE_H
