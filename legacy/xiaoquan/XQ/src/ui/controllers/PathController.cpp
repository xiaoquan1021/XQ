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

PathController::Status PathController::addPath(const AddPathIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }

    PathService::Result result = PathService::createPathCommand(
        scene_, intent.newPathId, intent.name, intent.sourceImageNode,
        intent.controlPoints, intent.spacing);
    if (!result.ok() || result.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(result.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
