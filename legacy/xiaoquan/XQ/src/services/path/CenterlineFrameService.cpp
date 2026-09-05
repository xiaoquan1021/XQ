#include "services/path/CenterlineFrameService.h"

#include <cstddef>
#include <vector>

namespace xq {

CenterlineFrameService::Result CenterlineFrameService::computeFrames(const XQPath& path,
                                                                     double spacing)
{
    Result result;
    result.status = Status::Ok;

    XQPath working = path; // work on a copy; never mutate the caller's path
    const XQPath::ResampleStatus resample_status = working.resample(spacing);
    switch (resample_status) {
    case XQPath::ResampleStatus::Ok:
        break;
    case XQPath::ResampleStatus::NotEnoughPoints:
        result.status = Status::NotEnoughControlPoints;
        return result;
    case XQPath::ResampleStatus::InvalidSpacing:
        result.status = Status::InvalidSpacing;
        return result;
    }

    const std::vector<PathSamplePoint>& samples = working.samplePoints();
    if (samples.empty()) {
        result.status = Status::ResampleFailed;
        return result;
    }

    // One frame per sample point, taken at the sample's arc length. Reusing
    // frameAtArcLength keeps frame construction (orthonormalization) in one place
    // and inherits the resample's rotation-minimizing normal transport, so the
    // normal stays continuous between adjacent frames.
    result.frames.reserve(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        PathFrame frame = {};
        const XQPath::FrameStatus frame_status =
            working.frameAtArcLength(samples[i].arcLength, &frame);
        if (frame_status != XQPath::FrameStatus::Ok) {
            result.status = Status::ResampleFailed;
            result.frames.clear();
            return result;
        }
        result.frames.push_back(frame);
    }

    return result;
}

} // namespace xq
