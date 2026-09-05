#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQScene.h>
#include <core/XQSimulationCase.h>
#include <services/flow/FlowSolver1D.h>
#include <services/ai/FlowMetricsService.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

double relError(double numeric, double analytic)
{
    const double denom = std::fabs(analytic) > 1e-30 ? std::fabs(analytic) : 1.0;
    return std::fabs(numeric - analytic) / denom;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Side-effecting
// calls are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

const double kPi = 3.14159265358979323846;

int main()
{
    using Solver = xq::FlowSolver1D;

    // ===================================================================
    // ANALYTIC GATE 1: steady Poiseuille pressure drop in a rigid tube.
    //   constant Q0, constant area A, length L:
    //   dP = 8*pi*mu*L*Q0 / A^2   (== 128*mu*L*Q0 / (pi*D^4))
    // ===================================================================
    {
        const double mu = 0.04;     // poise (CGS, blood)
        const double Q0 = 5.0;      // cm^3/s
        const double L = 10.0;      // cm
        const double A = 0.5;       // cm^2 (constant)
        const std::size_t n = 21;

        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, A);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = L * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        std::vector<double> pressure;
        const bool ok = Solver::solveSteadyPoiseuille(x, area, mu, Q0, 0.0, &pressure);
        CHECK(ok);
        CHECK(pressure.size() == n);
        CHECK(pressure.back() == 0.0); // outlet pressure prescribed

        const double analyticDrop = 8.0 * kPi * mu * L * Q0 / (A * A);
        const double numericDrop = pressure.front() - pressure.back();
        const double err = relError(numericDrop, analyticDrop);
        std::printf("Poiseuille (const A): analytic dP=%.6f numeric dP=%.6f relErr=%.3e\n",
                    analyticDrop, numericDrop, err);
        // Constant area: trapezoidal integration of a constant is exact.
        CHECK(err < 1e-9);
    }

    // Poiseuille, linearly varying area: compare against the closed-form
    // integral dP = 8*pi*mu*Q0 * integral_0^L dx / A(x)^2 with A(x) linear from
    // A0 to A1 -> integral = L/(A0*A1).
    {
        const double mu = 0.04;
        const double Q0 = 5.0;
        const double L = 10.0;
        const double A0 = 0.6;
        const double A1 = 0.3;
        const std::size_t n = 201;

        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double f = static_cast<double>(i) / static_cast<double>(n - 1);
            x[i] = L * f;
            area[i] = A0 + (A1 - A0) * f;
        }

        std::vector<double> pressure;
        const bool ok = Solver::solveSteadyPoiseuille(x, area, mu, Q0, 0.0, &pressure);
        CHECK(ok);

        const double analyticDrop = 8.0 * kPi * mu * Q0 * (L / (A0 * A1));
        const double numericDrop = pressure.front() - pressure.back();
        const double err = relError(numericDrop, analyticDrop);
        std::printf("Poiseuille (linear A): analytic dP=%.6f numeric dP=%.6f relErr=%.3e\n",
                    analyticDrop, numericDrop, err);
        CHECK(err < 1e-2); // trapezoidal approximation of 1/A^2
    }

    // ===================================================================
    // ANALYTIC GATE 2: single 0D RCR step response.
    //   constant Q0 into [Rp, C, Rd] from initial P0:
    //   P(t) = Q0(Rp+Rd) + (P0 - Q0(Rp+Rd)) exp(-t / (Rd*C))
    // ===================================================================
    {
        const double Rp = 106.0;
        const double C = 0.00068483;
        const double Rd = 1784.0;
        const std::vector<double> rcr = {Rp, C, Rd};
        const double Q0 = 5.0;
        const double P0 = 0.0;

        const double tau = Rd * C;
        const double dt = tau / 500.0; // small step for backward-Euler accuracy
        const std::size_t steps = 5500; // run past ~11 tau so the system reaches steady state

        std::vector<double> times(steps, 0.0);
        for (std::size_t i = 0; i < steps; ++i) {
            times[i] = dt * static_cast<double>(i);
        }

        std::vector<double> pressure;
        const bool ok = Solver::solveRcr0D(rcr, Q0, P0, times, &pressure);
        CHECK(ok);
        CHECK(pressure.size() == steps);

        const double pInf = Q0 * (Rp + Rd);
        double maxErr = 0.0;
        for (std::size_t i = 0; i < steps; ++i) {
            const double analytic = pInf + (P0 - pInf) * std::exp(-times[i] / tau);
            const double err = relError(pressure[i], analytic);
            if (err > maxErr) {
                maxErr = err;
            }
        }
        // Steady-state value must match Q0(Rp+Rd) tightly.
        const double ssErr = relError(pressure.back(), pInf);
        std::printf("RCR step: pInf=%.4f numeric_final=%.4f ssRelErr=%.3e maxRelErr=%.3e\n",
                    pInf, pressure.back(), ssErr, maxErr);
        CHECK(ssErr < 1e-3);
        CHECK(maxErr < 1e-2); // pointwise backward-Euler vs analytic exponential
    }

    // ===================================================================
    // CFL violation must be diagnosed, not silently diverge.
    // ===================================================================
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 5.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 5.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 100;
        in.dt = 1.0; // absurdly large -> CFL violation
        in.numCycles = 1;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.status == Solver::Status::CflViolation);
    }

    // ===================================================================
    // Transient solve produces a dimensionally consistent result.
    // ===================================================================
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 3.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 8.0));
        in.inletWaveform.push_back(std::make_pair(1.0, 3.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 200;
        in.dt = 1.0e-4;
        in.numCycles = 2;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.ok());
        const xq::XQFlowResult& flow = result.flow;
        CHECK(flow.isConsistent());
        CHECK(flow.segments().size() == n - 1);
        CHECK(flow.times().size() == static_cast<std::size_t>(in.numTimeSteps));
        CHECK(flow.flowQ().size() == n - 1);
        CHECK(flow.converged());
    }

    // ===================================================================
    // EmptyCenterline / InvalidArea diagnostics.
    // ===================================================================
    {
        Solver::SolverInput in;
        in.arcLength = {0.0};
        in.area0 = {0.5};
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 5.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 10;
        in.dt = 1.0e-4;
        const Solver::Result result = Solver::solve(in);
        CHECK(result.status == Solver::Status::EmptyCenterline);
    }
    {
        Solver::SolverInput in;
        in.arcLength = {0.0, 1.0, 2.0};
        in.area0 = {0.5, -0.1, 0.5}; // negative area
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 5.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 10;
        in.dt = 1.0e-4;
        const Solver::Result result = Solver::solve(in);
        CHECK(result.status == Solver::Status::InvalidArea);
    }

    // ===================================================================
    // FlowResult payload deep-copy (clone owns its own series).
    // ===================================================================
    {
        xq::XQFlowResult flow;
        flow.setTimes({0.0, 0.1});
        xq::FlowSegment seg;
        seg.segmentId = 0;
        flow.addSegment(seg);
        flow.setSeries({{1.0, 2.0}}, {{10.0, 20.0}}, {{0.5, 0.5}});

        xq::XQFlowResultPayload payload(flow);
        const auto cloned = payload.clone();
        CHECK(cloned != nullptr);
        CHECK(cloned->domainType() == xq::XQDomainType::FlowResult);
        const auto* clonedFlow = dynamic_cast<const xq::XQFlowResultPayload*>(cloned.get());
        CHECK(clonedFlow != nullptr);
        CHECK(clonedFlow->result().flowQ().size() == 1);
        CHECK(clonedFlow->result().flowQ()[0][1] == 2.0);

        // Mutate the original; the clone must not change.
        payload.result().setSeries({{99.0, 99.0}}, {{0.0, 0.0}}, {{0.0, 0.0}});
        CHECK(clonedFlow->result().flowQ()[0][0] == 1.0);
    }

    // ===================================================================
    // buildFlowResultCommand: into scene + undo, derived from case node.
    // ===================================================================
    {
        xq::XQScene scene;
        const xq::NodeId caseNode(6101);
        const xq::NodeId resultNode(6102);

        CHECK(scene.insert(xq::XQDataNode(caseNode, "simulation_case", "Case"))
              == xq::XQScene::InsertResult::Inserted);

        xq::XQFlowResult flow;
        flow.setTimes({0.0});
        xq::FlowSegment seg;
        flow.addSegment(seg);
        flow.setSeries({{1.0}}, {{2.0}}, {{0.5}});

        Solver::CommandResult cmd =
            Solver::buildFlowResultCommand(&scene, resultNode, "Flow", flow, caseNode);
        CHECK(cmd.ok());
        CHECK(cmd.command != nullptr);

        cmd.command->execute();
        const xq::XQDataNode* inserted = scene.find(resultNode);
        CHECK(inserted != nullptr);
        CHECK(inserted->domainType() == xq::XQDomainType::FlowResult);
        const auto* fp =
            dynamic_cast<const xq::XQFlowResultPayload*>(inserted->payload().get());
        CHECK(fp != nullptr);
        CHECK(fp->result().hasSourceCaseNode());
        CHECK(fp->result().sourceCaseNode() == caseNode);

        cmd.command->undo();
        CHECK(scene.find(resultNode) == nullptr);
    }

    // ---- null scene on the command path ----
    {
        xq::XQFlowResult flow;
        Solver::CommandResult cmd =
            Solver::buildFlowResultCommand(nullptr, xq::NodeId(1), "Flow", flow, xq::NodeId(2));
        CHECK(cmd.status == Solver::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }

    // ===================================================================
    // maxRecordedFrames decimates the recorded last cycle: frame count is
    // bounded, the series still starts at 0 and ends at the cycle end, and
    // the decimated result stays consistent and analyzable.
    // ===================================================================
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 3.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 8.0));
        in.inletWaveform.push_back(std::make_pair(1.0, 3.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 10000;
        in.dt = 1.0e-4; // 10000 * 1e-4 = one full period
        in.numCycles = 1;
        in.maxRecordedFrames = 100;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.ok());
        const xq::XQFlowResult& flow = result.flow;
        CHECK(flow.isConsistent());
        CHECK(flow.times().size() >= 100);
        CHECK(flow.times().size() <= 101);
        CHECK(flow.times().front() == 0.0);
        CHECK(flow.times().back() >= in.period - 2.0 * in.dt);
        CHECK(flow.converged());

        // Decimated frames must still feed the metrics stage.
        xq::FlowMetricsService::Request request;
        request.referencePressure = 1.0e5; // absolute baseline: FFR needs Pa > 0
        const xq::FlowMetricsService::Result metrics =
            xq::FlowMetricsService::analyzeFlow(flow, request);
        CHECK(metrics.ok());
    }

    // maxRecordedFrames = 0 (the default) records every step: the historical
    // behavior, asserted explicitly so the default can never silently change.
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 3.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 8.0));
        in.inletWaveform.push_back(std::make_pair(1.0, 3.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 200;
        in.dt = 1.0e-4;
        in.numCycles = 2;
        in.maxRecordedFrames = 0;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.ok());
        CHECK(result.flow.times().size() == static_cast<std::size_t>(in.numTimeSteps));
    }

    std::printf("OK: FlowSolver1D analytic gates + transient + command/undo\n");
    return 0;
}
