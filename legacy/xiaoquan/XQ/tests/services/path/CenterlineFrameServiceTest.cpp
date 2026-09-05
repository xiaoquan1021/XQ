#include <core/GeometryTypes.h>
#include <core/XQPath.h>
#include <services/path/CenterlineFrameService.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

bool close(double a, double b, double epsilon)
{
    return std::abs(a - b) < epsilon;
}

bool unit_vector(const xq::Vec3& value)
{
    return close(xq::norm(value), 1.0, 1e-6);
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects (computeFrames) are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

int main()
{
    // --- validation --------------------------------------------------------
    {
        xq::XQPath one_point;
        one_point.setControlPoints({{{0.0, 0.0, 0.0}}});
        const xq::CenterlineFrameService::Result too_few =
            xq::CenterlineFrameService::computeFrames(one_point, 1.0);
        CHECK(too_few.status == xq::CenterlineFrameService::Status::NotEnoughControlPoints);
        CHECK(too_few.frames.empty());

        xq::XQPath line;
        line.setControlPoints({{{0.0, 0.0, 0.0}}, {{10.0, 0.0, 0.0}}});
        const xq::CenterlineFrameService::Result bad_spacing =
            xq::CenterlineFrameService::computeFrames(line, 0.0);
        CHECK(bad_spacing.status == xq::CenterlineFrameService::Status::InvalidSpacing);
        CHECK(bad_spacing.frames.empty());
    }

    // --- a smoothly curving path (a discretized helix): its tangent turns
    //     continuously, so rotation-minimizing transport keeps the normal
    //     continuous. A naive per-segment recompute could still jump the normal;
    //     transport must not. --------------------------------------------------
    {
        // Helix: x = r cos t, y = r sin t, z = pitch * t, sampled densely so the
        // polyline tangent turns in small increments between control points.
        xq::XQPath path;
        std::vector<xq::PathControlPoint> control_points;
        const double radius = 10.0;
        const double pitch = 2.0;
        const int steps = 64;
        const double pi = 3.14159265358979323846;
        for (int i = 0; i <= steps; ++i) {
            const double t = (static_cast<double>(i) / steps) * (4.0 * pi); // two turns
            xq::PathControlPoint cp;
            cp.position.x = radius * std::cos(t);
            cp.position.y = radius * std::sin(t);
            cp.position.z = pitch * t;
            control_points.push_back(cp);
        }
        path.setControlPoints(control_points);

        const xq::CenterlineFrameService::Result result =
            xq::CenterlineFrameService::computeFrames(path, 1.0);
        CHECK(result.ok());
        CHECK(result.frames.size() >= 2);

        // arcLength is monotonically increasing.
        for (std::size_t i = 1; i < result.frames.size(); ++i) {
            CHECK(result.frames[i].arcLength > result.frames[i - 1].arcLength - 1e-9);
        }

        // each frame is orthonormal.
        for (std::size_t i = 0; i < result.frames.size(); ++i) {
            const xq::PathFrame& frame = result.frames[i];
            CHECK(unit_vector(frame.tangent));
            CHECK(unit_vector(frame.normal));
            CHECK(unit_vector(frame.binormal));
            CHECK(close(xq::dot(frame.tangent, frame.normal), 0.0, 1e-6));
            CHECK(close(xq::dot(frame.tangent, frame.binormal), 0.0, 1e-6));
            CHECK(close(xq::dot(frame.normal, frame.binormal), 0.0, 1e-6));
        }

        // the normal does not flip between adjacent frames: under continuous
        // transport along a smooth curve, consecutive normals stay strongly
        // aligned (dot product well above 0). A flip would give a value near -1.
        for (std::size_t i = 1; i < result.frames.size(); ++i) {
            const double alignment =
                xq::dot(result.frames[i].normal, result.frames[i - 1].normal);
            CHECK(alignment > 0.5);
        }
    }

    return 0;
}
