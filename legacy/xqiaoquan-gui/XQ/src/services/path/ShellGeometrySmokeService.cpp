#include "services/path/ShellGeometrySmokeService.h"

#include "services/path/VesselPathDump.h"

#include <algorithm>
#include <utility>

namespace xq {

ShellGeometrySmokeService::Result ShellGeometrySmokeService::run(
    const VesselPathV1& path)
{
    Result result;
    result.validation = VesselPathValidator::validate(path);
    if (!result.validation.ok()) {
        return result;
    }

    const VesselPathDump::Result dump = VesselPathDump::format(path);
    if (!dump.ok()) {
        result.status = Status::DumpFailed;
        return result;
    }

    Summary summary;
    summary.stationCount = path.stations.size();
    summary.segmentCount = path.stations.size() - 1;
    summary.arcSpanMm = path.stations.back().arcLengthMm
        - path.stations.front().arcLengthMm;
    summary.minimumRadiusMm = path.stations.front().radiusMm;
    summary.maximumRadiusMm = path.stations.front().radiusMm;
    double meanRadiusMm = 0.0;
    for (std::size_t i = 0; i < path.stations.size(); ++i) {
        const VesselPathStationV1& station = path.stations[i];
        summary.minimumRadiusMm =
            (std::min)(summary.minimumRadiusMm, station.radiusMm);
        summary.maximumRadiusMm =
            (std::max)(summary.maximumRadiusMm, station.radiusMm);
        meanRadiusMm += (station.radiusMm - meanRadiusMm)
            / static_cast<double>(i + 1);
        if (i > 0) {
            summary.polylineLengthMm += distance(
                path.stations[i - 1].positionMm, station.positionMm);
        }
    }
    summary.meanRadiusMm = meanRadiusMm;
    summary.canonicalDump = dump.text;

    result.status = Status::Ok;
    result.summary = std::move(summary);
    return result;
}

} // namespace xq
