#ifndef XQ_SERVICES_MESHING_SURFACE_MESH_SERVICE_H
#define XQ_SERVICES_MESHING_SURFACE_MESH_SERVICE_H

#include "core/NodeId.h"
#include "core/XQMesh.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>

namespace xq {

class XQSurfaceModel;
class XQScene;

// Pure domain service that turns a (capped) surface model into a surface mesh
// (XQMesh carrying real triangle geometry + boundary faces + quality summary).
// Like the M1..M3 services it computes results and (for scene insertion)
// returns a command; it never mutates the scene and holds no scene state. Zero
// external dependencies (no VTK/TetGen/Qt): triangle quality is XQ's own code.
//
// First version: the surface mesh is the model's triangle geometry copied
// verbatim (no remeshing / edge-length refinement yet; Params reserves room for
// it). Boundary faces are recovered directly from the per-triangle face ids
// that ModelingService stamps at generation time -- no geometric re-association.
class SurfaceMeshService {
public:
    enum class Status {
        Ok,
        InvalidModel, // model has no usable triangle geometry
        NullScene,    // scene pointer was null
    };

    struct Params {
        // Reserved for a future edge-length upper bound that would trigger
        // subdivision. First version performs no refinement.
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

    // Builds a surface mesh from the model's triangle geometry. Each ModelFace
    // becomes a MeshBoundaryFace (same faceId/name/kind/capId) whose cellIds are
    // the triangle indices tagged with that faceId. Fails (InvalidModel) when
    // the model has no valid triangle geometry.
    static Result buildSurfaceMesh(const XQSurfaceModel& model, const Params& params);

    // Builds the command that inserts the surface mesh node, linking it as
    // derived from the source model node. The caller assigns newMeshId and
    // passes the scene id of the model node (the source of the derived
    // relation). Failure statuses are passed through with a null command.
    static CommandResult buildSurfaceMeshCommand(XQScene* scene,
                                                 const NodeId& newMeshId,
                                                 const std::string& name,
                                                 const XQSurfaceModel& model,
                                                 const NodeId& modelNodeId,
                                                 const Params& params);
};

} // namespace xq

#endif // XQ_SERVICES_MESHING_SURFACE_MESH_SERVICE_H
