#include "services/profile/VesselPathSnapshotService.h"

#include <cmath>
#include <utility>

namespace xq {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

bool valid_source_stamp(const DerivationInputStamp& stamp)
{
    return stamp.nodeId.is_valid()
        && (!stamp.assetId.has_value() || stamp.assetId->is_valid())
        && ((!stamp.assetId.has_value() && stamp.assetFingerprint.empty())
            || (stamp.assetId.has_value() && !stamp.assetFingerprint.empty()));
}

VesselPathSourceKind source_from_profile(const VesselProfileV1& profile)
{
    const VesselEvidenceKind first = profile.samples.front().evidenceKind;
    bool allSame = true;
    for (const VesselProfileSample& sample : profile.samples) {
        allSame = allSame && sample.evidenceKind == first;
    }
    if (!allSame || first == VesselEvidenceKind::MeasuredContour) {
        return VesselPathSourceKind::SemiAutomatic;
    }
    if (first == VesselEvidenceKind::SegmentationDerived) {
        return VesselPathSourceKind::AutomaticCenterlineB;
    }
    if (first == VesselEvidenceKind::ImportedGold) {
        return VesselPathSourceKind::GoldFile;
    }
    return VesselPathSourceKind::Unknown;
}

} // namespace

VesselPathSnapshotService::Result VesselPathSnapshotService::build(
    const DerivationInputStamp& sourceProfile,
    const VesselProfileV1& profile)
{
    Result result;
    if (!valid_source_stamp(sourceProfile)) {
        result.status = Status::InvalidSourceStamp;
        return result;
    }

    result.profileValidation = VesselProfileValidator::validate(profile);
    if (!result.profileValidation.ok()) {
        result.status = Status::InvalidProfile;
        return result;
    }

    VesselPathV1 path;
    path.coordinateSystem = VesselPathCoordinateSystem::LPS;
    path.lengthUnit = VesselPathLengthUnit::Millimeter;
    path.radiusDefinition =
        VesselPathRadiusDefinition::EquivalentCircularArea;
    path.frameOfReferenceId = profile.frameOfReferenceId;
    path.source.kind = source_from_profile(profile);
    path.derivationStamp.algorithmId = AlgorithmId;
    path.derivationStamp.algorithmVersion = AlgorithmVersion;
    path.derivationStamp.parameterSummary =
        "radius=equivalent-circular-area;source=min-claim-v1";
    path.derivationStamp.inputs.push_back(sourceProfile);
    path.stations.reserve(profile.samples.size());

    for (const VesselProfileSample& sample : profile.samples) {
        VesselPathStationV1 station;
        station.stationId = VesselPathStationId(sample.sampleId.value());
        station.positionMm = sample.positionMm;
        station.radiusMm = std::sqrt(sample.areaMm2 / kPi);
        station.arcLengthMm = sample.arcLengthMm;
        path.stations.push_back(station);
    }

    result.pathValidation = VesselPathValidator::validate(path);
    if (!result.pathValidation.ok()) {
        result.status = Status::PathValidationFailed;
        return result;
    }

    result.status = Status::Ok;
    result.path = std::move(path);
    return result;
}

} // namespace xq
