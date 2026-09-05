#include <core/XQPath.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

bool close(double a, double b, double epsilon)
{
    return std::abs(a - b) < epsilon;
}

bool unit_vector(const xq::Vec3& value)
{
    return close(xq::norm(value), 1.0, 1e-6);
}

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survives Release /DNDEBUG. Calls with
// side effects (resample, frameAtArcLength) are evaluated to a variable first.
#define CHECK(cond)                            \
    do {                                       \
        if (!(cond)) {                         \
            return fail(#cond, __LINE__);      \
        }                                      \
    } while (0)

int main()
{
    xq::XQPath path;

    {
        const std::vector<xq::PathControlPoint> points = {
            {{0.0, 0.0, 0.0}},
            {{10.0, 0.0, 0.0}},
            {{10.0, 10.0, 0.0}},
        };
        path.setControlPoints(points);

        CHECK(path.controlPoints().size() == points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            CHECK(close(path.controlPoints()[i].position.x, points[i].position.x, 1e-9));
            CHECK(close(path.controlPoints()[i].position.y, points[i].position.y, 1e-9));
            CHECK(close(path.controlPoints()[i].position.z, points[i].position.z, 1e-9));
        }
    }

    {
        xq::PathFrame frame = {};
        const xq::XQPath::FrameStatus status = path.frameAtArcLength(1.0, &frame);
        CHECK(status == xq::XQPath::FrameStatus::NotResampled);
    }

    const xq::NodeId source_image(42);
    path.setSourceImageNode(source_image);
    CHECK(path.hasSourceImageNode());
    CHECK(path.sourceImageNode() == source_image);

    {
        xq::XQPath short_path;
        short_path.setControlPoints({{{0.0, 0.0, 0.0}}});
        const xq::XQPath::ResampleStatus short_status = short_path.resample(1.0);
        CHECK(short_status == xq::XQPath::ResampleStatus::NotEnoughPoints);

        xq::XQPath invalid_spacing_path;
        invalid_spacing_path.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        const xq::XQPath::ResampleStatus invalid_status = invalid_spacing_path.resample(0.0);
        CHECK(invalid_status == xq::XQPath::ResampleStatus::InvalidSpacing);

        // Sample-count ceiling: a hostile tiny spacing against a large arc length
        // would allocate an unbounded number of samples. resample must reject it
        // (InvalidSpacing) instead of hanging/OOMing. arcLength here is 1.0, so
        // spacing 1e-9 asks for 1e9 samples (>> 1e6 ceiling).
        xq::XQPath hostile_path;
        hostile_path.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        const xq::XQPath::ResampleStatus hostile_status = hostile_path.resample(1e-9);
        CHECK(hostile_status == xq::XQPath::ResampleStatus::InvalidSpacing);
        CHECK(hostile_path.samplePoints().empty());

        // Just under the ceiling still resamples fine (arcLength 1.0, spacing
        // 1e-5 -> 1e5 samples < 1e6).
        xq::XQPath ok_dense_path;
        ok_dense_path.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        const xq::XQPath::ResampleStatus ok_dense_status = ok_dense_path.resample(1e-5);
        CHECK(ok_dense_status == xq::XQPath::ResampleStatus::Ok);
    }

    const xq::XQPath::ResampleStatus resample_status = path.resample(1.0);
    CHECK(resample_status == xq::XQPath::ResampleStatus::Ok);
    CHECK(path.sampleSpacing() == 1.0);
    CHECK(!path.samplePoints().empty());
    CHECK(close(path.samplePoints().front().arcLength, 0.0, 1e-9));

    const double total_arc_length = 20.0;
    for (std::size_t i = 1; i < path.samplePoints().size(); ++i) {
        CHECK(path.samplePoints()[i].arcLength > path.samplePoints()[i - 1].arcLength);
    }
    CHECK(std::abs(path.samplePoints().back().arcLength - total_arc_length) <= path.sampleSpacing());

    for (std::size_t i = 0; i < path.samplePoints().size(); ++i) {
        const xq::PathSamplePoint& sample = path.samplePoints()[i];
        CHECK(unit_vector(sample.tangent));
        CHECK(unit_vector(sample.normal));
        CHECK(unit_vector(sample.binormal));
        CHECK(close(xq::dot(sample.tangent, sample.normal), 0.0, 1e-6));
        CHECK(close(xq::dot(sample.tangent, sample.binormal), 0.0, 1e-6));
        CHECK(close(xq::dot(sample.normal, sample.binormal), 0.0, 1e-6));
    }

    {
        xq::PathFrame frame = {};
        const xq::XQPath::FrameStatus status = path.frameAtArcLength(total_arc_length * 0.5, &frame);
        CHECK(status == xq::XQPath::FrameStatus::Ok);
        CHECK(close(frame.position.x, 10.0, 1e-9));
        CHECK(close(frame.position.y, 0.0, 1e-9));
        CHECK(close(frame.position.z, 0.0, 1e-9));
        CHECK(close(frame.arcLength, total_arc_length * 0.5, 1e-9));
    }

    // framesForAllSamples: NotResampled before resample / with a null out.
    {
        xq::XQPath unsampled;
        unsampled.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        std::vector<xq::PathFrame> frames;
        const xq::XQPath::FrameStatus status = unsampled.framesForAllSamples(&frames);
        CHECK(status == xq::XQPath::FrameStatus::NotResampled);
        CHECK(frames.empty());

        const xq::XQPath::ResampleStatus rs = unsampled.resample(0.5);
        CHECK(rs == xq::XQPath::ResampleStatus::Ok);
        const xq::XQPath::FrameStatus null_status = unsampled.framesForAllSamples(0);
        CHECK(null_status == xq::XQPath::FrameStatus::NotResampled);
    }

    // Cross-check on a long 3D zig-zag: framesForAllSamples must agree with
    // frameAtArcLength at every sample (this also exercises the cursor-based
    // segment lookup in resample against the cursor-free frameAtArcLength path).
    {
        xq::XQPath dense;
        std::vector<xq::PathControlPoint> points;
        for (int i = 0; i < 1000; ++i) {
            xq::PathControlPoint cp;
            cp.position.x = 0.1 * static_cast<double>(i);
            cp.position.y = (i % 2) == 0 ? 0.0 : 0.05;
            cp.position.z = 0.02 * static_cast<double>(i);
            points.push_back(cp);
        }
        dense.setControlPoints(points);

        const xq::XQPath::ResampleStatus rs = dense.resample(0.25);
        CHECK(rs == xq::XQPath::ResampleStatus::Ok);

        std::vector<xq::PathFrame> frames;
        const xq::XQPath::FrameStatus fs = dense.framesForAllSamples(&frames);
        CHECK(fs == xq::XQPath::FrameStatus::Ok);
        CHECK(frames.size() == dense.samplePoints().size());

        for (std::size_t i = 0; i < frames.size(); ++i) {
            xq::PathFrame reference = {};
            const xq::XQPath::FrameStatus ref_status =
                dense.frameAtArcLength(dense.samplePoints()[i].arcLength, &reference);
            CHECK(ref_status == xq::XQPath::FrameStatus::Ok);
            CHECK(close(frames[i].arcLength, reference.arcLength, 1e-9));
            CHECK(close(frames[i].position.x, reference.position.x, 1e-9));
            CHECK(close(frames[i].position.y, reference.position.y, 1e-9));
            CHECK(close(frames[i].position.z, reference.position.z, 1e-9));
            CHECK(close(frames[i].tangent.x, reference.tangent.x, 1e-9));
            CHECK(close(frames[i].tangent.y, reference.tangent.y, 1e-9));
            CHECK(close(frames[i].tangent.z, reference.tangent.z, 1e-9));
            CHECK(close(frames[i].normal.x, reference.normal.x, 1e-9));
            CHECK(close(frames[i].normal.y, reference.normal.y, 1e-9));
            CHECK(close(frames[i].normal.z, reference.normal.z, 1e-9));
            CHECK(close(frames[i].binormal.x, reference.binormal.x, 1e-9));
            CHECK(close(frames[i].binormal.y, reference.binormal.y, 1e-9));
            CHECK(close(frames[i].binormal.z, reference.binormal.z, 1e-9));
        }
    }

    return 0;
}
