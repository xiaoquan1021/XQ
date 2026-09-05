#include <core/NodeId.h>
#include <core/XQAiAnalysis.h>
#include <core/XQAiAnalysisPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResult.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQScene.h>
#include <core/XQSegmentationMask.h>
#include <core/XQSurfaceModel.h>
#include <core/command/XQCommandStack.h>
#include <services/ai/AiService.h>
#include <core/XQAiSegmentationRequest.h>

#include <cstddef>
#include <cstdint>
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

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

const double kPi = 3.14159265358979323846;

// ---- synthetic image ----
struct SyntheticImage {
    xq::XQImageVolume image;
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
};

SyntheticImage makeImage(int dimX, const std::vector<std::uint8_t>& values)
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = dimX;
    geometry.dimensions[1] = 1;
    geometry.dimensions[2] = 1;
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 1.0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            geometry.direction[r][c] = r == c ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    SyntheticImage out;
    out.image.setGeometry(geometry);
    out.image.setScalarType(xq::ScalarType::UInt8);
    out.image.setComponentCount(1);
    std::vector<std::uint8_t> bytes(values.begin(), values.end());
    out.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, geometry.dimensions, 1, std::move(bytes));
    return out;
}

// ---- mock segmentation backend (M2 contract): threshold >= 128 -> label ----
class MockSegBackend : public xq::XQAiSegmentationBackend {
public:
    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume& image,
        const xq::XQMemoryImageBufferHandle& buffer,
        const xq::XQAiSegmentationRequest& request) override
    {
        const xq::ImageGeometry& geometry = image.geometry();
        auto mask = std::make_shared<xq::XQSegmentationMask>(geometry.dimensions);
        mask->setGeometry(geometry);
        const int label = request.targetLabels.empty() ? 1 : request.targetLabels.front();
        const std::size_t voxels = buffer.voxelCount();
        for (std::size_t i = 0; i < voxels; ++i) {
            if (buffer.scalarAt(i, 0) >= 128.0) {
                mask->setLabelAt(i, static_cast<xq::XQSegmentationMask::LabelType>(label));
            }
        }
        return mask;
    }
};

class NullSegBackend : public xq::XQAiSegmentationBackend {
public:
    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume&,
        const xq::XQMemoryImageBufferHandle&,
        const xq::XQAiSegmentationRequest&) override
    {
        return nullptr;
    }
};

// ---- mock identify backend ----
class MockIdentifyBackend : public xq::XQAiIdentifyBackend {
public:
    int calls = 0;
    xq::XQAiAnalysis identify(const xq::XQSurfaceModel& model,
                              const xq::XQAiIdentifyRequest& request) override
    {
        ++calls;
        xq::XQAiAnalysis analysis;
        // Report one stenosis at the model's face count (arbitrary deterministic
        // mock output) to prove the backend result flows through.
        xq::Annotation a;
        a.label = "stenosis";
        a.arcLength = static_cast<double>(model.faces().size());
        a.faceId = 1;
        a.score = 0.9;
        analysis.addAnnotation(a);
        analysis.setModelId(request.modelId);
        return analysis;
    }
};

// ---- mock surrogate backend: returns a small valid flow result ----
class MockSurrogateBackend : public xq::XQSurrogateBackend {
public:
    std::shared_ptr<xq::XQFlowResult> predict(const xq::XQSurfaceModel&,
                                              const xq::XQSurrogateRequest&) override
    {
        auto flow = std::make_shared<xq::XQFlowResult>();
        flow->setTimes({0.0, 1.0});
        xq::FlowSegment seg;
        seg.segmentId = 0;
        seg.arcLengthStart = 0.0;
        seg.arcLengthEnd = 1.0;
        flow->addSegment(seg);
        std::vector<std::vector<double>> q = {{5.0, 5.0}};
        std::vector<std::vector<double>> p = {{1.0e5, 1.0e5}};
        std::vector<std::vector<double>> a = {{kPi, kPi}};
        flow->setSeries(q, p, a);
        return flow;
    }
};

class NullSurrogateBackend : public xq::XQSurrogateBackend {
public:
    std::shared_ptr<xq::XQFlowResult> predict(const xq::XQSurfaceModel&,
                                              const xq::XQSurrogateRequest&) override
    {
        return nullptr;
    }
};

xq::XQSurfaceModel makeModel()
{
    xq::XQSurfaceModel model;
    xq::ModelFace face;
    face.faceId = 1;
    face.name = "wall";
    face.kind = xq::FaceKind::Wall;
    model.addFace(face);
    return model;
}

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t n = 0;
    scene.visit_nodes([&n](const xq::XQDataNode&) { ++n; });
    return n;
}

std::size_t relationCount(const xq::XQScene& scene)
{
    std::size_t n = 0;
    scene.visit_derived_relations([&n](const xq::NodeId&, const xq::NodeId&) { ++n; });
    return n;
}

} // namespace

int main()
{
    // ===================================================================
    // 1. segment(): delegates to the M2 backend, returns the mask.
    // ===================================================================
    {
        SyntheticImage syn = makeImage(4, {10, 200, 50, 130});
        MockSegBackend backend;
        xq::XQAiSegmentationRequest request;
        request.modelId = "mock-seg";
        request.targetLabels = {7};
        const xq::AiService::SegmentResult result =
            xq::AiService::segment(syn.image, *syn.buffer, request, backend);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);
        // values >= 128: voxel 1 (200) and 3 (130) -> 2 foreground.
        CHECK(result.mask->foregroundVoxelCount() == 2);
        CHECK(result.mask->labelAt(1) == 7);
    }
    {
        // null mask from the backend -> InvalidInput.
        SyntheticImage syn = makeImage(2, {100, 200});
        NullSegBackend backend;
        xq::XQAiSegmentationRequest request;
        const xq::AiService::SegmentResult result =
            xq::AiService::segment(syn.image, *syn.buffer, request, backend);
        CHECK(result.status == xq::AiService::Status::InvalidInput);
        CHECK(result.mask == nullptr);
    }

    // ===================================================================
    // 2. identify(): backend analysis, provenance stamped ModelInferred,
    //    source node bound, can be inserted into the scene + undo.
    // ===================================================================
    {
        const xq::NodeId modelNode(10);
        const xq::NodeId analysisNode(11);
        xq::XQSurfaceModel model = makeModel();
        MockIdentifyBackend backend;
        xq::XQAiIdentifyRequest request;
        request.modelId = "mock-identify";

        const xq::AiService::AnalysisResult result =
            xq::AiService::identify(model, request, backend, modelNode);
        CHECK(result.ok());
        CHECK(backend.calls == 1);
        CHECK(result.analysis.kind() == xq::AnalysisKind::Identify);
        CHECK(result.analysis.provenance() == xq::AnalysisProvenance::ModelInferred);
        CHECK(result.analysis.modelId() == "mock-identify");
        CHECK(result.analysis.hasSourceNode());
        CHECK(result.analysis.sourceNode() == modelNode);
        CHECK(result.analysis.annotations().size() == 1);
        CHECK(result.analysis.annotations()[0].label == "stenosis");

        // Insert into scene with the source relation, then undo.
        xq::XQScene scene;
        xq::XQCommandStack stack;
        scene.insert(xq::XQDataNode(modelNode, xq::XQDomainType::SurfaceModel, "model",
                                    std::shared_ptr<xq::XQPayload>()));
        xq::AiService::CommandResult cmd =
            xq::AiService::buildAnalysisCommand(&scene, analysisNode, "identify",
                                                result.analysis);
        CHECK(cmd.ok());
        stack.push(std::move(cmd.command));
        CHECK(nodeCount(scene) == 2);
        CHECK(relationCount(scene) == 1);
        const xq::XQDataNode* node = scene.find(analysisNode);
        CHECK(node != nullptr);
        CHECK(node->domainType() == xq::XQDomainType::AiAnalysis);

        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(scene.find(analysisNode) == nullptr);
        CHECK(relationCount(scene) == 0);
    }

    // ===================================================================
    // 3. analyzeFlow(): delegates to FlowMetricsService (Computed provenance).
    // ===================================================================
    {
        xq::XQFlowResult flow;
        flow.setTimes({0.0, 1.0});
        for (int s = 0; s < 2; ++s) {
            xq::FlowSegment seg;
            seg.segmentId = s;
            seg.arcLengthStart = static_cast<double>(s);
            seg.arcLengthEnd = static_cast<double>(s + 1);
            flow.addSegment(seg);
        }
        std::vector<std::vector<double>> q = {{5.0, 5.0}, {5.0, 5.0}};
        std::vector<std::vector<double>> p = {{1.0e5, 1.0e5}, {9.0e4, 9.0e4}};
        std::vector<std::vector<double>> a = {{kPi, kPi}, {kPi, kPi}};
        flow.setSeries(q, p, a);

        const xq::AiService::AnalysisResult result = xq::AiService::analyzeFlow(flow);
        CHECK(result.ok());
        CHECK(result.analysis.kind() == xq::AnalysisKind::FlowMetrics);
        CHECK(result.analysis.provenance() == xq::AnalysisProvenance::Computed);
        xq::NamedMetric ffr;
        CHECK(result.analysis.metricByName("FFR", &ffr));
        CHECK(ffr.value > 0.89 && ffr.value < 0.91); // 0.9
    }
    {
        // Invalid flow -> InvalidFlow.
        xq::XQFlowResult empty;
        const xq::AiService::AnalysisResult result = xq::AiService::analyzeFlow(empty);
        CHECK(result.status == xq::AiService::Status::InvalidFlow);
    }

    // ===================================================================
    // 4. predictFlow(): surrogate backend, flow marked SurrogatePredicted.
    // ===================================================================
    {
        const xq::NodeId modelNode(20);
        xq::XQSurfaceModel model = makeModel();
        MockSurrogateBackend backend;
        xq::XQSurrogateRequest request;
        request.modelId = "mock-surrogate";

        const xq::AiService::PredictResult result =
            xq::AiService::predictFlow(model, request, backend, modelNode);
        CHECK(result.ok());
        CHECK(result.flow != nullptr);
        CHECK(result.flow->isConsistent());
        // The provenance marker is the load-bearing distinction from a solve.
        CHECK(result.predictedAnalysis.kind() == xq::AnalysisKind::SurrogatePrediction);
        CHECK(result.predictedAnalysis.provenance()
              == xq::AnalysisProvenance::SurrogatePredicted);
        CHECK(result.predictedAnalysis.modelId() == "mock-surrogate");
        CHECK(result.predictedAnalysis.hasSourceNode());
        CHECK(result.predictedAnalysis.sourceNode() == modelNode);
    }
    {
        // null prediction -> InvalidInput.
        xq::XQSurfaceModel model = makeModel();
        NullSurrogateBackend backend;
        xq::XQSurrogateRequest request;
        const xq::AiService::PredictResult result =
            xq::AiService::predictFlow(model, request, backend);
        CHECK(result.status == xq::AiService::Status::InvalidInput);
        CHECK(result.flow == nullptr);
    }

    // ===================================================================
    // 5. buildAnalysisCommand: null scene rejected; no-source -> plain add.
    // ===================================================================
    {
        xq::XQAiAnalysis analysis;
        xq::AiService::CommandResult cmd =
            xq::AiService::buildAnalysisCommand(nullptr, xq::NodeId(1), "a", analysis);
        CHECK(cmd.status == xq::AiService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }
    {
        // analysis without a source node -> plain AddNodeCommand (no relation).
        xq::XQScene scene;
        xq::XQCommandStack stack;
        xq::XQAiAnalysis analysis;
        analysis.setKind(xq::AnalysisKind::FlowMetrics);
        xq::AiService::CommandResult cmd =
            xq::AiService::buildAnalysisCommand(&scene, xq::NodeId(1), "a", analysis);
        CHECK(cmd.ok());
        stack.push(std::move(cmd.command));
        CHECK(nodeCount(scene) == 1);
        CHECK(relationCount(scene) == 0);
    }

    std::printf("OK: AiService mock-backend chain passed\n");
    return 0;
}
