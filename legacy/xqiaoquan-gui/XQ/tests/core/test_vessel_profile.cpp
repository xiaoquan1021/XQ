#include <core/XQDomainType.h>
#include <core/XQScene.h>
#include <core/XQVesselProfile.h>
#include <core/XQVesselProfilePayload.h>
#include <core/asset/AssetRecord.h>

#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

xq::DerivationStamp make_stamp()
{
    xq::DerivationStamp stamp;
    stamp.algorithmId = "xq.vessel-profile.fixture";
    stamp.algorithmVersion = "1.0";
    stamp.parameterSummary = "mode=test";
    stamp.randomSeed = 42;
    stamp.inputs.push_back({xq::NodeId(10), 3, std::nullopt, ""});
    return stamp;
}

xq::VesselProfileV1 make_measured_profile()
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "frame-1";
    profile.sourcePathNode = xq::NodeId(10);
    profile.sourceEvidenceNodes = {xq::NodeId(20), xq::NodeId(21), xq::NodeId(22)};
    profile.derivationStamp = make_stamp();

    for (std::size_t i = 0; i < 3; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(static_cast<unsigned long long>(100 + i));
        sample.arcLengthMm = static_cast<double>(i) * 5.0;
        sample.positionMm = {static_cast<double>(i) * 5.0, 2.0, 3.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 12.0 + static_cast<double>(i);
        sample.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
        sample.quality = i == 1
            ? xq::VesselSampleQuality::ReviewRequired
            : xq::VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = profile.sourceEvidenceNodes[i];
        profile.samples.push_back(sample);
    }
    return profile;
}

xq::VesselProfileV1 make_imported_profile()
{
    xq::VesselProfileV1 profile = make_measured_profile();
    profile.sourceEvidenceNodes.clear();
    profile.externalEvidenceId = "gold-profile-001";
    profile.externalEvidenceFingerprint = "sha256:abc123";
    for (xq::VesselProfileSample& sample : profile.samples) {
        sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
        sample.sourceEvidenceNode = xq::NodeId::invalid();
    }
    return profile;
}

xq::VesselProfileV1 make_segmentation_profile()
{
    xq::VesselProfileV1 profile = make_measured_profile();
    for (xq::VesselProfileSample& sample : profile.samples) {
        sample.evidenceKind = xq::VesselEvidenceKind::SegmentationDerived;
    }
    return profile;
}

bool has_issue(const xq::VesselProfileV1& profile, xq::VesselProfileValidationCode code)
{
    return xq::VesselProfileValidator::validate(profile).hasIssue(code);
}

#define EXPECT_ISSUE(profile, code, label) \
    do { \
        if (!has_issue((profile), (code))) { \
            return fail((label), __LINE__); \
        } \
    } while (false)

} // namespace

int main()
{
    static_assert(
        std::is_const_v<std::remove_reference_t<decltype(
            std::declval<xq::XQVesselProfilePayload&>().profile())>>,
        "VesselProfile payload must not expose mutable solver geometry");

    if (xq::VesselSampleId().is_valid()
        || xq::VesselSampleId::invalid().is_valid()
        || !(xq::VesselSampleId(1) < xq::VesselSampleId(2))
        || xq::VesselSampleId(7) != xq::VesselSampleId(7)) {
        return fail("VesselSampleId strong identity semantics", __LINE__);
    }
    const xq::VesselSampleId sampleId(42);
    xq::VesselSampleId parsedSampleId(7);
    if (sampleId.serialize() != "42"
        || !xq::VesselSampleId::parse(sampleId.serialize(), &parsedSampleId)
        || parsedSampleId != sampleId
        || xq::VesselSampleId::parse("42x", &parsedSampleId)
        || parsedSampleId != sampleId
        || xq::VesselSampleId::parse("42", nullptr)) {
        return fail("VesselSampleId strict stable serialization", __LINE__);
    }

    const xq::VesselProfileV1 measured = make_measured_profile();
    const xq::VesselProfileValidationResult measuredResult =
        xq::VesselProfileValidator::validate(measured);
    if (!measuredResult.ok() || !measuredResult.issues.empty()) {
        return fail("valid measured profile", __LINE__);
    }

    const xq::VesselProfileV1 imported = make_imported_profile();
    if (!xq::VesselProfileValidator::validate(imported).ok()) {
        return fail("valid imported-gold profile", __LINE__);
    }

    const xq::VesselProfileV1 segmentation = make_segmentation_profile();
    if (!xq::VesselProfileValidator::validate(segmentation).ok()) {
        return fail("valid segmentation-derived profile", __LINE__);
    }

    xq::VesselProfileV1 mixed = measured;
    mixed.samples[1].evidenceKind = xq::VesselEvidenceKind::SegmentationDerived;
    if (!xq::VesselProfileValidator::validate(mixed).ok()) {
        return fail("mixed contour and segmentation evidence is valid", __LINE__);
    }

    if (xq::domainTypeToString(xq::XQDomainType::VesselProfile)
            != std::string("vessel_profile")
        || xq::XQScene::groupForDomain(xq::XQDomainType::VesselProfile)
            != xq::XQScene::Group::Paths
        || static_cast<int>(xq::AssetKind::VesselProfile)
            <= static_cast<int>(xq::AssetKind::Contour)) {
        return fail("VesselProfile domain/group/AssetKind append mapping", __LINE__);
    }

    xq::XQVesselProfilePayload payload(measured);
    std::shared_ptr<xq::XQPayload> cloneBase = payload.clone();
    const auto* clone = dynamic_cast<const xq::XQVesselProfilePayload*>(cloneBase.get());
    if (clone == nullptr || clone == &payload
        || clone->domainType() != xq::XQDomainType::VesselProfile) {
        return fail("VesselProfile payload clone type", __LINE__);
    }
    xq::VesselProfileV1 editedCopy = payload.profile();
    editedCopy.samples[0].areaMm2 = 99.0;
    editedCopy.derivationStamp.algorithmId = "changed";
    if (payload.profile().samples[0].areaMm2 != 12.0
        || payload.profile().derivationStamp.algorithmId != "xq.vessel-profile.fixture"
        || clone->profile().samples[0].areaMm2 != 12.0
        || clone->profile().derivationStamp.algorithmId != "xq.vessel-profile.fixture") {
        return fail("VesselProfile payload and clone are immutable deep values", __LINE__);
    }

    xq::VesselProfileV1 bad = measured;
    bad.contractVersion = 2;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::UnsupportedContractVersion,
                 "unsupported contract version");
    bad = measured;
    bad.coordinateSystem = xq::VesselProfileCoordinateSystem::Unknown;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidCoordinateSystem,
                 "invalid coordinate system");
    bad = measured;
    bad.lengthUnit = xq::VesselProfileLengthUnit::Unknown;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidLengthUnit,
                 "invalid length unit");
    bad = measured;
    bad.areaUnit = xq::VesselProfileAreaUnit::Unknown;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidAreaUnit,
                 "invalid area unit");
    bad = measured;
    bad.frameOfReferenceId.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingFrameOfReference,
                 "missing frame");
    bad = measured;
    bad.sourcePathNode = xq::NodeId::invalid();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingSourcePath,
                 "missing source path");
    bad = measured;
    bad.derivationStamp.algorithmId.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingDerivationAlgorithmId,
                 "missing algorithm id");
    bad = measured;
    bad.derivationStamp.algorithmVersion.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingDerivationAlgorithmVersion,
                 "missing algorithm version");
    bad = measured;
    bad.derivationStamp.inputs.push_back({xq::NodeId::invalid(), 0, std::nullopt, ""});
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidDerivationInputNode,
                 "invalid derivation input");
    bad = measured;
    bad.derivationStamp.inputs.push_back(bad.derivationStamp.inputs.front());
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::DuplicateDerivationInputNode,
                 "duplicate derivation input");
    bad = measured;
    bad.sourceEvidenceNodes.push_back(xq::NodeId::invalid());
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidSourceEvidenceNode,
                 "invalid source evidence node");
    bad = measured;
    bad.sourceEvidenceNodes.push_back(bad.sourceEvidenceNodes.front());
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::DuplicateSourceEvidenceNode,
                 "duplicate source evidence node");
    bad = measured;
    bad.samples.resize(2);
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::TooFewSamples,
                 "too few samples");
    bad = measured;
    bad.samples[1].sampleId = xq::VesselSampleId::invalid();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidSampleId,
                 "invalid sample id");
    bad = measured;
    bad.samples[1].sampleId = bad.samples[0].sampleId;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::DuplicateSampleId,
                 "duplicate sample id");
    bad = measured;
    bad.samples[1].arcLengthMm = std::numeric_limits<double>::quiet_NaN();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonFiniteArcLength,
                 "non-finite arc length");
    bad = measured;
    bad.samples[1].arcLengthMm = bad.samples[0].arcLengthMm;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonIncreasingArcLength,
                 "non-increasing arc length");
    bad = measured;
    bad.samples[1].positionMm.x = std::numeric_limits<double>::infinity();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonFinitePosition,
                 "non-finite position");
    bad = measured;
    bad.samples[1].unitTangent = {0.0, 0.0, 0.0};
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonUnitTangent,
                 "zero tangent");
    bad = measured;
    bad.samples[1].unitTangent.x = std::numeric_limits<double>::infinity();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonFiniteTangent,
                 "non-finite tangent");
    bad = measured;
    bad.samples[1].areaMm2 = std::numeric_limits<double>::quiet_NaN();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonFiniteArea,
                 "non-finite area");
    bad = measured;
    bad.samples[1].areaMm2 = 0.0;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::NonPositiveArea,
                 "non-positive area");
    bad = measured;
    bad.samples[1].evidenceKind = xq::VesselEvidenceKind::Unknown;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidEvidenceKind,
                 "invalid evidence kind");
    bad = measured;
    bad.samples[1].quality = xq::VesselSampleQuality::Unknown;
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::InvalidQuality,
                 "invalid quality");
    bad = measured;
    bad.sourceEvidenceNodes.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingMeasuredEvidenceNodes,
                 "measured profile missing evidence nodes");
    bad = measured;
    bad.samples[1].sourceEvidenceNode = xq::NodeId::invalid();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingMeasuredSampleEvidence,
                 "measured sample missing evidence node");
    bad = measured;
    bad.samples[1].sourceEvidenceNode = xq::NodeId(999);
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MeasuredSampleEvidenceNotDeclared,
                 "measured sample evidence not declared");
    bad = segmentation;
    bad.sourceEvidenceNodes.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingSegmentationEvidenceNodes,
                 "segmentation profile missing evidence nodes");
    bad = segmentation;
    bad.samples[1].sourceEvidenceNode = xq::NodeId::invalid();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingSegmentationSampleEvidence,
                 "segmentation sample missing evidence node");
    bad = segmentation;
    bad.samples[1].sourceEvidenceNode = xq::NodeId(999);
    EXPECT_ISSUE(
        bad,
        xq::VesselProfileValidationCode::SegmentationSampleEvidenceNotDeclared,
        "segmentation sample evidence not declared");
    bad = measured;
    bad.externalEvidenceId = "unexpected";
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::UnexpectedExternalEvidence,
                 "measured profile has external evidence");
    bad = imported;
    bad.externalEvidenceId.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingExternalEvidenceId,
                 "imported profile missing external id");
    bad = imported;
    bad.externalEvidenceFingerprint.clear();
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::MissingExternalEvidenceFingerprint,
                 "imported profile missing fingerprint");
    bad = imported;
    bad.sourceEvidenceNodes.push_back(xq::NodeId(20));
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::UnexpectedImportedNodeEvidence,
                 "imported profile has node evidence");
    bad = imported;
    bad.samples[1].sourceEvidenceNode = xq::NodeId(20);
    EXPECT_ISSUE(bad, xq::VesselProfileValidationCode::UnexpectedImportedSampleEvidence,
                 "imported sample has node evidence");

    const xq::VesselProfileValidationResult indexedIssue =
        xq::VesselProfileValidator::validate(bad);
    bool foundIndexed = false;
    for (const xq::VesselProfileValidationIssue& issue : indexedIssue.issues) {
        if (issue.code == xq::VesselProfileValidationCode::UnexpectedImportedSampleEvidence
            && issue.hasSampleIndex && issue.sampleIndex == 1) {
            foundIndexed = true;
        }
    }
    if (!foundIndexed) {
        return fail("sample validation issue carries stable index", __LINE__);
    }

    return 0;
}
