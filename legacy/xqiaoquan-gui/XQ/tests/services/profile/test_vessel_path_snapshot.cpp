#include <services/path/VesselPathDump.h>
#include <services/profile/VesselPathSnapshotService.h>

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

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

bool near(double left, double right)
{
    return std::abs(left - right) < 1.0e-12;
}

xq::VesselProfileV1 profile_with(
    const std::vector<xq::VesselEvidenceKind>& evidence)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "1.2.840.snapshot.frame";
    profile.sourcePathNode = xq::NodeId(10);
    profile.derivationStamp.algorithmId = "profile-source";
    profile.derivationStamp.algorithmVersion = "1";
    profile.derivationStamp.inputs.push_back(
        {xq::NodeId(10), 2, std::nullopt, std::string()});

    bool hasNodeEvidence = false;
    bool hasGold = false;
    for (std::size_t i = 0; i < evidence.size(); ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(i + 1);
        sample.arcLengthMm = static_cast<double>(i) * 5.0;
        sample.positionMm = {static_cast<double>(i) * 5.0, 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 3.14159265358979323846
            * static_cast<double>((i + 1) * (i + 1));
        sample.evidenceKind = evidence[i];
        sample.quality = xq::VesselSampleQuality::Accepted;
        if (evidence[i] == xq::VesselEvidenceKind::MeasuredContour
            || evidence[i] == xq::VesselEvidenceKind::SegmentationDerived) {
            sample.sourceEvidenceNode = xq::NodeId(20);
            hasNodeEvidence = true;
        }
        hasGold = hasGold
            || evidence[i] == xq::VesselEvidenceKind::ImportedGold;
        profile.samples.push_back(sample);
    }
    if (hasNodeEvidence) {
        profile.sourceEvidenceNodes.push_back(xq::NodeId(20));
        profile.derivationStamp.inputs.push_back(
            {xq::NodeId(20), 3, std::nullopt, std::string()});
    }
    if (hasGold) {
        profile.externalEvidenceId = "gold-file-id-not-for-path-dump";
        profile.externalEvidenceFingerprint = "sha256:gold";
    }
    return profile;
}

xq::DerivationInputStamp source_stamp()
{
    xq::DerivationInputStamp stamp;
    stamp.nodeId = xq::NodeId(100);
    stamp.contentRevision = 9;
    stamp.assetId = xq::AssetId(200);
    stamp.assetFingerprint = "sha256:profile";
    return stamp;
}

int check_source(xq::VesselEvidenceKind evidence,
                 xq::VesselPathSourceKind expected)
{
    const xq::VesselPathSnapshotService::Result result =
        xq::VesselPathSnapshotService::build(
            source_stamp(), profile_with({evidence, evidence, evidence}));
    CHECK(result.ok());
    CHECK(result.path->source.kind == expected);
    CHECK(result.path->derivationStamp.inputs.size() == 1);
    CHECK(result.path->derivationStamp.inputs[0].nodeId == xq::NodeId(100));
    CHECK(result.path->derivationStamp.inputs[0].contentRevision == 9);
    CHECK(result.path->derivationStamp.inputs[0].assetId == xq::AssetId(200));
    CHECK(result.path->derivationStamp.inputs[0].assetFingerprint
          == "sha256:profile");
    CHECK(result.path->stations.size() == 3);
    for (std::size_t i = 0; i < result.path->stations.size(); ++i) {
        CHECK(result.path->stations[i].stationId.value() == i + 1);
        CHECK(near(result.path->stations[i].radiusMm,
                   static_cast<double>(i + 1)));
    }
    return 0;
}

} // namespace

int main()
{
    CHECK(check_source(
              xq::VesselEvidenceKind::SegmentationDerived,
              xq::VesselPathSourceKind::AutomaticCenterlineB)
          == 0);
    CHECK(check_source(
              xq::VesselEvidenceKind::MeasuredContour,
              xq::VesselPathSourceKind::SemiAutomatic)
          == 0);
    CHECK(check_source(
              xq::VesselEvidenceKind::ImportedGold,
              xq::VesselPathSourceKind::GoldFile)
          == 0);

    const xq::VesselProfileV1 mixed = profile_with({
        xq::VesselEvidenceKind::SegmentationDerived,
        xq::VesselEvidenceKind::ImportedGold,
        xq::VesselEvidenceKind::MeasuredContour,
    });
    const xq::VesselPathSnapshotService::Result first =
        xq::VesselPathSnapshotService::build(source_stamp(), mixed);
    const xq::VesselPathSnapshotService::Result second =
        xq::VesselPathSnapshotService::build(source_stamp(), mixed);
    CHECK(first.ok());
    CHECK(second.ok());
    CHECK(first.path->source.kind == xq::VesselPathSourceKind::SemiAutomatic);

    const xq::VesselPathDump::Result firstDump =
        xq::VesselPathDump::format(first.path.value());
    const xq::VesselPathDump::Result secondDump =
        xq::VesselPathDump::format(second.path.value());
    CHECK(firstDump.ok());
    CHECK(firstDump.text == secondDump.text);
    CHECK(firstDump.text.find("source=semi_automatic") != std::string::npos);
    CHECK(firstDump.text.find(
              "radius_definition=equivalent_circular_radius_from_area")
          != std::string::npos);
    CHECK(firstDump.text.find("gold-file-id-not-for-path-dump")
          == std::string::npos);
    CHECK(firstDump.text.find("parameterSummary") == std::string::npos);

    xq::VesselProfileV1 invalidProfile = mixed;
    invalidProfile.samples[0].areaMm2 = 0.0;
    const xq::VesselPathSnapshotService::Result invalid =
        xq::VesselPathSnapshotService::build(source_stamp(), invalidProfile);
    CHECK(!invalid.ok());
    CHECK(invalid.status
          == xq::VesselPathSnapshotService::Status::InvalidProfile);
    CHECK(!invalid.path.has_value());

    xq::DerivationInputStamp invalidStamp = source_stamp();
    invalidStamp.assetFingerprint.clear();
    const xq::VesselPathSnapshotService::Result badStamp =
        xq::VesselPathSnapshotService::build(invalidStamp, mixed);
    CHECK(!badStamp.ok());
    CHECK(badStamp.status
          == xq::VesselPathSnapshotService::Status::InvalidSourceStamp);
    CHECK(!badStamp.path.has_value());

    invalidStamp = source_stamp();
    invalidStamp.assetId = xq::AssetId::invalid();
    const xq::VesselPathSnapshotService::Result badAssetId =
        xq::VesselPathSnapshotService::build(invalidStamp, mixed);
    CHECK(!badAssetId.ok());
    CHECK(badAssetId.status
          == xq::VesselPathSnapshotService::Status::InvalidSourceStamp);
    CHECK(!badAssetId.path.has_value());

    std::printf("VesselProfile-to-Path snapshot checks passed\n");
    return 0;
}
