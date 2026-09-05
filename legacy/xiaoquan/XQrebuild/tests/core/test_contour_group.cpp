#include <core/XQContourGroup.h>

#include <cassert>
#include <cmath>
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

} // namespace

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

    assert(group.contours().size() == 2);
    assert(group.contours()[0].type == xq::ContourType::Manual);
    assert(group.contours()[1].type == xq::ContourType::Ellipse);

    const std::vector<xq::XQContour> ordered = group.orderedByPathPosition();
    assert(ordered.size() == 2);
    assert(ordered[0].pathArcLength < ordered[1].pathArcLength);
    assert(ordered[0].type == xq::ContourType::Ellipse);
    assert(ordered[1].type == xq::ContourType::Manual);

    const xq::NodeId source_path(77);
    group.setSourcePathNode(source_path);
    assert(group.hasSourcePathNode());
    assert(group.sourcePathNode() == source_path);

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
        assert(close(u, coordinates[i][0]));
        assert(close(v, coordinates[i][1]));
        assert(close_point(xq::XQContourGroup::unprojectFromFrame(frame, u, v), world));
    }

    return 0;
}
