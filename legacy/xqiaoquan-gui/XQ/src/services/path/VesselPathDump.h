#ifndef XQ_SERVICES_PATH_VESSEL_PATH_DUMP_H
#define XQ_SERVICES_PATH_VESSEL_PATH_DUMP_H

#include "core/XQVesselPath.h"

#include <string>

namespace xq {

class VesselPathDump {
public:
    enum class Status {
        Ok,
        InvalidPath
    };

    struct Result {
        Status status = Status::InvalidPath;
        VesselPathValidationResult validation;
        std::string text;

        bool ok() const
        {
            return status == Status::Ok && !text.empty();
        }
    };

    static Result format(const VesselPathV1& path);
};

} // namespace xq

#endif // XQ_SERVICES_PATH_VESSEL_PATH_DUMP_H
