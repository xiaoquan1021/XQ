#ifndef XQ_SERVICES_PATH_CENTERLINE_FRAME_SERVICE_H
#define XQ_SERVICES_PATH_CENTERLINE_FRAME_SERVICE_H

#include "core/XQPath.h"

#include <vector>

namespace xq {

// Computes the sequence of local frames (origin/tangent/normal/binormal/
// arcLength) along a path's centerline. This is the single source of path
// frames for contour placement, oblique slicing, and lofting; nothing else
// recomputes them.
//
// The normal does not flip along the path: frames come from XQPath::resample,
// which carries the normal forward with rotation-minimizing (parallel) transport
// rather than recomputing it per segment.
class CenterlineFrameService {
public:
    enum class Status {
        Ok,
        NotEnoughControlPoints, // path has fewer than 2 control points
        InvalidSpacing,         // spacing must be > 0
        ResampleFailed,         // XQPath::resample reported a failure
    };

    struct Result {
        Status status;
        std::vector<PathFrame> frames;

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    // Resamples the path at the given spacing and returns one frame per sample
    // point, in arc-length order. Does not mutate the input path (works on a copy).
    static Result computeFrames(const XQPath& path, double spacing);
};

} // namespace xq

#endif // XQ_SERVICES_PATH_CENTERLINE_FRAME_SERVICE_H
