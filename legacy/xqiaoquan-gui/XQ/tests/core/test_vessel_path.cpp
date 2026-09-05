#include <core/XQVesselPath.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                     \
    do {                                      \
        if (!(expression)) {                  \
            return fail(#expression, __LINE__); \
        }                                     \
    } while (0)

xq::VesselPathV1 valid_path()
{
    xq::VesselPathV1 path;
    path.coordinateSystem = xq::VesselPathCoordinateSystem::LPS;
    path.lengthUnit = xq::VesselPathLengthUnit::Millimeter;
    path.radiusDefinition =
        xq::VesselPathRadiusDefinition::EquivalentCircularArea;
    path.frameOfReferenceId = "1.2.840.path.frame";
    path.source.kind = xq::VesselPathSourceKind::SemiAutomatic;
    path.derivationStamp.algorithmId = "test-path";
    path.derivationStamp.algorithmVersion = "1";
    path.derivationStamp.inputs.push_back(
        {xq::NodeId(10), 7, std::nullopt, std::string()});
    path.stations = {
        {xq::VesselPathStationId(1), {0.0, 0.0, 0.0}, 1.0, 0.0},
        {xq::VesselPathStationId(2), {1.0, 0.0, 0.0}, 2.0, 1.0},
        {xq::VesselPathStationId(3), {2.0, 0.0, 0.0}, 3.0, 2.0},
    };
    return path;
}

bool rejects(xq::VesselPathV1 path, xq::VesselPathValidationCode code)
{
    const xq::VesselPathValidationResult result =
        xq::VesselPathValidator::validate(path);
    return !result.ok() && result.hasIssue(code);
}

} // namespace

int main()
{
    const xq::VesselPathV1 valid = valid_path();
    CHECK(xq::VesselPathValidator::validate(valid).ok());
    CHECK(std::string(xq::vesselPathSourceToken(valid.source.kind))
          == "semi_automatic");
    CHECK(std::string(xq::vesselPathRadiusDefinitionToken(valid.radiusDefinition))
          == "equivalent_circular_radius_from_area");

    xq::VesselPathV1 equalArc = valid;
    equalArc.stations[2].arcLengthMm = equalArc.stations[1].arcLengthMm;
    CHECK(xq::VesselPathValidator::validate(equalArc).ok());

    xq::VesselPathV1 changed = valid;
    changed.contractVersion = 2;
    CHECK(rejects(changed, xq::VesselPathValidationCode::UnsupportedContractVersion));
    changed = valid;
    changed.coordinateSystem = xq::VesselPathCoordinateSystem::Unknown;
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidCoordinateSystem));
    changed = valid;
    changed.lengthUnit = xq::VesselPathLengthUnit::Unknown;
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidLengthUnit));
    changed = valid;
    changed.radiusDefinition = xq::VesselPathRadiusDefinition::Unknown;
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidRadiusDefinition));
    changed = valid;
    changed.frameOfReferenceId.clear();
    CHECK(rejects(changed, xq::VesselPathValidationCode::MissingFrameOfReference));
    changed = valid;
    changed.source.kind = xq::VesselPathSourceKind::Unknown;
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidSourceKind));
    changed = valid;
    changed.derivationStamp.algorithmId.clear();
    CHECK(rejects(changed, xq::VesselPathValidationCode::MissingDerivationAlgorithmId));
    changed = valid;
    changed.derivationStamp.algorithmVersion.clear();
    CHECK(rejects(changed, xq::VesselPathValidationCode::MissingDerivationAlgorithmVersion));
    changed = valid;
    changed.derivationStamp.inputs.clear();
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidProfileInputCount));
    changed = valid;
    changed.derivationStamp.inputs[0].nodeId = xq::NodeId::invalid();
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidProfileInputNode));
    changed = valid;
    changed.derivationStamp.inputs[0].assetId = xq::AssetId::invalid();
    changed.derivationStamp.inputs[0].assetFingerprint = "sha256:invalid-asset";
    CHECK(rejects(
        changed,
        xq::VesselPathValidationCode::InvalidProfileInputAsset));
    changed = valid;
    changed.derivationStamp.inputs[0].assetId = xq::AssetId(12);
    CHECK(rejects(
        changed,
        xq::VesselPathValidationCode::MissingProfileInputAssetFingerprint));
    changed = valid;
    changed.derivationStamp.inputs[0].assetFingerprint = "sha256:unexpected";
    CHECK(rejects(
        changed,
        xq::VesselPathValidationCode::UnexpectedProfileInputAssetFingerprint));
    changed = valid;
    changed.stations.resize(1);
    CHECK(rejects(changed, xq::VesselPathValidationCode::TooFewStations));
    changed = valid;
    changed.stations[0].stationId = xq::VesselPathStationId::invalid();
    CHECK(rejects(changed, xq::VesselPathValidationCode::InvalidStationId));
    changed = valid;
    changed.stations[1].stationId = changed.stations[0].stationId;
    CHECK(rejects(changed, xq::VesselPathValidationCode::DuplicateStationId));
    changed = valid;
    changed.stations[1].positionMm.x =
        std::numeric_limits<double>::quiet_NaN();
    CHECK(rejects(changed, xq::VesselPathValidationCode::NonFinitePosition));
    changed = valid;
    changed.stations[1].radiusMm =
        std::numeric_limits<double>::infinity();
    CHECK(rejects(changed, xq::VesselPathValidationCode::NonFiniteRadius));
    changed = valid;
    changed.stations[1].radiusMm = 0.0;
    CHECK(rejects(changed, xq::VesselPathValidationCode::NonPositiveRadius));
    changed = valid;
    changed.stations[1].arcLengthMm =
        std::numeric_limits<double>::quiet_NaN();
    CHECK(rejects(changed, xq::VesselPathValidationCode::NonFiniteArcLength));
    changed = valid;
    changed.stations[2].arcLengthMm = 0.5;
    CHECK(rejects(changed, xq::VesselPathValidationCode::DecreasingArcLength));

    std::printf("VesselPathV1 contract checks passed\n");
    return 0;
}
