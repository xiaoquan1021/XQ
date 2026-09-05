#ifndef XQ_SERVICES_PROFILE_VESSEL_PATH_SNAPSHOT_SERVICE_H
#define XQ_SERVICES_PROFILE_VESSEL_PATH_SNAPSHOT_SERVICE_H

#include "core/XQVesselPath.h"
#include "core/XQVesselProfile.h"

#include <optional>

namespace xq {

class VesselPathSnapshotService {
public:
    static constexpr const char* AlgorithmId = "xq-vessel-path-snapshot";
    static constexpr const char* AlgorithmVersion = "1";

    enum class Status {
        Ok,
        InvalidSourceStamp,
        InvalidProfile,
        PathValidationFailed
    };

    struct Result {
        Status status = Status::InvalidSourceStamp;
        VesselProfileValidationResult profileValidation;
        VesselPathValidationResult pathValidation;
        std::optional<VesselPathV1> path;

        bool ok() const
        {
            return status == Status::Ok && path.has_value();
        }
    };

    static Result build(const DerivationInputStamp& sourceProfile,
                        const VesselProfileV1& profile);
};

} // namespace xq

#endif // XQ_SERVICES_PROFILE_VESSEL_PATH_SNAPSHOT_SERVICE_H
