#include "ui/controllers/PathController.h"

#include "core/command/XQCommandStack.h"
#include "services/path/PathService.h"

#include <utility>

namespace xq {

PathController::PathController(XQScene* scene, XQCommandStack* stack)
    : scene_(scene)
    , stack_(stack)
{
}

PathController::PreparedCommand PathController::prepareAddPath(const AddPathIntent& intent)
{
    PreparedCommand prepared;
    if (scene_ == nullptr || stack_ == nullptr) {
        prepared.status = Status::NullScene;
        return prepared;
    }

    PathService::Result result = PathService::createPathCommand(
        scene_, intent.newPathId, intent.name, intent.sourceImageNode,
        intent.controlPoints, intent.spacing);
    if (!result.ok() || result.command == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    prepared.status = Status::Ok;
    prepared.command = std::move(result.command);
    return prepared;
}

// Headless/test convenience: pushes directly to the stack and does NOT fire the
// session sceneChanged callback; GUI code must go through the async runner /
// session gateway. No GUI caller consumes this today (tests only).
PathController::Status PathController::addPath(const AddPathIntent& intent)
{
    PreparedCommand prepared = prepareAddPath(intent);
    if (!prepared.ok()) {
        return prepared.status;
    }
    return stack_->push(std::move(prepared.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
