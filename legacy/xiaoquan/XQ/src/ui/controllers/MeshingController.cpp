#include "ui/controllers/MeshingController.h"

#include "core/XQDataNode.h"
#include "core/XQScene.h"
#include "core/XQSurfaceModel.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/command/XQCommandStack.h"

#include <utility>

namespace xq {
namespace {

// Reads the surface model carried by a scene node, or null if the node is
// missing or holds a different payload.
const XQSurfaceModel* modelOf(XQScene* scene, const NodeId& node)
{
    if (scene == nullptr) {
        return nullptr;
    }
    const XQDataNode* dataNode = scene->find(node);
    if (dataNode == nullptr) {
        return nullptr;
    }
    const auto* payload =
        dynamic_cast<const XQSurfaceModelPayload*>(dataNode->payload().get());
    if (payload == nullptr) {
        return nullptr;
    }
    return &payload->model();
}

} // namespace

MeshingController::MeshingController(XQScene* scene, XQCommandStack* stack, ITetMesher* mesher)
    : scene_(scene)
    , stack_(stack)
    , mesher_(mesher)
{
}

MeshingController::Status MeshingController::buildSurfaceMesh(const SurfaceMeshIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }
    const XQSurfaceModel* model = modelOf(scene_, intent.modelNode);
    if (model == nullptr) {
        return Status::ModelNotFound;
    }

    SurfaceMeshService::CommandResult cmd = SurfaceMeshService::buildSurfaceMeshCommand(
        scene_, intent.newMeshId, intent.name, *model, intent.modelNode, intent.params);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

MeshingController::Status MeshingController::buildVolumeMesh(const VolumeMeshIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }
    const XQSurfaceModel* model = modelOf(scene_, intent.modelNode);
    if (model == nullptr || !model->hasTriangleGeometry()) {
        return Status::ModelNotFound;
    }

    VolumeMeshService::CommandResult cmd = VolumeMeshService::buildVolumeMeshCommand(
        scene_, intent.newMeshId, intent.name, *model->triangleGeometry(),
        model->faces(), intent.sourceNode, intent.params, mesher_);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
