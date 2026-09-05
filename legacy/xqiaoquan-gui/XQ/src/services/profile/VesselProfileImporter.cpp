#include "services/profile/VesselProfileImporter.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace xq {
namespace {

struct Conversion {
    double lengthToMillimeter = 0.0;
    double areaToSquareMillimeter = 0.0;
    const char* lengthToken = "unknown";
    const char* areaToken = "unknown";
};

bool conversion_for(VesselProfileImporter::LengthUnit lengthUnit,
                    VesselProfileImporter::AreaUnit areaUnit,
                    Conversion* out)
{
    if (out == nullptr) {
        return false;
    }
    if (lengthUnit == VesselProfileImporter::LengthUnit::Millimeter
        && areaUnit == VesselProfileImporter::AreaUnit::SquareMillimeter) {
        out->lengthToMillimeter = 1.0;
        out->areaToSquareMillimeter = 1.0;
        out->lengthToken = "mm";
        out->areaToken = "mm2";
        return true;
    }
    if (lengthUnit == VesselProfileImporter::LengthUnit::Centimeter
        && areaUnit == VesselProfileImporter::AreaUnit::SquareCentimeter) {
        out->lengthToMillimeter = 10.0;
        out->areaToSquareMillimeter = 100.0;
        out->lengthToken = "cm";
        out->areaToken = "cm2";
        return true;
    }
    return false;
}

std::string parameter_summary(const Conversion& conversion)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::scientific << std::setprecision(17)
           << "source_length_unit=" << conversion.lengthToken
           << ";source_area_unit=" << conversion.areaToken
           << ";length_to_mm=" << conversion.lengthToMillimeter
           << ";area_to_mm2=" << conversion.areaToSquareMillimeter;
    return stream.str();
}

} // namespace

const char* VesselProfileImporter::algorithmId()
{
    return "xq.vessel_profile.imported_gold";
}

const char* VesselProfileImporter::algorithmVersion()
{
    return "1.0.0";
}

VesselProfileImporter::Result VesselProfileImporter::importProfile(
    const Request& request)
{
    Result result;
    if (request.contractVersion != VesselProfileV1::ContractVersion) {
        result.status = Status::UnsupportedVersion;
        return result;
    }
    if (request.coordinateSystem != VesselProfileCoordinateSystem::LPS) {
        result.status = Status::InvalidCoordinateSystem;
        return result;
    }

    Conversion conversion;
    if (!conversion_for(request.lengthUnit, request.areaUnit, &conversion)) {
        result.status = Status::InvalidUnits;
        return result;
    }
    if ((request.sourcePath.assetId.has_value()
         && !request.sourcePath.assetId->is_valid())
        || (request.sourcePath.assetId.has_value()
            != !request.sourcePath.assetFingerprint.empty())) {
        result.status = Status::InvalidSource;
        return result;
    }

    VesselProfileV1 profile;
    profile.contractVersion = request.contractVersion;
    profile.coordinateSystem = VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = request.frameOfReferenceId;
    profile.sourcePathNode = request.sourcePath.nodeId;
    profile.externalEvidenceId = request.externalEvidenceId;
    profile.externalEvidenceFingerprint = request.externalEvidenceFingerprint;
    profile.derivationStamp.algorithmId = algorithmId();
    profile.derivationStamp.algorithmVersion = algorithmVersion();
    profile.derivationStamp.parameterSummary = parameter_summary(conversion);
    profile.derivationStamp.inputs.push_back(request.sourcePath);
    profile.samples.reserve(request.samples.size());

    for (const Sample& source : request.samples) {
        VesselProfileSample sample;
        sample.sampleId = source.sampleId;
        sample.arcLengthMm = source.arcLength * conversion.lengthToMillimeter;
        sample.positionMm = scale(source.position, conversion.lengthToMillimeter);
        sample.unitTangent = source.unitTangent;
        sample.areaMm2 = source.area * conversion.areaToSquareMillimeter;
        sample.evidenceKind = VesselEvidenceKind::ImportedGold;
        sample.quality = source.quality;
        sample.sourceEvidenceNode = NodeId::invalid();
        profile.samples.push_back(sample);
    }

    result.validation = VesselProfileValidator::validate(profile);
    if (!result.validation.ok()) {
        result.status = Status::ProfileValidationFailed;
        return result;
    }

    result.status = Status::Ok;
    result.profile = std::move(profile);
    return result;
}

} // namespace xq
