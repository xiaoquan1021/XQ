#include "ui/controllers/ModelingController.h"

#include "core/XQSurfaceModel.h"
#include "core/command/XQCommandStack.h"

#include <memory>
#include <utility>

namespace xq {

ModelingController::ModelingController(XQScene* scene, XQCommandStack* stack)
    : scene_(scene)
    , stack_(stack)
{
}

ModelingController::Status ModelingController::loft(const LoftIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }

    // Stamp the source contour-group node onto the group copy so the lofted
    // model (and its insertion command) binds back to that node in the tree.
    XQContourGroup group = intent.contourGroup;
    group.setId(intent.contourGroupNode);

    ContourLoftInputBuilder::Result built =
        ContourLoftInputBuilder::buildLoftInput(group, intent.loftOptions);
    if (!built.ok()) {
        return Status::Rejected;
    }

    ModelingService::Result lofted = ModelingService::loftSurface(built.input);
    if (!lofted.ok() || lofted.model == nullptr) {
        return Status::Rejected;
    }

    std::shared_ptr<XQSurfaceModel> model = lofted.model;
    if (intent.capEnds) {
        ModelingService::Result capped =
            ModelingService::capModel(*lofted.model, intent.capOptions);
        if (!capped.ok() || capped.model == nullptr) {
            return Status::Rejected;
        }
        model = capped.model;
    }

    ModelingService::CommandResult cmd = ModelingService::createModelNodeCommand(
        scene_, intent.newModelId, intent.name, model);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
