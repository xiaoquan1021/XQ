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

FlowController::PreparedCommand FlowController::prepareSolve(const SolveIntent& intent)
{
    PreparedCommand prepared;
    if (scene_ == nullptr || stack_ == nullptr) {
        prepared.status = Status::NullScene;
        return prepared;
    }

    // Validate / bind the boundary conditions against the mesh first.
    BoundaryConditionService::Result bound =
        BoundaryConditionService::validateAndBind(intent.simulationCase, intent.mesh);
    if (!bound.ok()) {
        prepared.status = Status::InvalidBoundaryConditions;
        return prepared;
    }

    // Run the reduced-order solver.
    FlowSolver1D::Result solved = FlowSolver1D::solve(intent.solverInput);
    if (!solved.ok()) {
        prepared.status = Status::SolveFailed;
        return prepared;
    }

    // Commit the flow-result node, derived from the simulation-case node.
    FlowSolver1D::CommandResult cmd = FlowSolver1D::buildFlowResultCommand(
        scene_, intent.newResultId, intent.name, solved.flow, intent.caseNode);
    if (!cmd.ok() || cmd.command == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    prepared.status = Status::Ok;
    prepared.command = std::move(cmd.command);
    return prepared;
}

FlowController::Status FlowController::solve(const SolveIntent& intent)
{
    PreparedCommand prepared = prepareSolve(intent);
    if (!prepared.ok()) {
        return prepared.status;
    }
    return stack_->push(std::move(prepared.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
