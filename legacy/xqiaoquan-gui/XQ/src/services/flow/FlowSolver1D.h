#ifndef XQ_SERVICES_FLOW_FLOW_SOLVER_1D_H
#define XQ_SERVICES_FLOW_FLOW_SOLVER_1D_H

#include "core/NodeId.h"
#include "core/XQFlowResult.h"
#include "core/XQSimulationCase.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xq {

class XQScene;

// Pure-numeric reduced-order / 1D blood-flow solver. Zero external dependencies
// (no Qt / VTK / ITK / external CFD / Python): only xq_core + standard C++.
// All quantities CGS: x [cm], A [cm^2], Q [cm^3/s], P [dyn/cm^2], density
// [g/cm^3], viscosity [poise], time [s].
//
// Governing 1D blood-flow equations (rigid wall, first version), state (A, Q)
// along the centerline x:
//   mass:     dA/dt + dQ/dx = 0
//   momentum: dQ/dt + d(Q^2/A)/dx + (A/rho) dP/dx = -K_R * Q / A
// with viscous (Poiseuille) friction coefficient K_R = 8*pi*mu/rho. The outlet
// is a 0D three-element RCR windkessel.
//
// The transient PDE path (solve) is the deepest layer; two reduced paths are
// provided for analytic verification (steady Poiseuille pressure drop, single
// 0D RCR step response) which the unit tests assert against closed-form solutions.
class FlowSolver1D {
public:
    enum class Status {
        Ok,
        EmptyCenterline, // arcLength / area0 empty or too short
        InvalidArea,     // a non-positive cross-sectional area
        CflViolation,    // chosen dt violates the CFL limit (diagnosed, not silent)
        NoInlet,         // no inlet waveform supplied
        NoOutlet,        // no outlet RCR supplied
        NullScene,       // scene pointer was null (command path only)
    };

    struct SolverInput {
        std::vector<double> arcLength;   // monotonic x stations [cm], size N
        std::vector<double> area0;       // A0(x) at each station [cm^2], size N
        FluidProperties fluid;           // density, viscosity (CGS)
        std::vector<std::pair<double, double>> inletWaveform; // (t, Q) inlet flow
        double period = 0.0;             // waveform period [s]
        std::vector<double> rcr;         // outlet [Rp, C, Rd] (CGS)
        int numTimeSteps = 0;            // steps per cycle
        double dt = 0.0;                 // time step [s]
        int numCycles = 1;               // cycles to run; last cycle is recorded
        // Maximum number of recorded last-cycle frames. 0 (or negative) keeps
        // the historical behavior of recording every step. When positive and
        // numTimeSteps exceeds it, frames are recorded every
        // ceil(numTimeSteps / maxRecordedFrames) steps, and the final step is
        // always recorded so the series still ends at the end of the cycle.
        int maxRecordedFrames = 0;
    };

    struct Result {
        Status status;
        XQFlowResult flow; // meaningful only when status == Ok

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

    // Transient 1D solve over numCycles cycles; records the last cycle into the
    // returned XQFlowResult (per-segment Q/P/A series). CFL is checked up front:
    // if dt exceeds the limit dt <= CFL * dx / (|u| + c) the solver returns
    // CflViolation rather than diverging silently.
    static Result solve(const SolverInput& input);

    // --- Analytic-verification paths (used by tests; closed-form comparable) ---

    // Steady Poiseuille pressure drop along a rigid tube carrying a constant flow
    // rate Q0. Returns the pressure P(x) at each station (P at the last station =
    // outletPressure), integrating dP/dx = -(8*pi*mu / A^2) * Q0 upstream from the
    // outlet. For a constant area A this reproduces the textbook Poiseuille drop
    // dP = 8*pi*mu*L*Q0 / A^2 (== 128*mu*L*Q0 / (pi*D^4)). Returns false on empty
    // / non-positive-area input.
    static bool solveSteadyPoiseuille(const std::vector<double>& arcLength,
                                      const std::vector<double>& area0,
                                      double viscosity,
                                      double flowRate,
                                      double outletPressure,
                                      std::vector<double>* pressureOut);

    // Single 0D RCR step response: a constant flow Q0 entering a three-element
    // windkessel [Rp, C, Rd] from initial proximal pressure P0. Fills pressureOut
    // with the outlet pressure at each instant times[i], using backward Euler on
    // C dPc/dt = Q0 - Pc/Rd with outlet pressure P = Q0*Rp + Pc. Converges to the
    // analytic P(t) = Q0(Rp+Rd) + (P0 - Q0(Rp+Rd)) exp(-t/(Rd*C)). Returns false
    // on invalid RCR (size != 3 or non-positive entries).
    static bool solveRcr0D(const std::vector<double>& rcr,
                           double flowRate,
                           double initialPressure,
                           const std::vector<double>& times,
                           std::vector<double>* pressureOut);

    // Builds the command that inserts the flow-result node, linking it as derived
    // from caseNodeId (the simulation-case node). The caller assigns newResultId.
    static CommandResult buildFlowResultCommand(XQScene* scene,
                                                const NodeId& newResultId,
                                                const std::string& name,
                                                const XQFlowResult& result,
                                                const NodeId& caseNodeId);
};

} // namespace xq

#endif // XQ_SERVICES_FLOW_FLOW_SOLVER_1D_H
