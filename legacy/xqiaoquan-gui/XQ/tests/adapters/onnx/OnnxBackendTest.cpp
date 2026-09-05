// ONNX backend test. Compiled ONLY when XQ_ENABLE_ONNX is ON (see CMakeLists).
// With onnxruntime installed this should be fleshed out to load a minimal .onnx
// model and run one real inference (M6 technical debt). For now it asserts the
// interface is wired up and the backend reports availability.
#include <adapters/onnx/OnnxBackend.h>
#include <core/XQAiAnalysis.h>
#include <services/ai/AiService.h>

#include <cstdio>
#include <string>

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

int main()
{
    // When built with XQ_ENABLE_ONNX, the backend reports as available and can be
    // constructed against a model directory. The XQTensor boundary type and the
    // three backend contracts are exercised here once onnxruntime + a fixture
    // model are wired in (technical debt).
    CHECK(xq::OnnxBackend::isAvailable());

    xq::OnnxBackend backend(std::string("models"));

    // The XQ-owned tensor description must not depend on onnxruntime.
    xq::XQTensor tensor;
    tensor.shape = {1, 1, 4};
    tensor.data = {0.0f, 1.0f, 2.0f, 3.0f};
    CHECK(tensor.elementCount() == 4);

    std::printf("OK: OnnxBackend available and constructible\n");
    return 0;
}
