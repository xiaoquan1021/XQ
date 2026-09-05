#include <core/XQContourGroup.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

bool close_point(const xq::Point3& a, const xq::Point3& b)
{
    return close(a.x, b.x) && close(a.y, b.y) && close(a.z, b.z);
}

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survives Release /DNDEBUG.
#define CHECK(cond)                            \
    do {                                       \
        if (!(cond)) {                         \
            return fail(#cond, __LINE__);      \
        }                                      \
    } while (0)

int main()
{
    const xq::ContourFrame frame = {
        {1.0, 2.0, 3.0},
        {0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };

    xq::XQContour manual = {};
    manual.contourId = xq::ContourId(1);
    manual.pathArcLength = 5.0;
    manual.frame = frame;
    manual.type = xq::ContourType::Manual;
    manual.points = {{1.0, 2.0, 3.0}, {2.0, 2.0, 3.0}};
    manual.closed = true;

    xq::XQContour ellipse = {};
    ellipse.contourId = xq::ContourId(2);
    ellipse.pathArcLength = 2.0;
    ellipse.frame = frame;
    ellipse.type = xq::ContourType::Ellipse;
    ellipse.points = {{1.0, 3.0, 3.0}, {2.0, 3.0, 3.0}};
    ellipse.closed = true;

    xq::XQContourGroup group;
    group.addContour(manual);
    group.addContour(ellipse);

    CHECK(group.contours().size() == 2);
    CHECK(group.contours()[0].type == xq::ContourType::Manual);
    CHECK(group.contours()[1].type == xq::ContourType::Ellipse);

    const std::vector<xq::XQContour> ordered = group.orderedByPathPosition();
    CHECK(ordered.size() == 2);
    CHECK(ordered[0].pathArcLength < ordered[1].pathArcLength);
    CHECK(ordered[0].type == xq::ContourType::Ellipse);
    CHECK(ordered[1].type == xq::ContourType::Manual);

    const xq::NodeId source_path(77);
    group.setSourcePathNode(source_path);
    CHECK(group.hasSourcePathNode());
    CHECK(group.sourcePathNode() == source_path);

    const double coordinates[][2] = {
        {0.0, 0.0},
        {2.5, -1.25},
        {-3.0, 4.0},
    };
    for (int i = 0; i < 3; ++i) {
        const xq::Point3 world =
            xq::XQContourGroup::unprojectFromFrame(frame, coordinates[i][0], coordinates[i][1]);
        double u = 0.0;
        double v = 0.0;
        xq::XQContourGroup::projectToFrame(frame, world, &u, &v);
        CHECK(close(u, coordinates[i][0]));
        CHECK(close(v, coordinates[i][1]));
        const xq::Point3 roundtrip = xq::XQContourGroup::unprojectFromFrame(frame, u, v);
        CHECK(close_point(roundtrip, world));
    }

    return 0;
}
