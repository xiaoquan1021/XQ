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

    // One frame per sample point, copied straight from the resample's
    // rotation-minimizing transport in a single O(M) pass (the per-sample
    // frameAtArcLength loop was O(M^2): each call scans the sample list).
    const XQPath::FrameStatus frame_status = working.framesForAllSamples(&result.frames);
    if (frame_status != XQPath::FrameStatus::Ok) {
        result.status = Status::ResampleFailed;
        result.frames.clear();
        return result;
    }

    return result;
}

} // namespace xq
