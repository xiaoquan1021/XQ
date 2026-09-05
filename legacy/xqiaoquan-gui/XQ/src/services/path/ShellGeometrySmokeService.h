#ifndef XQ_SERVICES_PATH_SHELL_GEOMETRY_SMOKE_SERVICE_H
#define XQ_SERVICES_PATH_SHELL_GEOMETRY_SMOKE_SERVICE_H

#include "core/XQVesselPath.h"

#include <cstddef>
#include <string>

namespace xq {

class ShellGeometrySmokeService {
public:
    enum class Status {
        Ok,
        InvalidPath,
        DumpFailed
    };

    struct Summary {
        std::size_t stationCount = 0;
        std::size_t segmentCount = 0;
        double arcSpanMm = 0.0;
        double polylineLengthMm = 0.0;
        double minimumRadiusMm = 0.0;
        double maximumRadiusMm = 0.0;
        double meanRadiusMm = 0.0;
        std::string canonicalDump;
    };

    struct Result {
        Status status = Status::InvalidPath;
        VesselPathValidationResult validation;
        Summary summary;

        bool ok() const { return status == Status::Ok; }
    };

    static Result run(const VesselPathV1& path);
};

} // namespace xq

#endif // XQ_SERVICES_PATH_SHELL_GEOMETRY_SMOKE_SERVICE_H
