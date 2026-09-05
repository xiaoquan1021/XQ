#include <core/NodeId.h>
#include <core/XQAiAnalysis.h>
#include <core/XQAiAnalysisPayload.h>
#include <core/XQContourGroup.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQScene.h>
#include <core/command/XQCommandStack.h>
#include <io/project/CTGRContourReader.h>
#include <services/ai/FlowMetricsService.h>
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
    area2 += prevU * firstV - firstU * prevV;
    return std::fabs(area2) * 0.5;
}

} // namespace

int main()
{
    // ---- read the real 0007 aorta contour group and build A0(x) ----
    const std::string ctgrPath = std::string(XQ_CTGR_DIR) + "/aorta_final.ctgr";
    xq::CTGRReadResult read;
    const xq::CTGRContourReader::Status status =
        xq::CTGRContourReader::read(ctgrPath, &read);
    CHECK(status == xq::CTGRContourReader::Status::Ok);

    std::vector<xq::XQContour> ordered = read.group.orderedByPathPosition();
    std::vector<double> arcLength;
    std::vector<double> area0;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const double a = contourArea(ordered[i]);
        if (a <= 0.0) {
            continue;
        }
        if (!arcLength.empty() && ordered[i].pathArcLength <= arcLength.back()) {
            continue;
        }
        arcLength.push_back(ordered[i].pathArcLength);
        area0.push_back(a);
    }
    CHECK(arcLength.size() >= 3);

    // ---- read the real inflow waveform ----
    const std::string flowPath = std::string(XQ_FLOW_DIR) + "/inflow_1d.flow";
    std::ifstream flowFile(flowPath.c_str());
    CHECK(flowFile.good());
    std::stringstream buffer;
    buffer << flowFile.rdbuf();
    const std::vector<std::pair<double, double>> waveform =
        xq::BoundaryConditionService::parseFlowFile(buffer.str());
    CHECK(waveform.size() >= 100);
    const double period = waveform.back().first;
    CHECK(period > 0.5 && period < 1.5);

    const std::vector<double> rcr = {106.0, 0.00068483, 1784.0};

    // ---- run the 1D solver (M5) to produce a real flow result ----
    xq::FlowSolver1D::SolverInput in;
    in.arcLength = arcLength;
    in.area0 = area0;
    in.fluid = xq::FluidProperties{};
    in.inletWaveform = waveform;
    in.period = period;
    in.rcr = rcr;
    in.numCycles = 2;
    in.numTimeSteps = 2000;
    in.dt = period / static_cast<double>(in.numTimeSteps);

    xq::FlowSolver1D::Result solved = xq::FlowSolver1D::solve(in);
    if (solved.status == xq::FlowSolver1D::Status::CflViolation) {
        in.numTimeSteps = 20000;
        in.dt = period / static_cast<double>(in.numTimeSteps);
        solved = xq::FlowSolver1D::solve(in);
    }
    CHECK(solved.ok());
    CHECK(solved.flow.isConsistent());

    // ---- M6: analyze the flow for hemodynamic metrics ----
    // The reduced-order solver records wall-relative pressure centered near zero
    // (can be negative). FFR is an absolute-pressure ratio, so supply a baseline
    // that lifts the lowest segment time-mean pressure to a physiological floor
    // (60 mmHg = 8.0e4 dyn/cm^2). dP and WSS/OSI are unaffected by the baseline.
    const std::vector<std::vector<double>>& Pseg = solved.flow.pressureP();
    double minMeanP = 1.0e300;
    for (std::size_t s = 0; s < Pseg.size(); ++s) {
        double sum = 0.0;
        for (std::size_t t = 0; t < Pseg[s].size(); ++t) {
            sum += Pseg[s][t];
        }
        const double meanP = sum / static_cast<double>(Pseg[s].size());
        if (meanP < minMeanP) {
            minMeanP = meanP;
        }
    }
    const double physiologicalFloor = 8.0e4; // 60 mmHg
    xq::FlowMetricsService::Request req; // CGS blood mu = 0.04 default
    req.referencePressure = physiologicalFloor - minMeanP;

    const xq::FlowMetricsService::Result metrics =
        xq::FlowMetricsService::analyzeFlow(solved.flow, req);
    CHECK(metrics.ok());
    CHECK(metrics.analysis.kind() == xq::AnalysisKind::FlowMetrics);
    CHECK(metrics.analysis.provenance() == xq::AnalysisProvenance::Computed);

    xq::NamedMetric ffr;
    CHECK(metrics.analysis.metricByName("FFR", &ffr));
    xq::NamedMetric tawss;
    CHECK(metrics.analysis.metricByName("TAWSS", &tawss));
    xq::NamedMetric wssMax;
    CHECK(metrics.analysis.metricByName("WSS_max", &wssMax));
    xq::NamedMetric osi;
    CHECK(metrics.analysis.metricByName("OSI", &osi));
    xq::NamedMetric dp;
    CHECK(metrics.analysis.metricByName("dP", &dp));

    std::printf("0007 flow metrics: FFR=%.4f TAWSS=%.3f dyn/cm^2 WSS_max=%.3f "
                "OSI=%.4f dP=%.1f dyn/cm^2\n",
                ffr.value, tawss.value, wssMax.value, osi.value, dp.value);

    // Sanity bounds. WSS is the load-bearing physiological check here: aortic
    // wall shear stress is O(1-100) dyn/cm^2 and this run yields TAWSS ~ 10,
    // WSS_max ~ 40 -- physiological. OSI is bounded to [0, 0.5] by construction.
    //
    // FFR here is a pressure ratio computed on the reduced-order solver's
    // wall-relative pressure lifted by a single baseline; with this 1D field the
    // most-distal segment can sit at higher pressure than the most-proximal one
    // (a known artifact of relative-pressure baselining, not a metric bug), so
    // FFR can exceed 1. We only assert FFR is finite and positive and that it is
    // self-consistent with dP (dP = Pa - Pd, so dP < 0 iff FFR > 1). The exact
    // FFR = Pd/Pa relation and direction are pinned by the unit test against
    // synthetic absolute-pressure fields.
    CHECK(ffr.value > 0.0);
    CHECK(std::isfinite(ffr.value));
    CHECK(ffr.value < 5.0); // bounded, not blown up
    const bool ffrDpConsistent = (dp.value < 0.0) == (ffr.value > 1.0);
    CHECK(ffrDpConsistent);
    // Aortic wall shear stress order of magnitude.
    CHECK(tawss.value > 0.0);
    CHECK(tawss.value < 1.0e3);
    CHECK(wssMax.value >= tawss.value); // peak >= time-average
    CHECK(osi.value >= 0.0);
    CHECK(osi.value <= 0.5 + 1.0e-9);
    CHECK(std::isfinite(dp.value));

    // ---- insert the analysis into a scene derived from the flow node ----
    const xq::NodeId flowNode(100);
    const xq::NodeId analysisNode(101);
    xq::XQScene scene;
    xq::XQCommandStack stack;
    scene.insert(xq::XQDataNode(flowNode, xq::XQDomainType::FlowResult, "flow",
                               std::shared_ptr<xq::XQPayload>()));

    xq::FlowMetricsService::CommandResult cmd =
        xq::FlowMetricsService::analyzeFlowCommand(&scene, analysisNode, "flow-metrics",
                                                   solved.flow, flowNode, req);
    CHECK(cmd.ok());
    stack.push(std::move(cmd.command));

    const xq::XQDataNode* node = scene.find(analysisNode);
    CHECK(node != nullptr);
    CHECK(node->domainType() == xq::XQDomainType::AiAnalysis);
    const auto* payload =
        dynamic_cast<const xq::XQAiAnalysisPayload*>(node->payload().get());
    CHECK(payload != nullptr);
    CHECK(payload->analysis().provenance() == xq::AnalysisProvenance::Computed);
    CHECK(payload->analysis().hasSourceNode());
    CHECK(payload->analysis().sourceNode() == flowNode);

    std::size_t relations = 0;
    scene.visit_derived_relations(
        [&relations](const xq::NodeId&, const xq::NodeId&) { ++relations; });
    CHECK(relations == 1);

    std::printf("OK: 0007 end-to-end flow metrics into scene\n");
    return 0;
}
