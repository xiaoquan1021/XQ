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

ModelingController::PreparedCommand ModelingController::prepareLoft(const LoftIntent& intent)
{
    PreparedCommand prepared;
    if (scene_ == nullptr || stack_ == nullptr) {
        prepared.status = Status::NullScene;
        return prepared;
    }

    // Stamp the source contour-group node onto the group copy so the lofted
    // model (and its insertion command) binds back to that node in the tree.
    XQContourGroup group = intent.contourGroup;
    group.setId(intent.contourGroupNode);

    ContourLoftInputBuilder::Result built =
        ContourLoftInputBuilder::buildLoftInput(group, intent.loftOptions);
    if (!built.ok()) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    ModelingService::Result lofted = ModelingService::loftSurface(built.input);
    if (!lofted.ok() || lofted.model == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    std::shared_ptr<XQSurfaceModel> model = lofted.model;
    if (intent.capEnds) {
        ModelingService::Result capped =
            ModelingService::capModel(*lofted.model, intent.capOptions);
        if (!capped.ok() || capped.model == nullptr) {
            prepared.status = Status::Rejected;
            return prepared;
        }
        model = capped.model;
    }

    ModelingService::CommandResult cmd = ModelingService::createModelNodeCommand(
        scene_, intent.newModelId, intent.name, model);
    if (!cmd.ok() || cmd.command == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    prepared.status = Status::Ok;
    prepared.command = std::move(cmd.command);
    return prepared;
}

ModelingController::Status ModelingController::loft(const LoftIntent& intent)
{
    PreparedCommand prepared = prepareLoft(intent);
    if (!prepared.ok()) {
        return prepared.status;
    }
    return stack_->push(std::move(prepared.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
