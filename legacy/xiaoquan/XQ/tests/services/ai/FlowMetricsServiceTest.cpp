#include <core/NodeId.h>
#include <core/XQAiAnalysis.h>
#include <core/XQAiAnalysisPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQScene.h>
#include <core/command/XQCommandStack.h>
#include <services/ai/FlowMetricsService.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

const double kPi = 3.14159265358979323846;

// Relative error between an observed value and an analytic reference.
double relErr(double observed, double reference)
{
    const double denom = std::fabs(reference) > 1.0e-30 ? std::fabs(reference) : 1.0;
    return std::fabs(observed - reference) / denom;
}

// Closed-form Poiseuille wall shear stress for area A carrying flow Q at
// viscosity mu: tau = 4*mu*Q/(pi*R^3), R = sqrt(A/pi).
double analyticWss(double mu, double q, double area)
{
    const double r = std::sqrt(area / kPi);
    return 4.0 * mu * q / (kPi * r * r * r);
}

// Builds a flow result with nSeg segments (increasing arc length) over the given
// time samples. The series are filled from the provided per-time Q / A scalars
// (shared across segments) and per-segment pressure.
xq::XQFlowResult makeFlow(const std::vector<double>& times,
                          std::size_t nSeg,
                          const std::vector<double>& qByTime,
                          const std::vector<double>& aByTime,
                          // pressure[segment][time]
                          const std::vector<std::vector<double>>& pBySegTime)
{
    xq::XQFlowResult flow;
    flow.setTimes(times);
    for (std::size_t s = 0; s < nSeg; ++s) {
        xq::FlowSegment seg;
        seg.segmentId = static_cast<int>(s);
        seg.arcLengthStart = static_cast<double>(s);
        seg.arcLengthEnd = static_cast<double>(s + 1);
        seg.faceId = 0;
        flow.addSegment(seg);
    }
    std::vector<std::vector<double>> Q(nSeg, qByTime);
    std::vector<std::vector<double>> A(nSeg, aByTime);
    std::vector<std::vector<double>> P = pBySegTime;
    flow.setSeries(Q, P, A);
    return flow;
}

} // namespace

int main()
{
    const double mu = 0.04; // CGS blood viscosity

    // ===================================================================
    // 1. WSS closed-form: constant Q / A over time -> tau = 4*mu*Q/(pi*R^3).
    //    Choose A = 4*pi so R = 2, R^3 = 8 (and R^2 = 4): an R^3 -> R^2 bug
    //    changes tau by a factor of two and is caught by the tolerance.
    // ===================================================================
    {
        const std::vector<double> times = {0.0, 0.25, 0.5, 0.75};
        const double Q0 = 10.0;        // cm^3/s, constant
        const double A0 = 4.0 * kPi;   // cm^2  -> R = 2 cm
        const std::vector<double> q(times.size(), Q0);
        const std::vector<double> a(times.size(), A0);

        // Proximal pressure 1.0e5, distal 7.0e4 -> FFR = 0.7 (also < 0.8 risk).
        const std::size_t nSeg = 3;
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 0.0));
        for (std::size_t t = 0; t < times.size(); ++t) {
            p[0][t] = 1.0e5; // proximal segment (arcLengthStart = 0)
            p[1][t] = 8.5e4;
            p[2][t] = 7.0e4; // distal segment (arcLengthEnd = 3)
        }

        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        CHECK(flow.isConsistent());

        xq::FlowMetricsService::Request req;
        req.mu = mu;
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow, req);
        CHECK(result.ok());

        const double expectedWss = analyticWss(mu, Q0, A0);
        std::printf("WSS analytic = %.8f dyn/cm^2 (R=2)\n", expectedWss);

        xq::NamedMetric wssMax;
        CHECK(result.analysis.metricByName("WSS_max", &wssMax));
        std::printf("WSS_max observed = %.8f, relErr = %.3e\n",
                    wssMax.value, relErr(wssMax.value, expectedWss));
        CHECK(relErr(wssMax.value, expectedWss) < 1.0e-9);

        xq::NamedMetric tawss;
        CHECK(result.analysis.metricByName("TAWSS", &tawss));
        // Constant waveform -> TAWSS equals the instantaneous WSS.
        CHECK(relErr(tawss.value, expectedWss) < 1.0e-9);

        // FFR = Pd / Pa = 7.0e4 / 1.0e5 = 0.7.
        xq::NamedMetric ffr;
        CHECK(result.analysis.metricByName("FFR", &ffr));
        std::printf("FFR observed = %.8f (expected 0.7)\n", ffr.value);
        CHECK(relErr(ffr.value, 0.7) < 1.0e-9);

        // dP = Pa - Pd = 1.0e5 - 7.0e4 = 3.0e4.
        xq::NamedMetric dp;
        CHECK(result.analysis.metricByName("dP", &dp));
        CHECK(relErr(dp.value, 3.0e4) < 1.0e-9);

        // Steady flow -> OSI = 0 (no reversal).
        xq::NamedMetric osi;
        CHECK(result.analysis.metricByName("OSI", &osi));
        CHECK(std::fabs(osi.value) < 1.0e-12);

        // FFR 0.7 < 0.8 -> stenosis annotation at the distal segment.
        CHECK(result.analysis.annotations().size() == 1);
        CHECK(result.analysis.annotations()[0].label == "stenosis");
        CHECK(relErr(result.analysis.annotations()[0].arcLength, 3.0) < 1.0e-9);

        CHECK(result.analysis.kind() == xq::AnalysisKind::FlowMetrics);
        CHECK(result.analysis.provenance() == xq::AnalysisProvenance::Computed);
    }

    // ===================================================================
    // 2. FFR direction: distal/proximal, not the reverse. A high-FFR case
    //    (no stenosis annotation) pins the ratio direction.
    // ===================================================================
    {
        const std::vector<double> times = {0.0, 1.0};
        const std::size_t nSeg = 2;
        const std::vector<double> q(times.size(), 5.0);
        const std::vector<double> a(times.size(), kPi); // R = 1
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 0.0));
        for (std::size_t t = 0; t < times.size(); ++t) {
            p[0][t] = 1.0e5; // proximal
            p[1][t] = 9.5e4; // distal -> FFR = 0.95 (> 0.8, no risk)
        }
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.ok());
        xq::NamedMetric ffr;
        CHECK(result.analysis.metricByName("FFR", &ffr));
        CHECK(relErr(ffr.value, 0.95) < 1.0e-9);
        // If the ratio were inverted (Pa/Pd) it would be ~1.0526, caught here.
        CHECK(ffr.value < 1.0);
        CHECK(result.analysis.annotations().empty()); // FFR >= 0.8
    }

    // ===================================================================
    // 3. OSI extremes from known signed WSS waveforms.
    //    Fully reversing symmetric flow -> OSI = 0.5; one-directional -> 0.
    // ===================================================================
    {
        // Symmetric +/- flow over two equal-weight samples -> integral of tau is
        // zero while integral of |tau| is positive -> OSI = 0.5.
        const std::vector<double> times = {0.0, 1.0};
        const std::size_t nSeg = 1;
        std::vector<double> q = {7.0, -7.0};
        const std::vector<double> a(times.size(), kPi);
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 1.0e5));
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.ok());
        xq::NamedMetric osi;
        CHECK(result.analysis.metricByName("OSI", &osi));
        std::printf("OSI (reversing) = %.8f (expected 0.5)\n", osi.value);
        CHECK(relErr(osi.value, 0.5) < 1.0e-9);
    }
    {
        // One-directional flow -> integral of tau equals integral of |tau| ->
        // OSI = 0.
        const std::vector<double> times = {0.0, 1.0, 2.0};
        const std::size_t nSeg = 1;
        std::vector<double> q = {3.0, 5.0, 4.0};
        const std::vector<double> a(times.size(), kPi);
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 1.0e5));
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.ok());
        xq::NamedMetric osi;
        CHECK(result.analysis.metricByName("OSI", &osi));
        std::printf("OSI (one-directional) = %.8f (expected 0)\n", osi.value);
        CHECK(std::fabs(osi.value) < 1.0e-12);
    }

    // ===================================================================
    // 4. InvalidFlow diagnostics: inconsistent / empty / non-positive area / Pa.
    // ===================================================================
    {
        // Inconsistent: series sizes do not match times/segments.
        xq::XQFlowResult flow;
        flow.setTimes({0.0, 1.0});
        xq::FlowSegment seg;
        seg.segmentId = 0;
        flow.addSegment(seg);
        // no series set -> inconsistent
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.status == xq::FlowMetricsService::Status::InvalidFlow);
    }
    {
        // Empty (no segments / times).
        xq::XQFlowResult flow;
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.status == xq::FlowMetricsService::Status::InvalidFlow);
    }
    {
        // Non-positive area -> InvalidFlow.
        const std::vector<double> times = {0.0, 1.0};
        const std::size_t nSeg = 1;
        const std::vector<double> q(times.size(), 5.0);
        const std::vector<double> a = {kPi, -1.0}; // negative area
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 1.0e5));
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.status == xq::FlowMetricsService::Status::InvalidFlow);
    }
    {
        // Non-positive proximal mean pressure -> InvalidFlow (FFR undefined).
        const std::vector<double> times = {0.0, 1.0};
        const std::size_t nSeg = 1;
        const std::vector<double> q(times.size(), 5.0);
        const std::vector<double> a(times.size(), kPi);
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 0.0));
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);
        const xq::FlowMetricsService::Result result =
            xq::FlowMetricsService::analyzeFlow(flow);
        CHECK(result.status == xq::FlowMetricsService::Status::InvalidFlow);
    }

    // ===================================================================
    // 5. analyzeFlowCommand: into scene with source relation + undo/redo.
    // ===================================================================
    {
        const xq::NodeId flowId(1);
        const xq::NodeId analysisId(2);

        const std::vector<double> times = {0.0, 1.0};
        const std::size_t nSeg = 2;
        const std::vector<double> q(times.size(), 5.0);
        const std::vector<double> a(times.size(), kPi);
        std::vector<std::vector<double>> p(nSeg, std::vector<double>(times.size(), 0.0));
        for (std::size_t t = 0; t < times.size(); ++t) {
            p[0][t] = 1.0e5;
            p[1][t] = 9.0e4;
        }
        xq::XQFlowResult flow = makeFlow(times, nSeg, q, a, p);

        xq::XQScene scene;
        xq::XQCommandStack stack;
        scene.insert(xq::XQDataNode(flowId, xq::XQDomainType::FlowResult, "flow",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::FlowMetricsService::CommandResult cmd =
            xq::FlowMetricsService::analyzeFlowCommand(&scene, analysisId, "metrics",
                                                       flow, flowId);
        CHECK(cmd.ok());
        CHECK(cmd.command != nullptr);

        std::size_t nodes = 0;
        scene.visit_nodes([&nodes](const xq::XQDataNode&) { ++nodes; });
        CHECK(nodes == 1);

        stack.push(std::move(cmd.command));

        nodes = 0;
        scene.visit_nodes([&nodes](const xq::XQDataNode&) { ++nodes; });
        CHECK(nodes == 2);

        std::size_t relations = 0;
        scene.visit_derived_relations(
            [&relations](const xq::NodeId&, const xq::NodeId&) { ++relations; });
        CHECK(relations == 1); // flow -> analysis

        const xq::XQDataNode* node = scene.find(analysisId);
        CHECK(node != nullptr);
        CHECK(node->domainType() == xq::XQDomainType::AiAnalysis);
        const auto* payload =
            dynamic_cast<const xq::XQAiAnalysisPayload*>(node->payload().get());
        CHECK(payload != nullptr);
        CHECK(payload->analysis().provenance() == xq::AnalysisProvenance::Computed);
        CHECK(payload->analysis().hasSourceNode());
        CHECK(payload->analysis().sourceNode() == flowId);
        xq::NamedMetric ffr;
        CHECK(payload->analysis().metricByName("FFR", &ffr));
        CHECK(relErr(ffr.value, 0.9) < 1.0e-9);

        const bool undone = stack.undo();
        CHECK(undone);
        nodes = 0;
        scene.visit_nodes([&nodes](const xq::XQDataNode&) { ++nodes; });
        CHECK(nodes == 1);
        CHECK(scene.find(analysisId) == nullptr);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(scene.find(analysisId) != nullptr);
    }
    {
        // Null scene rejected.
        const std::vector<double> times = {0.0, 1.0};
        const std::vector<double> q(times.size(), 5.0);
        const std::vector<double> a(times.size(), kPi);
        std::vector<std::vector<double>> p(1, std::vector<double>(times.size(), 1.0e5));
        xq::XQFlowResult flow = makeFlow(times, 1, q, a, p);
        xq::FlowMetricsService::CommandResult cmd =
            xq::FlowMetricsService::analyzeFlowCommand(nullptr, xq::NodeId(2), "m",
                                                       flow, xq::NodeId(1));
        CHECK(cmd.status == xq::FlowMetricsService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }
    {
        // InvalidFlow propagates through the command path (no command).
        xq::XQScene scene;
        xq::XQFlowResult empty;
        xq::FlowMetricsService::CommandResult cmd =
            xq::FlowMetricsService::analyzeFlowCommand(&scene, xq::NodeId(2), "m",
                                                       empty, xq::NodeId(1));
        CHECK(cmd.status == xq::FlowMetricsService::Status::InvalidFlow);
        CHECK(cmd.command == nullptr);
    }

    // ===================================================================
    // 6. Payload deep copy: cloning then mutating the clone leaves the
    //    original untouched.
    // ===================================================================
    {
        xq::XQAiAnalysis analysis;
        analysis.setKind(xq::AnalysisKind::FlowMetrics);
        xq::NamedMetric m;
        m.name = "FFR";
        m.value = 0.5;
        analysis.addMetric(m);

        xq::XQAiAnalysisPayload payload(analysis);
        CHECK(payload.domainType() == xq::XQDomainType::AiAnalysis);
        const std::shared_ptr<xq::XQPayload> cloned = payload.clone();
        CHECK(cloned != nullptr);
        auto* clonedPayload = dynamic_cast<xq::XQAiAnalysisPayload*>(cloned.get());
        CHECK(clonedPayload != nullptr);

        xq::NamedMetric extra;
        extra.name = "WSS_max";
        extra.value = 99.0;
        clonedPayload->analysis().addMetric(extra);

        CHECK(clonedPayload->analysis().metrics().size() == 2);
        CHECK(payload.analysis().metrics().size() == 1); // original untouched
    }

    std::printf("OK: FlowMetricsService analytic checks passed\n");
    return 0;
}
