#include "services/modeling/ContourLoftInputBuilder.h"

#include "core/GeometryTypes.h"
#include "core/XQContourGroup.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace xq {
namespace {

ContourLoftInputBuilder::Result fail(ContourLoftInputBuilder::Status status)
{
    ContourLoftInputBuilder::Result result;
    result.status = status;
    return result;
}

// Counts distinct consecutive points (a coarse degeneracy guard: a contour that
// is all one point cannot be lofted).
std::size_t distinctPointCount(const std::vector<Point3>& points)
{
    if (points.empty()) {
        return 0;
    }
    std::size_t distinct = 1;
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (distance(points[i], points[i - 1]) > 1e-9) {
            ++distinct;
        }
    }
    return distinct;
}

// Resamples a closed polygon to exactly `count` points evenly spaced by arc
// length around its perimeter. The polygon is treated as closed: the segment
// from the last input point back to the first is included. Returns `count`
// points; the result is a fresh start-at-original-vertex-0 loop.
std::vector<Point3> resampleClosed(const std::vector<Point3>& in, std::size_t count)
{
    std::vector<Point3> out;
    out.reserve(count);

    // Cumulative arc length around the closed loop.
    const std::size_t n = in.size();
    std::vector<double> cumulative(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const Point3& a = in[i];
        const Point3& b = in[(i + 1) % n];
        cumulative[i + 1] = cumulative[i] + distance(a, b);
    }
    const double perimeter = cumulative[n];
    if (perimeter <= 0.0) {
        // Degenerate: emit copies of the first point (caller rejects via guard).
        out.assign(count, in.empty() ? Point3{0.0, 0.0, 0.0} : in[0]);
        return out;
    }

    const double step = perimeter / static_cast<double>(count);
    std::size_t seg = 0;
    for (std::size_t k = 0; k < count; ++k) {
        const double target = step * static_cast<double>(k);
        while (seg < n && cumulative[seg + 1] < target) {
            ++seg;
        }
        if (seg >= n) {
            seg = n - 1;
        }
        const double segStart = cumulative[seg];
        const double segLen = cumulative[seg + 1] - segStart;
        const double t = segLen > 0.0 ? (target - segStart) / segLen : 0.0;
        const Point3& a = in[seg];
        const Point3& b = in[(seg + 1) % n];
        out.push_back(add(a, scale(sub(b, a), t)));
    }
    return out;
}

// Total squared distance between two equal-length rings when `next` is read
// starting at offset `shift` and walked in direction `dir` (+1 / -1).
double matchCost(const std::vector<Point3>& ref,
                 const std::vector<Point3>& next,
                 std::size_t shift,
                 int dir)
{
    const std::size_t n = ref.size();
    double cost = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t j;
        if (dir > 0) {
            j = (shift + i) % n;
        } else {
            j = (shift + n - i) % n;
        }
        const Vec3 d = sub(ref[i], next[j]);
        cost += dot(d, d);
    }
    return cost;
}

// Returns `next` rotated (and possibly reversed) so its point order best matches
// `ref` point-for-point, minimizing total squared distance. This is the
// anti-twist alignment: without it, adjacent rings stitched index-to-index can
// spiral and self-intersect.
std::vector<Point3> alignToRef(const std::vector<Point3>& ref,
                               const std::vector<Point3>& next)
{
    const std::size_t n = ref.size();
    double bestCost = std::numeric_limits<double>::max();
    std::size_t bestShift = 0;
    int bestDir = 1;
    for (int dir = 1; dir >= -1; dir -= 2) {
        for (std::size_t shift = 0; shift < n; ++shift) {
            const double cost = matchCost(ref, next, shift, dir);
            if (cost < bestCost) {
                bestCost = cost;
                bestShift = shift;
                bestDir = dir;
            }
        }
    }

    std::vector<Point3> aligned;
    aligned.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t j;
        if (bestDir > 0) {
            j = (bestShift + i) % n;
        } else {
            j = (bestShift + n - i) % n;
        }
        aligned.push_back(next[j]);
    }
    return aligned;
}

} // namespace

ContourLoftInputBuilder::Result ContourLoftInputBuilder::buildLoftInput(
    const XQContourGroup& group, const Options& options)
{
    const std::vector<XQContour> ordered = group.orderedByPathPosition();

    // Keep only contours with usable point loops.
    std::vector<const XQContour*> usable;
    std::size_t maxPoints = 0;
    for (const XQContour& c : ordered) {
        if (distinctPointCount(c.points) >= 3) {
            usable.push_back(&c);
            if (c.points.size() > maxPoints) {
                maxPoints = c.points.size();
            }
        }
    }
    if (usable.size() < 2) {
        return fail(Status::NotEnoughContours);
    }

    std::size_t count = options.pointsPerContour;
    if (count == 0) {
        count = maxPoints;
    }
    if (count < 3) {
        return fail(Status::InvalidSampleCount);
    }

    // Resample each contour to a uniform count, then align each ring to its
    // predecessor so corresponding indices stay across neighbours (anti-twist).
    Result result;
    result.status = Status::Ok;
    result.input.sourceContourGroup = group.id();
    result.input.pointsPerContour = count;
    result.input.rings.reserve(usable.size());

    for (std::size_t i = 0; i < usable.size(); ++i) {
        const XQContour& c = *usable[i];
        std::vector<Point3> ring = resampleClosed(c.points, count);
        if (ring.size() != count) {
            return fail(Status::DegenerateContour);
        }
        if (i > 0) {
            ring = alignToRef(result.input.rings[i - 1].points, ring);
        }
        XQLoftRing loftRing;
        loftRing.contourId = c.contourId;
        loftRing.pathArcLength = c.pathArcLength;
        loftRing.points = std::move(ring);
        result.input.rings.push_back(std::move(loftRing));
    }

    return result;
}

} // namespace xq
