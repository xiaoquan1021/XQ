#include <core/XQPath.h>

#include <cassert>
#include <cmath>
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

} // namespace

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

        assert(path.controlPoints().size() == points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            assert(close(path.controlPoints()[i].position.x, points[i].position.x, 1e-9));
            assert(close(path.controlPoints()[i].position.y, points[i].position.y, 1e-9));
            assert(close(path.controlPoints()[i].position.z, points[i].position.z, 1e-9));
        }
    }

    {
        xq::PathFrame frame = {};
        assert(path.frameAtArcLength(1.0, &frame) == xq::XQPath::FrameStatus::NotResampled);
    }

    const xq::NodeId source_image(42);
    path.setSourceImageNode(source_image);
    assert(path.hasSourceImageNode());
    assert(path.sourceImageNode() == source_image);

    {
        xq::XQPath short_path;
        short_path.setControlPoints({{{0.0, 0.0, 0.0}}});
        assert(short_path.resample(1.0) == xq::XQPath::ResampleStatus::NotEnoughPoints);

        xq::XQPath invalid_spacing_path;
        invalid_spacing_path.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        assert(invalid_spacing_path.resample(0.0) == xq::XQPath::ResampleStatus::InvalidSpacing);
    }

    assert(path.resample(1.0) == xq::XQPath::ResampleStatus::Ok);
    assert(path.sampleSpacing() == 1.0);
    assert(!path.samplePoints().empty());
    assert(close(path.samplePoints().front().arcLength, 0.0, 1e-9));

    const double total_arc_length = 20.0;
    for (std::size_t i = 1; i < path.samplePoints().size(); ++i) {
        assert(path.samplePoints()[i].arcLength > path.samplePoints()[i - 1].arcLength);
    }
    assert(std::abs(path.samplePoints().back().arcLength - total_arc_length) <= path.sampleSpacing());

    for (std::size_t i = 0; i < path.samplePoints().size(); ++i) {
        const xq::PathSamplePoint& sample = path.samplePoints()[i];
        assert(unit_vector(sample.tangent));
        assert(unit_vector(sample.normal));
        assert(unit_vector(sample.binormal));
        assert(close(xq::dot(sample.tangent, sample.normal), 0.0, 1e-6));
        assert(close(xq::dot(sample.tangent, sample.binormal), 0.0, 1e-6));
        assert(close(xq::dot(sample.normal, sample.binormal), 0.0, 1e-6));
    }

    {
        xq::PathFrame frame = {};
        assert(path.frameAtArcLength(total_arc_length * 0.5, &frame) == xq::XQPath::FrameStatus::Ok);
        assert(close(frame.position.x, 10.0, 1e-9));
        assert(close(frame.position.y, 0.0, 1e-9));
        assert(close(frame.position.z, 0.0, 1e-9));
        assert(close(frame.arcLength, total_arc_length * 0.5, 1e-9));
    }

    return 0;
}
