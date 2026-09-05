#ifndef XQ_UI_CONTROLLERS_FLOW_CONTROLLER_H
#define XQ_UI_CONTROLLERS_FLOW_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQMesh.h"
#include "core/XQSimulationCase.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"
#include "services/flow/FlowSolver1D.h"

#include <memory>
#include <string>

namespace xq {

class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the flow stage. It validates the simulation
// case's boundary conditions against the mesh (BoundaryConditionService), runs
// the reduced-order solver (FlowSolver1D), and commits the flow-result-insertion
// command through the command stack. It implements no solver numerics itself and
// never mutates the scene directly.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class FlowController {
public:
    FlowController(XQScene* scene, XQCommandStack* stack);

    enum class Status {
        Ok,
        InvalidBoundaryConditions, // BoundaryConditionService rejected the case
        SolveFailed,               // the solver rejected the input / diverged
        Rejected,                  // command construction failed (no scene change)
        NullScene,
    };

    // A computed-but-not-committed command: prepare*() runs the service work
    // (safe on a worker thread -- pure computation over the copied intent) and
    // the caller pushes the command on the GUI thread.
    struct PreparedCommand {
        Status status = Status::Rejected;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const { return status == Status::Ok && command != nullptr; }
    };

    // Pushes a prepared command on the stack (GUI-thread half of prepare*).
    bool commitPrepared(std::unique_ptr<XQCommand> command)
    {
        return stack_ != nullptr && command != nullptr
            && stack_->push(std::move(command));
    }

    struct SolveIntent {
        NodeId newResultId;
        std::string name;
        NodeId caseNode;                  // simulation-case node the result derives from
        XQSimulationCase simulationCase;  // boundary conditions to validate
        XQMesh mesh;                      // authoritative boundary-face list for validation
        FlowSolver1D::SolverInput solverInput; // centerline + waveform + RCR + time plan
    };

    // Validates the BCs, solves, then pushes the flow-result command. Returns a
    // diagnostic status (and leaves the scene untouched) on any failure.
    PreparedCommand prepareSolve(const SolveIntent& intent);
    Status solve(const SolveIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_FLOW_CONTROLLER_H
