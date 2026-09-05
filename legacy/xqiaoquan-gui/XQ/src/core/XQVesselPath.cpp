#include "core/XQVesselPath.h"

#include <cmath>
#include <set>

namespace xq {
namespace {

constexpr VesselPathStationId::ValueType kInvalidStationId = 0;

void add_path_issue(VesselPathValidationResult* result,
                    VesselPathValidationCode code)
{
    VesselPathValidationIssue issue;
    issue.code = code;
    result->issues.push_back(issue);
}

void add_station_issue(VesselPathValidationResult* result,
                       VesselPathValidationCode code,
                       std::size_t stationIndex)
{
    VesselPathValidationIssue issue;
    issue.code = code;
    issue.hasStationIndex = true;
    issue.stationIndex = stationIndex;
    result->issues.push_back(issue);
}

bool finite_point(const Point3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool valid_source(VesselPathSourceKind source)
{
    return source == VesselPathSourceKind::AutomaticCenterlineB
        || source == VesselPathSourceKind::SemiAutomatic
        || source == VesselPathSourceKind::GoldFile;
}

} // namespace

VesselPathStationId::VesselPathStationId()
    : value_(kInvalidStationId)
{
}

VesselPathStationId::VesselPathStationId(ValueType value)
    : value_(value)
{
}

VesselPathStationId VesselPathStationId::invalid()
{
    return VesselPathStationId(kInvalidStationId);
}

bool VesselPathStationId::is_valid() const
{
    return value_ != kInvalidStationId;
}

VesselPathStationId::ValueType VesselPathStationId::value() const
{
    return value_;
}

bool VesselPathStationId::operator==(const VesselPathStationId& other) const
{
    return value_ == other.value_;
}

bool VesselPathStationId::operator!=(const VesselPathStationId& other) const
{
    return !(*this == other);
}

bool VesselPathStationId::operator<(const VesselPathStationId& other) const
{
    return value_ < other.value_;
}

const char* vesselPathRadiusDefinitionToken(VesselPathRadiusDefinition value)
{
    switch (value) {
    case VesselPathRadiusDefinition::EquivalentCircularArea:
        return "equivalent_circular_radius_from_area";
    case VesselPathRadiusDefinition::Unknown:
        break;
    }
    return "unknown";
}

const char* vesselPathSourceToken(VesselPathSourceKind value)
{
    switch (value) {
    case VesselPathSourceKind::AutomaticCenterlineB:
        return "automatic_centerline_b";
    case VesselPathSourceKind::SemiAutomatic:
        return "semi_automatic";
    case VesselPathSourceKind::GoldFile:
        return "gold_file";
    case VesselPathSourceKind::Unknown:
        break;
    }
    return "unknown";
}

bool VesselPathValidationResult::ok() const
{
    return issues.empty();
}

bool VesselPathValidationResult::hasIssue(VesselPathValidationCode code) const
{
    for (const VesselPathValidationIssue& issue : issues) {
        if (issue.code == code) {
            return true;
        }
    }
    return false;
}

VesselPathValidationResult VesselPathValidator::validate(
    const VesselPathV1& path)
{
    VesselPathValidationResult result;

    if (path.contractVersion != VesselPathV1::ContractVersion) {
        add_path_issue(
            &result, VesselPathValidationCode::UnsupportedContractVersion);
    }
    if (path.coordinateSystem != VesselPathCoordinateSystem::LPS) {
        add_path_issue(&result, VesselPathValidationCode::InvalidCoordinateSystem);
    }
    if (path.lengthUnit != VesselPathLengthUnit::Millimeter) {
        add_path_issue(&result, VesselPathValidationCode::InvalidLengthUnit);
    }
    if (path.radiusDefinition
        != VesselPathRadiusDefinition::EquivalentCircularArea) {
        add_path_issue(&result, VesselPathValidationCode::InvalidRadiusDefinition);
    }
    if (path.frameOfReferenceId.empty()) {
        add_path_issue(&result, VesselPathValidationCode::MissingFrameOfReference);
    }
    if (!valid_source(path.source.kind)) {
        add_path_issue(&result, VesselPathValidationCode::InvalidSourceKind);
    }
    if (path.derivationStamp.algorithmId.empty()) {
        add_path_issue(
            &result, VesselPathValidationCode::MissingDerivationAlgorithmId);
    }
    if (path.derivationStamp.algorithmVersion.empty()) {
        add_path_issue(
            &result, VesselPathValidationCode::MissingDerivationAlgorithmVersion);
    }
    if (path.derivationStamp.inputs.size() != 1) {
        add_path_issue(&result, VesselPathValidationCode::InvalidProfileInputCount);
    } else {
        const DerivationInputStamp& input = path.derivationStamp.inputs.front();
        if (!input.nodeId.is_valid()) {
            add_path_issue(&result, VesselPathValidationCode::InvalidProfileInputNode);
        }
        if (input.assetId.has_value() && !input.assetId->is_valid()) {
            add_path_issue(
                &result, VesselPathValidationCode::InvalidProfileInputAsset);
        }
        if (input.assetId.has_value() && input.assetFingerprint.empty()) {
            add_path_issue(
                &result,
                VesselPathValidationCode::MissingProfileInputAssetFingerprint);
        }
        if (!input.assetId.has_value() && !input.assetFingerprint.empty()) {
            add_path_issue(
                &result,
                VesselPathValidationCode::UnexpectedProfileInputAssetFingerprint);
        }
    }

    if (path.stations.size() < 2) {
        add_path_issue(&result, VesselPathValidationCode::TooFewStations);
    }

    std::set<VesselPathStationId> stationIds;
    double previousArc = 0.0;
    bool hasPreviousFiniteArc = false;
    for (std::size_t i = 0; i < path.stations.size(); ++i) {
        const VesselPathStationV1& station = path.stations[i];
        if (!station.stationId.is_valid()) {
            add_station_issue(
                &result, VesselPathValidationCode::InvalidStationId, i);
        } else if (!stationIds.insert(station.stationId).second) {
            add_station_issue(
                &result, VesselPathValidationCode::DuplicateStationId, i);
        }
        if (!finite_point(station.positionMm)) {
            add_station_issue(
                &result, VesselPathValidationCode::NonFinitePosition, i);
        }
        if (!std::isfinite(station.radiusMm)) {
            add_station_issue(
                &result, VesselPathValidationCode::NonFiniteRadius, i);
        } else if (!(station.radiusMm > 0.0)) {
            add_station_issue(
                &result, VesselPathValidationCode::NonPositiveRadius, i);
        }
        if (!std::isfinite(station.arcLengthMm)) {
            add_station_issue(
                &result, VesselPathValidationCode::NonFiniteArcLength, i);
        } else {
            if (hasPreviousFiniteArc && station.arcLengthMm < previousArc) {
                add_station_issue(
                    &result, VesselPathValidationCode::DecreasingArcLength, i);
            }
            previousArc = station.arcLengthMm;
            hasPreviousFiniteArc = true;
        }
    }

    return result;
}

} // namespace xq
