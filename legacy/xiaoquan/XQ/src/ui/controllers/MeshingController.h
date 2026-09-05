#ifndef XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H
#define XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H

#include "core/NodeId.h"
#include "services/meshing/SurfaceMeshService.h"
#include "services/meshing/VolumeMeshService.h"

#include <string>

namespace xq {

class XQScene;
class XQCommandStack;
class ITetMesher;

// Thin UI-facing controller for the meshing stage. It reads the chosen surface
// model from its scene node payload, asks SurfaceMeshService / VolumeMeshService
// for a mesh-insertion command, and commits it through the command stack. It
// implements no meshing itself and never mutates the scene directly.
//
// Unlike the contour group, the surface model has a payload type, so the
// controller reads the model straight from its node (modelNode) rather than
// taking it on the intent -- keeping the shell thin and the source binding
// authoritative.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class MeshingController {
public:
    MeshingController(XQScene* scene, XQCommandStack* stack, ITetMesher* mesher = nullptr);

    enum class Status {
        Ok,
        Rejected,        // the service rejected the input (no scene change)
        ModelNotFound,   // modelNode is missing or carries no surface model
        NullScene,
    };

    struct SurfaceMeshIntent {
        NodeId newMeshId;
        std::string name;
        NodeId modelNode;                // surface-model node to mesh
        SurfaceMeshService::Params params;
    };
    Status buildSurfaceMesh(const SurfaceMeshIntent& intent);

    struct VolumeMeshIntent {
        NodeId newMeshId;
        std::string name;
        NodeId modelNode;                // surface-model node supplying the closed surface
        NodeId sourceNode;               // tree parent of the volume mesh (e.g. the surface-mesh node)
        VolumeMeshService::Params params;
    };
    Status buildVolumeMesh(const VolumeMeshIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
    ITetMesher* mesher_;   // borrowed (owned by the app shell); null = star fallback
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H
