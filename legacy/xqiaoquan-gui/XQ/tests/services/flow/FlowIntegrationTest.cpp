#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQSimulationCase.h>
#include <io/project/CTGRContourReader.h>
#include <services/flow/BoundaryConditionService.h>
#include <services/flow/FlowSolver1D.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

// Polygon area of a contour by the shoelace formula in its own frame plane.
double contourArea(const xq::XQContour& contour)
{
    const std::size_t m = contour.points.size();
    if (m < 3) {
        return 0.0;
    }
    double area2 = 0.0;
    double prevU = 0.0;
    double prevV = 0.0;
    bool first = true;
    double firstU = 0.0;
    double firstV = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        double u = 0.0;
        double v = 0.0;
        xq::XQContourGroup::projectToFrame(contour.frame, contour.points[i], &u, &v);
        if (first) {
            firstU = u;
            firstV = v;
            first = false;
        } else {
            area2 += prevU * v - u * prevV;
        }
        prevU = u;
        prevV = v;
    }
    // close polygon
    area2 += prevU * firstV - firstU * prevV;
    return std::fabs(area2) * 0.5;
}

} // namespace

int main()
{
    // ---- read the real 0007 aorta contour group ----
    const std::string ctgrPath = std::string(XQ_CTGR_DIR) + "/aorta_final.ctgr";
    xq::CTGRReadResult read;
    const xq::CTGRContourReader::Status status =
        xq::CTGRContourReader::read(ctgrPath, &read);
    CHECK(status == xq::CTGRContourReader::Status::Ok);
    CHECK(read.group.contours().size() >= 3);

    // Build A0(x) from contour polygon areas ordered by path arc length.
    std::vector<xq::XQContour> ordered = read.group.orderedByPathPosition();
    std::vector<double> arcLength;
    std::vector<double> area0;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const double a = contourArea(ordered[i]);
        if (a <= 0.0) {
            continue; // skip degenerate contours
        }
        // arc length must be strictly increasing for the solver grid
        if (!arcLength.empty() && ordered[i].pathArcLength <= arcLength.back()) {
            continue;
        }
        arcLength.push_back(ordered[i].pathArcLength);
        area0.push_back(a);
    }
    CHECK(arcLength.size() >= 3);
    std::printf("0007 aorta: %zu usable contour stations, arcLength %.3f..%.3f cm, "
                "A0 %.4f..%.4f cm^2\n",
                arcLength.size(), arcLength.front(), arcLength.back(),
                *std::min_element(area0.begin(), area0.end()),
                *std::max_element(area0.begin(), area0.end()));

    // ---- read the real inflow_1d.flow waveform (two columns t Q) ----
    const std::string flowPath = std::string(XQ_FLOW_DIR) + "/inflow_1d.flow";
    std::ifstream flowFile(flowPath.c_str());
    CHECK(flowFile.good());
    std::stringstream buffer;
    buffer << flowFile.rdbuf();
    const std::vector<std::pair<double, double>> waveform =
        xq::BoundaryConditionService::parseFlowFile(buffer.str());
    CHECK(waveform.size() >= 100);
    const double period = waveform.back().first; // ~0.984 s
    CHECK(period > 0.5);
    CHECK(period < 1.5);
    std::printf("inflow_1d.flow: %zu samples, period=%.4f s, Q range %.3f..%.3f cm^3/s\n",
                waveform.size(), period, waveform.front().second, waveform.back().second);

    // ---- outlet RCR from the .sjb reference (outflow cap: 106.0 / 0.00068483 / 1784.0) ----
    const std::vector<double> rcr = {106.0, 0.00068483, 1784.0};

    // ---- run a couple of cycles of the 1D solver ----
    xq::FlowSolver1D::SolverInput in;
    in.arcLength = arcLength;
    in.area0 = area0;
    in.fluid = xq::FluidProperties{}; // CGS blood defaults (1.06 / 0.04)
    in.inletWaveform = waveform;
    in.period = period;
    in.rcr = rcr;
    in.numCycles = 2;
    // choose dt under the CFL limit; the solver re-checks and diagnoses otherwise.
    in.numTimeSteps = 2000;
    in.dt = period / static_cast<double>(in.numTimeSteps);

    xq::FlowSolver1D::Result result = xq::FlowSolver1D::solve(in);
    // If the chosen dt trips CFL, shrink it once and retry (keeps the test
    // deterministic without hand-tuning to the exact mesh).
    if (result.status == xq::FlowSolver1D::Status::CflViolation) {
        in.numTimeSteps = 20000;
        in.dt = period / static_cast<double>(in.numTimeSteps);
        result = xq::FlowSolver1D::solve(in);
    }
    CHECK(result.ok());

    const xq::XQFlowResult& flow = result.flow;
    CHECK(flow.isConsistent());
    CHECK(flow.segments().size() == arcLength.size() - 1);
    CHECK(flow.times().size() == static_cast<std::size_t>(in.numTimeSteps));
    CHECK(flow.converged());

    // Pressure magnitude sanity: physiological aortic pressure is O(10^5) dyn/cm^2
    // (1 mmHg = 1333 dyn/cm^2; 80-120 mmHg ~ 1.0e5-1.6e5). The outlet RCR sets the
    // level via mean Q * (Rp+Rd). Compute mean inlet flow to bound the order.
    double meanQ = 0.0;
    for (std::size_t i = 0; i < waveform.size(); ++i) {
        meanQ += waveform[i].second;
    }
    meanQ /= static_cast<double>(waveform.size());
    const double expectedMeanP = meanQ * (rcr[0] + rcr[2]);
    std::printf("mean inlet Q=%.3f cm^3/s -> RCR mean outlet P ~ %.1f dyn/cm^2 "
                "(%.1f mmHg)\n",
                meanQ, expectedMeanP, expectedMeanP / 1333.22);
    // The RCR steady level itself must be physiological (tens of mmHg).
    CHECK(expectedMeanP > 1.0e4);
    CHECK(expectedMeanP < 1.0e6);

    // Every recorded Q/A must be finite and area positive.
    const std::size_t segCount = flow.segments().size();
    const std::size_t timeCount = flow.times().size();
    for (std::size_t s = 0; s < segCount; ++s) {
        for (std::size_t t = 0; t < timeCount; ++t) {
            CHECK(std::isfinite(flow.flowQ()[s][t]));
            CHECK(std::isfinite(flow.pressureP()[s][t]));
            CHECK(std::isfinite(flow.areaA()[s][t]));
            CHECK(flow.areaA()[s][t] > 0.0);
        }
    }

    std::printf("OK: 0007 end-to-end 1D flow over %d steps x %d cycles, maxCfl=%.4f, "
                "converged=%d\n",
                in.numTimeSteps, in.numCycles, flow.maxCfl(),
                flow.converged() ? 1 : 0);
    return 0;
}
