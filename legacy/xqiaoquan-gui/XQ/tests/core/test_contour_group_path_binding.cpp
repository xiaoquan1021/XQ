#include <core/XQContourGroup.h>
#include <services/modeling/ContourLoftInputBuilder.h>
#include <services/segmentation/ContourExtractionService.h>

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

// Builds one circular XQContour at the given arc length: the section-local 2D
// circle points are lifted to world space through unprojectFromFrame, exactly
// as the app maps a hand-drawn contour. The frame is centered at `origin` with
// unit x/y axes so the world points stay easy to reason about.
xq::XQContour makeCircleContour(xq::NodeId contourId, double arcLength,
                                const xq::Point3& origin, double radius)
{
    xq::ContourFrame frame;
    frame.origin = origin;
    frame.normal = {0.0, 0.0, 1.0};
    frame.xAxis = {1.0, 0.0, 0.0};
    frame.yAxis = {0.0, 1.0, 0.0};

    const std::vector<xq::ContourPoint2D> points2d =
        xq::ContourExtractionService::circle({0.0, 0.0}, {radius, 0.0}, 36);

    xq::XQContour contour;
    contour.contourId = contourId;
    contour.pathArcLength = arcLength;
    contour.frame = frame;
    contour.type = xq::ContourType::Circle;
    contour.closed = true;
    contour.points.reserve(points2d.size());
    for (std::size_t i = 0; i < points2d.size(); ++i) {
        contour.points.push_back(
            xq::XQContourGroup::unprojectFromFrame(frame, points2d[i].u, points2d[i].v));
    }
    return contour;
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

int main()
{
    // --- Source path binding -------------------------------------------------
    xq::XQContourGroup group;
    group.setId(xq::ContourGroupId(1));
    const xq::NodeId pathNode(42);
    group.setSourcePathNode(pathNode);
    CHECK(group.hasSourcePathNode());
    CHECK(group.sourcePathNode() == pathNode);

    // --- Ordered by path position (added out of order) -----------------------
    group.addContour(makeCircleContour(xq::NodeId(101), 2.0, {0.0, 0.0, 2.0}, 3.0));
    group.addContour(makeCircleContour(xq::NodeId(102), 0.5, {0.0, 0.0, 0.5}, 3.0));
    group.addContour(makeCircleContour(xq::NodeId(103), 1.0, {0.0, 0.0, 1.0}, 3.0));

    const std::vector<xq::XQContour> ordered = group.orderedByPathPosition();
    CHECK(ordered.size() == 3);
    CHECK(ordered[0].pathArcLength < ordered[1].pathArcLength);
    CHECK(ordered[1].pathArcLength < ordered[2].pathArcLength);
    CHECK(std::abs(ordered[0].pathArcLength - 0.5) < 1e-9);
    CHECK(std::abs(ordered[1].pathArcLength - 1.0) < 1e-9);
    CHECK(std::abs(ordered[2].pathArcLength - 2.0) < 1e-9);

    // --- End-to-end: hand-drawn circles feed the loft chain ------------------
    xq::ContourLoftInputBuilder::Options options;
    const xq::ContourLoftInputBuilder::Result loft =
        xq::ContourLoftInputBuilder::buildLoftInput(group, options);
    CHECK(loft.status == xq::ContourLoftInputBuilder::Status::Ok);
    // Geometry conservation: every ordered contour became a ring, each ring has
    // the resampled point count, and it matches the largest input loop (36).
    CHECK(loft.input.rings.size() == 3);
    CHECK(loft.input.pointsPerContour >= 3);
    CHECK(loft.input.pointsPerContour == 36);
    for (std::size_t i = 0; i < loft.input.rings.size(); ++i) {
        CHECK(loft.input.rings[i].points.size() == loft.input.pointsPerContour);
    }

    // --- A single contour is not loftable ------------------------------------
    xq::XQContourGroup single;
    single.setId(xq::ContourGroupId(2));
    single.addContour(makeCircleContour(xq::NodeId(201), 0.0, {0.0, 0.0, 0.0}, 3.0));
    const xq::ContourLoftInputBuilder::Result loftSingle =
        xq::ContourLoftInputBuilder::buildLoftInput(single, options);
    CHECK(loftSingle.status == xq::ContourLoftInputBuilder::Status::NotEnoughContours);

    return 0;
}
