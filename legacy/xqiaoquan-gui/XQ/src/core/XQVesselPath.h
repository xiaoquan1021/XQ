#ifndef XQ_CORE_XQ_VESSEL_PATH_H
#define XQ_CORE_XQ_VESSEL_PATH_H

#include "core/GeometryTypes.h"
#include "core/XQDerivationStamp.h"

#include <cstddef>
#include <string>
#include <vector>

namespace xq {

class VesselPathStationId {
public:
    using ValueType = unsigned long long;

    VesselPathStationId();
    explicit VesselPathStationId(ValueType value);

    static VesselPathStationId invalid();

    bool is_valid() const;
    ValueType value() const;

    bool operator==(const VesselPathStationId& other) const;
    bool operator!=(const VesselPathStationId& other) const;
    bool operator<(const VesselPathStationId& other) const;

private:
    ValueType value_;
};

enum class VesselPathCoordinateSystem {
    Unknown,
    LPS
};

enum class VesselPathLengthUnit {
    Unknown,
    Millimeter
};

// Weak-radius contract for Shell A: the station radius is the radius of a
// circle with the authoritative VesselProfile cross-sectional area.
enum class VesselPathRadiusDefinition {
    Unknown,
    EquivalentCircularArea
};

enum class VesselPathSourceKind {
    Unknown,
    AutomaticCenterlineB,
    SemiAutomatic,
    GoldFile
};

const char* vesselPathRadiusDefinitionToken(VesselPathRadiusDefinition value);
const char* vesselPathSourceToken(VesselPathSourceKind value);

struct VesselPathSourceV1 {
    VesselPathSourceKind kind = VesselPathSourceKind::Unknown;
};

struct VesselPathStationV1 {
    VesselPathStationId stationId;
    Point3 positionMm{0.0, 0.0, 0.0};
    double radiusMm = 0.0;
    double arcLengthMm = 0.0;
};

// Immutable module input. This value is rebuilt from VesselProfileV1 when it
// is needed; it is deliberately not a Scene payload or a second persisted
// geometry authority.
struct VesselPathV1 {
    static constexpr unsigned int ContractVersion = 1;

    unsigned int contractVersion = ContractVersion;
    VesselPathCoordinateSystem coordinateSystem =
        VesselPathCoordinateSystem::Unknown;
    VesselPathLengthUnit lengthUnit = VesselPathLengthUnit::Unknown;
    VesselPathRadiusDefinition radiusDefinition =
        VesselPathRadiusDefinition::Unknown;
    std::string frameOfReferenceId;
    VesselPathSourceV1 source;
    DerivationStamp derivationStamp;
    std::vector<VesselPathStationV1> stations;
};

enum class VesselPathValidationCode {
    UnsupportedContractVersion,
    InvalidCoordinateSystem,
    InvalidLengthUnit,
    InvalidRadiusDefinition,
    MissingFrameOfReference,
    InvalidSourceKind,
    MissingDerivationAlgorithmId,
    MissingDerivationAlgorithmVersion,
    InvalidProfileInputCount,
    InvalidProfileInputNode,
    InvalidProfileInputAsset,
    MissingProfileInputAssetFingerprint,
    UnexpectedProfileInputAssetFingerprint,
    TooFewStations,
    InvalidStationId,
    DuplicateStationId,
    NonFinitePosition,
    NonFiniteRadius,
    NonPositiveRadius,
    NonFiniteArcLength,
    DecreasingArcLength
};

struct VesselPathValidationIssue {
    VesselPathValidationCode code =
        VesselPathValidationCode::UnsupportedContractVersion;
    bool hasStationIndex = false;
    std::size_t stationIndex = 0;
};

class VesselPathValidationResult {
public:
    bool ok() const;
    bool hasIssue(VesselPathValidationCode code) const;

    std::vector<VesselPathValidationIssue> issues;
};

class VesselPathValidator {
public:
    static VesselPathValidationResult validate(const VesselPathV1& path);
};

} // namespace xq

#endif // XQ_CORE_XQ_VESSEL_PATH_H
