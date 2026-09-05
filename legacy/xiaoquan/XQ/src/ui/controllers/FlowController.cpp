#include "ui/controllers/FlowController.h"

#include "core/command/XQCommandStack.h"
#include "services/flow/BoundaryConditionService.h"

#include <utility>

namespace xq {

FlowController::FlowController(XQScene* scene, XQCommandStack* stack)
    : scene_(scene)
    , stack_(stack)
{
}

FlowController::Status FlowController::solve(const SolveIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }

    // Validate / bind the boundary conditions against the mesh first.
    BoundaryConditionService::Result bound =
        BoundaryConditionService::validateAndBind(intent.simulationCase, intent.mesh);
    if (!bound.ok()) {
        return Status::InvalidBoundaryConditions;
    }

    // Run the reduced-order solver.
    FlowSolver1D::Result solved = FlowSolver1D::solve(intent.solverInput);
    if (!solved.ok()) {
        return Status::SolveFailed;
    }

    // Commit the flow-result node, derived from the simulation-case node.
    FlowSolver1D::CommandResult cmd = FlowSolver1D::buildFlowResultCommand(
        scene_, intent.newResultId, intent.name, solved.flow, intent.caseNode);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
