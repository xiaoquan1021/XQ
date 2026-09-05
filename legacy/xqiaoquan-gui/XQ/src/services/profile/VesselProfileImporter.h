#ifndef XQ_SERVICES_PROFILE_VESSEL_PROFILE_IMPORTER_H
#define XQ_SERVICES_PROFILE_VESSEL_PROFILE_IMPORTER_H

#include "core/XQDerivationStamp.h"
#include "core/XQVesselProfile.h"

#include <optional>
#include <string>
#include <vector>

namespace xq {

// Typed, non-file-format import boundary for controlled golden/reference
// profiles. Source units are explicit and converted once to canonical LPS/mm
// before the shared VesselProfileValidator is invoked.
class VesselProfileImporter {
public:
    enum class LengthUnit {
        Unknown,
        Millimeter,
        Centimeter
    };

    enum class AreaUnit {
        Unknown,
        SquareMillimeter,
        SquareCentimeter
    };

    struct Sample {
        VesselSampleId sampleId;
        double arcLength = 0.0;
        Point3 position{0.0, 0.0, 0.0};
        Vec3 unitTangent{0.0, 0.0, 0.0};
        double area = 0.0;
        VesselSampleQuality quality = VesselSampleQuality::Accepted;
    };

    struct Request {
        unsigned int contractVersion = VesselProfileV1::ContractVersion;
        VesselProfileCoordinateSystem coordinateSystem =
            VesselProfileCoordinateSystem::LPS;
        LengthUnit lengthUnit = LengthUnit::Unknown;
        AreaUnit areaUnit = AreaUnit::Unknown;
        std::string frameOfReferenceId;
        DerivationInputStamp sourcePath;
        std::string externalEvidenceId;
        std::string externalEvidenceFingerprint;
        std::vector<Sample> samples;
    };

    enum class Status {
        Ok,
        UnsupportedVersion,
        InvalidCoordinateSystem,
        InvalidUnits,
        InvalidSource,
        ProfileValidationFailed
    };

    struct Result {
        Status status = Status::ProfileValidationFailed;
        std::optional<VesselProfileV1> profile;
        VesselProfileValidationResult validation;

        bool ok() const
        {
            return status == Status::Ok && profile.has_value() && validation.ok();
        }
    };

    static const char* algorithmId();
    static const char* algorithmVersion();
    static Result importProfile(const Request& request);
};

} // namespace xq

#endif // XQ_SERVICES_PROFILE_VESSEL_PROFILE_IMPORTER_H
