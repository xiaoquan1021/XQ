#ifndef XQ_SERVICES_MODELING_CONTOUR_LOFT_INPUT_BUILDER_H
#define XQ_SERVICES_MODELING_CONTOUR_LOFT_INPUT_BUILDER_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"

#include <cstddef>
#include <vector>

namespace xq {

class XQContourGroup;

// One lofting ring: a fixed-count, ordered, start-aligned point loop sitting in
// the contour's frame. All rings in an XQContourLoftInput share the same point
// count and consistent winding/start so adjacent rings can be stitched into a
// non-twisting triangle strip.
struct XQLoftRing {
    NodeId contourId;
    double pathArcLength;
    std::vector<Point3> points; // size == XQContourLoftInput::pointsPerContour
};

// Prepared input for ModelingService::loftSurface: ordered rings, each
// resampled to a uniform point count and rotated so corresponding points line
// up across neighbours.
struct XQContourLoftInput {
    NodeId sourceContourGroup;
    std::size_t pointsPerContour = 0;
    std::vector<XQLoftRing> rings; // ordered by path position, size >= 2
};

// Builds an XQContourLoftInput from a contour group, following the SimVascular
// loft preparation: order by path position, resample each contour to a uniform
// point count by arc length, then rotate each ring's start so corresponding
// points across neighbours minimize total squared distance (anti-twist).
//
// Pure domain code: depends only on xq_core, returns a status + value (no throw).
class ContourLoftInputBuilder {
public:
    enum class Status {
        Ok,
        NotEnoughContours,    // fewer than 2 contours with usable points
        InvalidSampleCount,   // requested points-per-contour < 3
        DegenerateContour,    // a contour has fewer than 3 distinct points
    };

    struct Options {
        // Target point count per ring. 0 means "use the largest contour's point
        // count" (clamped to >= 3).
        std::size_t pointsPerContour = 0;
    };

    struct Result {
        Status status;
        XQContourLoftInput input;

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    static Result buildLoftInput(const XQContourGroup& group, const Options& options);
};

} // namespace xq

#endif // XQ_SERVICES_MODELING_CONTOUR_LOFT_INPUT_BUILDER_H
