#include <core/NodeId.h>
#include <core/XQVesselProfile.h>
#include <services/profile/VesselProfileImporter.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

bool near(double left, double right, double tolerance = 1.0e-12)
{
    return std::abs(left - right) <= tolerance;
}

xq::VesselProfileImporter::Request make_request(
    xq::VesselProfileImporter::LengthUnit lengthUnit,
    xq::VesselProfileImporter::AreaUnit areaUnit)
{
    xq::VesselProfileImporter::Request request;
    request.lengthUnit = lengthUnit;
    request.areaUnit = areaUnit;
    request.frameOfReferenceId = "1.2.840.shell.gold.frame";
    request.sourcePath.nodeId = xq::NodeId(501);
    request.sourcePath.contentRevision = 19;
    request.externalEvidenceId = "gold-profile-v1";
    request.externalEvidenceFingerprint = "sha256:gold-profile-v1";

    for (int i = 0; i < 3; ++i) {
        xq::VesselProfileImporter::Sample sample;
        sample.sampleId = xq::VesselSampleId(static_cast<unsigned long long>(601 + i));
        sample.arcLength = 2.0 + static_cast<double>(i) * 3.0;
        sample.position = {1.0, 2.0, sample.arcLength};
        sample.unitTangent = {0.0, 0.0, 1.0};
        sample.area = 0.5 + static_cast<double>(i);
        sample.quality = xq::VesselSampleQuality::Accepted;
        request.samples.push_back(sample);
    }
    return request;
}

int test_canonical_mm_import()
{
    const xq::VesselProfileImporter::Request request = make_request(
        xq::VesselProfileImporter::LengthUnit::Millimeter,
        xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
    const xq::VesselProfileImporter::Result result =
        xq::VesselProfileImporter::importProfile(request);
    CHECK(result.ok());
    const xq::VesselProfileV1& profile = result.profile.value();
    CHECK(xq::VesselProfileValidator::validate(profile).ok());
    CHECK(profile.coordinateSystem == xq::VesselProfileCoordinateSystem::LPS);
    CHECK(profile.lengthUnit == xq::VesselProfileLengthUnit::Millimeter);
    CHECK(profile.areaUnit == xq::VesselProfileAreaUnit::SquareMillimeter);
    CHECK(profile.sourcePathNode == request.sourcePath.nodeId);
    CHECK(profile.sourceEvidenceNodes.empty());
    CHECK(profile.externalEvidenceId == request.externalEvidenceId);
    CHECK(profile.externalEvidenceFingerprint
          == request.externalEvidenceFingerprint);
    CHECK(profile.derivationStamp.algorithmId
          == xq::VesselProfileImporter::algorithmId());
    CHECK(profile.derivationStamp.algorithmVersion
          == xq::VesselProfileImporter::algorithmVersion());
    CHECK(profile.derivationStamp.inputs.size() == 1);
    CHECK(profile.derivationStamp.inputs[0].contentRevision == 19);
    CHECK(profile.samples.size() == request.samples.size());
    for (std::size_t i = 0; i < profile.samples.size(); ++i) {
        CHECK(profile.samples[i].sampleId == request.samples[i].sampleId);
        CHECK(near(profile.samples[i].arcLengthMm, request.samples[i].arcLength));
        CHECK(near(profile.samples[i].positionMm.x, request.samples[i].position.x));
        CHECK(near(profile.samples[i].positionMm.y, request.samples[i].position.y));
        CHECK(near(profile.samples[i].positionMm.z, request.samples[i].position.z));
        CHECK(near(profile.samples[i].areaMm2, request.samples[i].area));
        CHECK(profile.samples[i].evidenceKind
              == xq::VesselEvidenceKind::ImportedGold);
        CHECK(!profile.samples[i].sourceEvidenceNode.is_valid());
    }
    return 0;
}

int test_legacy_cm_conversion()
{
    const xq::VesselProfileImporter::Request request = make_request(
        xq::VesselProfileImporter::LengthUnit::Centimeter,
        xq::VesselProfileImporter::AreaUnit::SquareCentimeter);
    const xq::VesselProfileImporter::Result first =
        xq::VesselProfileImporter::importProfile(request);
    const xq::VesselProfileImporter::Result second =
        xq::VesselProfileImporter::importProfile(request);
    CHECK(first.ok());
    CHECK(second.ok());
    const xq::VesselProfileV1& profile = first.profile.value();
    for (std::size_t i = 0; i < profile.samples.size(); ++i) {
        CHECK(near(profile.samples[i].arcLengthMm,
                   request.samples[i].arcLength * 10.0));
        CHECK(near(profile.samples[i].positionMm.x,
                   request.samples[i].position.x * 10.0));
        CHECK(near(profile.samples[i].positionMm.y,
                   request.samples[i].position.y * 10.0));
        CHECK(near(profile.samples[i].positionMm.z,
                   request.samples[i].position.z * 10.0));
        CHECK(near(profile.samples[i].areaMm2,
                   request.samples[i].area * 100.0));
        CHECK(second.profile->samples[i].sampleId == profile.samples[i].sampleId);
        CHECK(near(second.profile->samples[i].arcLengthMm,
                   profile.samples[i].arcLengthMm));
        CHECK(near(second.profile->samples[i].areaMm2,
                   profile.samples[i].areaMm2));
    }
    CHECK(second.profile->derivationStamp.parameterSummary
          == profile.derivationStamp.parameterSummary);
    CHECK(profile.derivationStamp.parameterSummary.find("source_length_unit=cm")
          != std::string::npos);
    CHECK(profile.derivationStamp.parameterSummary.find("source_area_unit=cm2")
          != std::string::npos);
    return 0;
}

int test_import_error_matrix()
{
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.contractVersion = 99;
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(result.status
              == xq::VesselProfileImporter::Status::UnsupportedVersion);
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.coordinateSystem = xq::VesselProfileCoordinateSystem::Unknown;
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.status
              == xq::VesselProfileImporter::Status::InvalidCoordinateSystem);
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Centimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(result.status == xq::VesselProfileImporter::Status::InvalidUnits);
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Unknown,
            xq::VesselProfileImporter::AreaUnit::Unknown);
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.status == xq::VesselProfileImporter::Status::InvalidUnits);
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.sourcePath.assetId = xq::AssetId::invalid();
        request.sourcePath.assetFingerprint = "sha256:invalid-asset-id";
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(result.status == xq::VesselProfileImporter::Status::InvalidSource);
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.sourcePath.nodeId = xq::NodeId::invalid();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(result.status
              == xq::VesselProfileImporter::Status::ProfileValidationFailed);
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::MissingSourcePath));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.frameOfReferenceId.clear();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::MissingFrameOfReference));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.externalEvidenceId.clear();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::MissingExternalEvidenceId));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.externalEvidenceFingerprint.clear();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::MissingExternalEvidenceFingerprint));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.samples.pop_back();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::TooFewSamples));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.samples[1].sampleId = request.samples[0].sampleId;
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::DuplicateSampleId));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.samples[1].arcLength = request.samples[0].arcLength;
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::NonIncreasingArcLength));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.samples[1].area = std::numeric_limits<double>::infinity();
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::NonFiniteArea));
    }
    {
        xq::VesselProfileImporter::Request request = make_request(
            xq::VesselProfileImporter::LengthUnit::Millimeter,
            xq::VesselProfileImporter::AreaUnit::SquareMillimeter);
        request.samples[1].unitTangent = {0.0, 0.0, 2.0};
        const xq::VesselProfileImporter::Result result =
            xq::VesselProfileImporter::importProfile(request);
        CHECK(!result.ok());
        CHECK(result.validation.hasIssue(
            xq::VesselProfileValidationCode::NonUnitTangent));
    }
    return 0;
}

} // namespace

int main()
{
    int result = test_canonical_mm_import();
    if (result != 0) return result;
    result = test_legacy_cm_conversion();
    if (result != 0) return result;
    result = test_import_error_matrix();
    if (result != 0) return result;
    std::printf("OK: typed imported-gold vessel profile conversion\n");
    return 0;
}
