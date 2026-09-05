#include "services/flow/FlowSolver1D.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResult.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQScene.h"
#include "core/command/XQSceneCommands.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace xq {
namespace {

const double kPi = 3.14159265358979323846;

// CFL safety factor for the explicit Lax-Friedrichs march.
const double kCflLimit = 0.9;

FlowSolver1D::Result solverFailure(FlowSolver1D::Status status)
{
    FlowSolver1D::Result result;
    result.status = status;
    return result;
}

// Linear interpolation of a periodic (t, Q) waveform at time t. The waveform is
// assumed sorted by t over [0, period); t is wrapped into one period.
double sampleWaveform(const std::vector<std::pair<double, double>>& waveform,
                      double period,
                      double t)
{
    if (waveform.empty()) {
        return 0.0;
    }
    if (waveform.size() == 1) {
        return waveform[0].second;
    }
    double tw = t;
    if (period > 0.0) {
        tw = t - std::floor(t / period) * period;
    }
    // Below the first / above the last sample: clamp to the endpoints (the
    // waveform spans one period so this only nips the seam).
    if (tw <= waveform.front().first) {
        return waveform.front().second;
    }
    if (tw >= waveform.back().first) {
        return waveform.back().second;
    }
    for (std::size_t i = 1; i < waveform.size(); ++i) {
        if (tw <= waveform[i].first) {
            const double t0 = waveform[i - 1].first;
            const double t1 = waveform[i].first;
            const double q0 = waveform[i - 1].second;
            const double q1 = waveform[i].second;
            const double span = t1 - t0;
            if (span <= 0.0) {
                return q1;
            }
            const double frac = (tw - t0) / span;
            return q0 + frac * (q1 - q0);
        }
    }
    return waveform.back().second;
}

} // namespace

bool FlowSolver1D::solveSteadyPoiseuille(const std::vector<double>& arcLength,
                                         const std::vector<double>& area0,
                                         double viscosity,
                                         double flowRate,
                                         double outletPressure,
                                         std::vector<double>* pressureOut)
{
    const std::size_t n = arcLength.size();
    if (n < 2 || area0.size() != n || pressureOut == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (area0[i] <= 0.0) {
            return false;
        }
    }

    // Poiseuille resistance per unit length for a circular cross-section:
    //   dP/dx = -(8*pi*mu / A^2) * Q
    // Integrate upstream from the outlet (last station, pressure = outletPressure)
    // using the trapezoidal rule on the resistance density r(x) = 8*pi*mu/A(x)^2.
    std::vector<double> pressure(n, 0.0);
    pressure[n - 1] = outletPressure;
    for (std::size_t k = n - 1; k > 0; --k) {
        const std::size_t i = k - 1; // upstream node
        const double dx = arcLength[k] - arcLength[i];
        const double rUp = (8.0 * kPi * viscosity) / (area0[i] * area0[i]);
        const double rDown = (8.0 * kPi * viscosity) / (area0[k] * area0[k]);
        // P increases upstream: P[i] = P[k] + Q * average_resistance * dx.
        pressure[i] = pressure[k] + flowRate * 0.5 * (rUp + rDown) * dx;
    }

    *pressureOut = pressure;
    return true;
}

bool FlowSolver1D::solveRcr0D(const std::vector<double>& rcr,
                              double flowRate,
                              double initialPressure,
                              const std::vector<double>& times,
                              std::vector<double>* pressureOut)
{
    if (rcr.size() != 3 || pressureOut == nullptr) {
        return false;
    }
    const double Rp = rcr[0];
    const double C = rcr[1];
    const double Rd = rcr[2];
    if (Rp <= 0.0 || C <= 0.0 || Rd <= 0.0) {
        return false;
    }
    if (times.empty()) {
        *pressureOut = std::vector<double>();
        return true;
    }

    // Three-element windkessel. Proximal (capacitor) pressure Pc obeys
    //   C dPc/dt = Q - Pc/Rd
    // and the outlet pressure is P = Q*Rp + Pc. The initial outlet pressure is
    // initialPressure, so Pc(0) = initialPressure - Q*Rp. Backward Euler:
    //   Pc^{n+1} = (Pc^n + dt/C * Q) / (1 + dt/(Rd*C))
    std::vector<double> pressure;
    pressure.reserve(times.size());

    double pc = initialPressure - flowRate * Rp;
    double prevTime = times[0];
    // First sample reports the initial state.
    pressure.push_back(flowRate * Rp + pc);
    for (std::size_t i = 1; i < times.size(); ++i) {
        const double dt = times[i] - prevTime;
        prevTime = times[i];
        if (dt <= 0.0) {
            pressure.push_back(flowRate * Rp + pc);
            continue;
        }
        const double denom = 1.0 + dt / (Rd * C);
        pc = (pc + (dt / C) * flowRate) / denom;
        pressure.push_back(flowRate * Rp + pc);
    }

    *pressureOut = pressure;
    return true;
}

FlowSolver1D::Result FlowSolver1D::solve(const SolverInput& input)
{
    const std::size_t n = input.arcLength.size();
    if (n < 2 || input.area0.size() != n) {
        return solverFailure(Status::EmptyCenterline);
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (input.area0[i] <= 0.0) {
            return solverFailure(Status::InvalidArea);
        }
    }
    if (input.inletWaveform.empty() || input.period <= 0.0) {
        return solverFailure(Status::NoInlet);
    }
    if (input.rcr.size() != 3
        || input.rcr[0] <= 0.0 || input.rcr[1] <= 0.0 || input.rcr[2] <= 0.0) {
        return solverFailure(Status::NoOutlet);
    }
    if (input.numTimeSteps < 1 || input.dt <= 0.0) {
        return solverFailure(Status::EmptyCenterline);
    }

    const double rho = input.fluid.density;
    const double mu = input.fluid.viscosity;
    const double Rp = input.rcr[0];
    const double Cwk = input.rcr[1];
    const double Rd = input.rcr[2];

    // Uniform grid spacing from the (possibly non-uniform) stations: use the mean
    // spacing for the CFL bound and the explicit stencil. dx > 0 is guaranteed by
    // a monotonic centerline.
    const double length = input.arcLength[n - 1] - input.arcLength[0];
    if (length <= 0.0) {
        return solverFailure(Status::EmptyCenterline);
    }
    const double dx = length / static_cast<double>(n - 1);

    // Elastic-wall constitutive law gives a finite wave speed so the explicit
    // march is stable (a truly rigid wall has c -> infinity and cannot be marched
    // explicitly). P(A) = beta * (sqrt(A) - sqrt(A0)) / A0; the Moens-Korteweg
    // wave speed is c = sqrt(beta / (2 rho sqrt(A0)) * sqrt(A)) ~ sqrt(beta /
    // (2 rho sqrt(A0))). We pick beta so c is in a physiological range; this is
    // the propagation layer -- the analytic acceptance gates (steady Poiseuille,
    // 0D RCR) are checked through the dedicated reduced paths above.
    const double beta = 1.0e6; // dyn/cm^2 scale, yields c ~ O(100 cm/s)

    // Friction coefficient K_R = 8*pi*mu/rho (Poiseuille).
    const double Kr = 8.0 * kPi * mu / rho;

    std::vector<double> A(input.area0);
    std::vector<double> Q(n, 0.0);

    auto pressureAt = [&](std::size_t i, double area) {
        const double a0 = input.area0[i];
        return beta * (std::sqrt(area) - std::sqrt(a0)) / a0;
    };
    auto waveSpeed = [&](std::size_t i, double area) {
        const double a0 = input.area0[i];
        // c = sqrt( (beta / (2 rho)) * sqrt(area) / a0 )
        const double inner = (beta / (2.0 * rho)) * std::sqrt(area) / a0;
        return std::sqrt(inner > 0.0 ? inner : 0.0);
    };

    // Up-front CFL check using the initial state (max |u| + c over the grid).
    double maxSpeed = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double u = Q[i] / A[i];
        const double c = waveSpeed(i, A[i]);
        const double s = std::fabs(u) + c;
        if (s > maxSpeed) {
            maxSpeed = s;
        }
    }
    // Estimate the peak inlet speed too (cold start has Q=0 everywhere).
    double peakQ = 0.0;
    for (std::size_t i = 0; i < input.inletWaveform.size(); ++i) {
        const double q = std::fabs(input.inletWaveform[i].second);
        if (q > peakQ) {
            peakQ = q;
        }
    }
    const double peakU = peakQ / input.area0[0];
    const double cInlet = waveSpeed(0, input.area0[0]);
    const double estMaxSpeed = (peakU + cInlet) > maxSpeed ? (peakU + cInlet) : maxSpeed;

    const double cflDt = kCflLimit * dx / (estMaxSpeed > 0.0 ? estMaxSpeed : 1.0);
    const double cflNumber = (estMaxSpeed * input.dt) / dx;
    if (input.dt > cflDt) {
        FlowSolver1D::Result result = solverFailure(Status::CflViolation);
        result.flow.setMaxCfl(cflNumber);
        return result;
    }

    const int totalSteps = input.numTimeSteps * (input.numCycles > 0 ? input.numCycles : 1);
    const int recordStart = input.numTimeSteps * ((input.numCycles > 0 ? input.numCycles : 1) - 1);

    // Frame-recording stride for the last cycle (see SolverInput::maxRecordedFrames).
    int recordStride = 1;
    if (input.maxRecordedFrames > 0 && input.numTimeSteps > input.maxRecordedFrames) {
        recordStride =
            (input.numTimeSteps + input.maxRecordedFrames - 1) / input.maxRecordedFrames;
    }

    // RCR proximal (capacitor) pressure state for the outlet coupling.
    double pc = 0.0;

    double observedMaxCfl = 0.0;

    // Recorded last-cycle series: [segment][time]. One segment per cell interval.
    const std::size_t segmentCount = n - 1;
    std::vector<double> recTimes;
    std::vector<std::vector<double>> recQ(segmentCount);
    std::vector<std::vector<double>> recP(segmentCount);
    std::vector<std::vector<double>> recA(segmentCount);

    std::vector<double> An(n, 0.0);
    std::vector<double> Qn(n, 0.0);

    for (int step = 0; step < totalSteps; ++step) {
        const double t = static_cast<double>(step) * input.dt;
        const double qIn = sampleWaveform(input.inletWaveform, input.period, t);

        // Interior update via Lax-Friedrichs on the conservative (A, Q) system.
        //   F_A = Q
        //   F_Q = Q^2/A + (beta/(3 rho A0)) A^{3/2}   (pressure flux from P(A))
        // momentum source: -Kr Q / A.
        auto fluxQ = [&](std::size_t i, double area, double flow) {
            const double a0 = input.area0[i];
            const double conv = (area > 0.0) ? (flow * flow / area) : 0.0;
            const double press = (beta / (3.0 * rho * a0)) * std::pow(area, 1.5);
            return conv + press;
        };

        for (std::size_t i = 1; i + 1 < n; ++i) {
            const double AL = A[i - 1];
            const double AR = A[i + 1];
            const double QL = Q[i - 1];
            const double QR = Q[i + 1];

            const double fA_L = QL;
            const double fA_R = QR;
            const double fQ_L = fluxQ(i - 1, AL, QL);
            const double fQ_R = fluxQ(i + 1, AR, QR);

            const double srcQ = (A[i] > 0.0) ? (-Kr * Q[i] / A[i]) : 0.0;

            An[i] = 0.5 * (AL + AR) - (input.dt / (2.0 * dx)) * (fA_R - fA_L);
            Qn[i] = 0.5 * (QL + QR) - (input.dt / (2.0 * dx)) * (fQ_R - fQ_L)
                + input.dt * srcQ;
            if (An[i] <= 0.0) {
                An[i] = input.area0[i] * 1.0e-3; // floor to keep area positive
            }
        }

        // Inlet (i=0): prescribe Q, extrapolate A from the interior.
        Qn[0] = qIn;
        An[0] = (n >= 2) ? An[1] : A[0];
        if (An[0] <= 0.0) {
            An[0] = input.area0[0] * 1.0e-3;
        }

        // Outlet (i=n-1): 0D RCR coupling. Outflow Q_out is taken from the
        // upstream interior; update the windkessel capacitor pressure with
        // backward Euler, then set the outlet pressure -> area through P(A).
        const double qOut = Qn[n - 2];
        const double denom = 1.0 + input.dt / (Rd * Cwk);
        pc = (pc + (input.dt / Cwk) * qOut) / denom;
        const double pOut = qOut * Rp + pc;
        // Invert P(A) = beta (sqrt(A) - sqrt(A0)) / A0 for the outlet area.
        const double a0Out = input.area0[n - 1];
        const double sqrtA = std::sqrt(a0Out) + pOut * a0Out / beta;
        An[n - 1] = (sqrtA > 0.0) ? (sqrtA * sqrtA) : (a0Out * 1.0e-3);
        Qn[n - 1] = qOut;

        A.swap(An);
        Q.swap(Qn);

        // Track the realized CFL number.
        for (std::size_t i = 0; i < n; ++i) {
            const double u = Q[i] / A[i];
            const double c = waveSpeed(i, A[i]);
            const double cfl = (std::fabs(u) + c) * input.dt / dx;
            if (cfl > observedMaxCfl) {
                observedMaxCfl = cfl;
            }
        }

        // Record the last cycle (decimated by recordStride; final step always kept).
        if (step >= recordStart
            && (((step - recordStart) % recordStride) == 0 || step == totalSteps - 1)) {
            recTimes.push_back(t - static_cast<double>(recordStart) * input.dt);
            for (std::size_t s = 0; s < segmentCount; ++s) {
                const double aSeg = 0.5 * (A[s] + A[s + 1]);
                const double qSeg = 0.5 * (Q[s] + Q[s + 1]);
                const double pSeg = 0.5 * (pressureAt(s, A[s]) + pressureAt(s + 1, A[s + 1]));
                recA[s].push_back(aSeg);
                recQ[s].push_back(qSeg);
                recP[s].push_back(pSeg);
            }
        }
    }

    XQFlowResult flow;
    flow.setTimes(recTimes);
    for (std::size_t s = 0; s < segmentCount; ++s) {
        FlowSegment seg;
        seg.segmentId = static_cast<int>(s);
        seg.arcLengthStart = input.arcLength[s];
        seg.arcLengthEnd = input.arcLength[s + 1];
        seg.faceId = 0;
        flow.addSegment(seg);
    }
    flow.setSeries(recQ, recP, recA);
    flow.setMaxCfl(observedMaxCfl);
    // Convergence: finite state everywhere (no NaN/Inf) at the end of the run.
    bool finite = true;
    for (std::size_t i = 0; i < n && finite; ++i) {
        if (!std::isfinite(A[i]) || !std::isfinite(Q[i])) {
            finite = false;
        }
    }
    flow.setConverged(finite);

    FlowSolver1D::Result result;
    result.status = Status::Ok;
    result.flow = flow;
    return result;
}

FlowSolver1D::CommandResult FlowSolver1D::buildFlowResultCommand(
    XQScene* scene,
    const NodeId& newResultId,
    const std::string& name,
    const XQFlowResult& result,
    const NodeId& caseNodeId)
{
    CommandResult commandResult;
    if (scene == nullptr) {
        commandResult.status = Status::NullScene;
        commandResult.command = nullptr;
        return commandResult;
    }

    XQFlowResult stored = result;
    if (caseNodeId.is_valid()) {
        stored.setSourceCaseNode(caseNodeId);
    }
    auto payload = std::make_shared<XQFlowResultPayload>(std::move(stored));
    const XQDataNode node(newResultId, XQDomainType::FlowResult, name, payload);

    commandResult.status = Status::Ok;
    if (caseNodeId.is_valid()) {
        commandResult.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, caseNodeId, "Add flow result"));
    } else {
        commandResult.command.reset(new AddNodeCommand(scene, node, "Add flow result"));
    }
    return commandResult;
}

} // namespace xq
