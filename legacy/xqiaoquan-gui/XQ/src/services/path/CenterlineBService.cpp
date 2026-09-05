#include "services/path/CenterlineBService.h"

#include "io/blob/Sha256.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace xq {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr char kPathFingerprintPrefix[] =
    "xq-centerline-b-path-v1:sha256:";
constexpr char kProfileFingerprintPrefix[] =
    "xq-centerline-b-profile-v1:sha256:";

bool validStamp(const DerivationInputStamp& stamp)
{
    return stamp.nodeId.is_valid()
        && (!stamp.assetId.has_value() || stamp.assetId->is_valid())
        && ((!stamp.assetId.has_value() && stamp.assetFingerprint.empty())
            || (stamp.assetId.has_value()
                && !stamp.assetFingerprint.empty()));
}

bool validRequest(const CenterlineBService::Request& request)
{
    if (!request.sourceImageNode.is_valid()
        || request.frameOfReferenceId.empty()
        || !validStamp(request.sourceMask)
        || !request.outputPathNode.is_valid()
        || !request.outputProfileNode.is_valid()
        || !request.outputPathAsset.is_valid()
        || !request.outputProfileAsset.is_valid()
        || request.sourceImageNode == request.sourceMask.nodeId
        || request.outputPathNode == request.outputProfileNode
        || request.outputPathNode == request.sourceImageNode
        || request.outputPathNode == request.sourceMask.nodeId
        || request.outputProfileNode == request.sourceImageNode
        || request.outputProfileNode == request.sourceMask.nodeId
        || request.outputPathAsset == request.outputProfileAsset
        || (request.sourceMask.assetId.has_value()
            && (request.sourceMask.assetId.value() == request.outputPathAsset
                || request.sourceMask.assetId.value()
                    == request.outputProfileAsset))) {
        return false;
    }
    return request.graphProfile.contractVersion
            == CenterlineBGraphProfileV1::ContractVersion
        && std::isfinite(request.graphProfile.shortSpurLengthMm)
        && request.graphProfile.shortSpurLengthMm >= 0.0;
}

std::string parameterSummary(
    const CenterlineSkeletonV1& skeleton,
    const CenterlineBGraphProfileV1& graphProfile)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision((std::numeric_limits<double>::max_digits10))
           << "thinning=" << skeleton.thinningBackendId
           << "@" << skeleton.thinningBackendVersion
           << ";distance=" << skeleton.distanceBackendId
           << "@" << skeleton.distanceBackendVersion
           << ";neighborhood=26"
           << ";short_spur_length_mm="
           << graphProfile.shortSpurLengthMm
           << ";main_path=endpoint-geodesic-diameter-v1";
    return output.str();
}

void writeString(std::ostringstream* output,
                 const char* field,
                 const std::string& value)
{
    *output << field << " " << value.size() << ":" << value << "\n";
}

void writeStamp(std::ostringstream* output,
                const DerivationInputStamp& stamp)
{
    *output << "input " << stamp.nodeId.value()
            << " revision " << stamp.contentRevision
            << " asset "
            << (stamp.assetId.has_value() ? stamp.assetId->value() : 0)
            << " has_asset " << (stamp.assetId.has_value() ? 1 : 0)
            << "\n";
    writeString(output, "input_fingerprint", stamp.assetFingerprint);
}

std::string pathFingerprint(const XQPath& path)
{
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::setprecision(
        (std::numeric_limits<double>::max_digits10));
    canonical << "centerline_b_path_v1\n"
              << "id " << path.id().value() << "\n"
              << "interpolation "
              << static_cast<int>(path.interpolation()) << "\n"
              << "sample_spacing_mm " << path.sampleSpacing() << "\n"
              << "has_source_image "
              << (path.hasSourceImageNode() ? 1 : 0) << "\n"
              << "source_image " << path.sourceImageNode().value() << "\n"
              << "control_points " << path.controlPoints().size() << "\n";
    for (const PathControlPoint& point : path.controlPoints()) {
        canonical << point.position.x << " " << point.position.y << " "
                  << point.position.z << "\n";
    }
    const std::string bytes = canonical.str();
    return std::string(kPathFingerprintPrefix)
        + Sha256::hashHex(bytes.data(), bytes.size());
}

std::string profileFingerprint(const VesselProfileV1& profile)
{
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::setprecision(
        (std::numeric_limits<double>::max_digits10));
    canonical << "centerline_b_profile_v1\n"
              << "version " << profile.contractVersion << "\n"
              << "coordinate "
              << static_cast<int>(profile.coordinateSystem) << "\n"
              << "length_unit " << static_cast<int>(profile.lengthUnit)
              << "\n"
              << "area_unit " << static_cast<int>(profile.areaUnit) << "\n";
    writeString(&canonical, "frame", profile.frameOfReferenceId);
    canonical << "source_path " << profile.sourcePathNode.value() << "\n"
              << "evidence_nodes " << profile.sourceEvidenceNodes.size()
              << "\n";
    for (const NodeId& node : profile.sourceEvidenceNodes) {
        canonical << node.value() << "\n";
    }
    writeString(&canonical, "external_id", profile.externalEvidenceId);
    writeString(
        &canonical, "external_fingerprint",
        profile.externalEvidenceFingerprint);
    writeString(
        &canonical, "algorithm", profile.derivationStamp.algorithmId);
    writeString(
        &canonical, "algorithm_version",
        profile.derivationStamp.algorithmVersion);
    writeString(
        &canonical, "parameters",
        profile.derivationStamp.parameterSummary);
    canonical << "has_seed "
              << (profile.derivationStamp.randomSeed.has_value() ? 1 : 0)
              << " seed "
              << profile.derivationStamp.randomSeed.value_or(0) << "\n"
              << "inputs " << profile.derivationStamp.inputs.size() << "\n";
    for (const DerivationInputStamp& input : profile.derivationStamp.inputs) {
        writeStamp(&canonical, input);
    }
    canonical << "samples " << profile.samples.size() << "\n";
    for (const VesselProfileSample& sample : profile.samples) {
        canonical << sample.sampleId.value() << " "
                  << sample.arcLengthMm << " "
                  << sample.positionMm.x << " "
                  << sample.positionMm.y << " "
                  << sample.positionMm.z << " "
                  << sample.unitTangent.x << " "
                  << sample.unitTangent.y << " "
                  << sample.unitTangent.z << " "
                  << sample.areaMm2 << " "
                  << static_cast<int>(sample.evidenceKind) << " "
                  << static_cast<int>(sample.quality) << " "
                  << sample.sourceEvidenceNode.value() << "\n";
    }
    const std::string bytes = canonical.str();
    return std::string(kProfileFingerprintPrefix)
        + Sha256::hashHex(bytes.data(), bytes.size());
}

Vec3 tangentAt(const std::vector<CenterlineBGraphSample>& samples,
               std::size_t index)
{
    if (index == 0) {
        return normalized(sub(samples[1].positionMm, samples[0].positionMm));
    }
    if (index + 1 == samples.size()) {
        return normalized(sub(samples[index].positionMm,
                              samples[index - 1].positionMm));
    }
    return normalized(sub(samples[index + 1].positionMm,
                          samples[index - 1].positionMm));
}

} // namespace

const char* centerlineBServiceStatusToken(CenterlineBService::Status status)
{
    switch (status) {
    case CenterlineBService::Status::Ok: return "ok";
    case CenterlineBService::Status::InvalidRequest: return "invalid_request";
    case CenterlineBService::Status::SkeletonizationFailed:
        return "skeletonization_failed";
    case CenterlineBService::Status::GraphFailed: return "graph_failed";
    case CenterlineBService::Status::PathAssemblyFailed:
        return "path_assembly_failed";
    case CenterlineBService::Status::ProfileValidationFailed:
        return "profile_validation_failed";
    case CenterlineBService::Status::FingerprintFailed:
        return "fingerprint_failed";
    case CenterlineBService::Status::SnapshotFailed: return "snapshot_failed";
    case CenterlineBService::Status::GeometrySmokeFailed:
        return "geometry_smoke_failed";
    }
    return "unknown";
}

CenterlineBService::Result CenterlineBService::run(
    const ICenterlineSkeletonizer3D& skeletonizer,
    const ImageGeometry& geometry,
    const IVoxelSource& source,
    const Request& request)
{
    Result result;
    if (!validRequest(request)) {
        return result;
    }

    CenterlineSkeletonizationResult skeletonized =
        skeletonizer.run(geometry, source);
    result.skeletonizationStatus = skeletonized.status;
    result.skeletonizationStage = skeletonized.stage;
    if (!skeletonized.ok()) {
        result.status = Status::SkeletonizationFailed;
        return result;
    }

    CenterlineBGraphResult graph = CenterlineBGraph::extractMainPath(
        skeletonized.output.value(), request.graphProfile);
    result.graphStatus = graph.status;
    if (!graph.ok()) {
        result.status = Status::GraphFailed;
        return result;
    }

    Output output;
    output.path.setId(request.outputPathNode);
    output.path.setInterpolation(PathInterpolation::Polyline);
    output.path.setSourceImageNode(request.sourceImageNode);
    std::vector<PathControlPoint> controlPoints;
    controlPoints.reserve(graph.mainPath.size());
    for (const CenterlineBGraphSample& sample : graph.mainPath) {
        controlPoints.push_back({sample.positionMm});
    }
    output.path.setControlPoints(controlPoints);
    const double sampleSpacing = (std::min)(
        geometry.spacing[0],
        (std::min)(geometry.spacing[1], geometry.spacing[2]));
    result.pathResampleStatus = output.path.resample(sampleSpacing);
    if (result.pathResampleStatus != XQPath::ResampleStatus::Ok) {
        result.status = Status::PathAssemblyFailed;
        return result;
    }

    output.pathContentFingerprint = pathFingerprint(output.path);
    if (output.pathContentFingerprint.size()
        != sizeof(kPathFingerprintPrefix) - 1 + 64) {
        result.status = Status::FingerprintFailed;
        return result;
    }

    DerivationInputStamp pathStamp;
    pathStamp.nodeId = request.outputPathNode;
    pathStamp.contentRevision = 0;
    pathStamp.assetId = request.outputPathAsset;
    pathStamp.assetFingerprint = output.pathContentFingerprint;

    output.profile.coordinateSystem = VesselProfileCoordinateSystem::LPS;
    output.profile.lengthUnit = VesselProfileLengthUnit::Millimeter;
    output.profile.areaUnit = VesselProfileAreaUnit::SquareMillimeter;
    output.profile.frameOfReferenceId = request.frameOfReferenceId;
    output.profile.sourcePathNode = request.outputPathNode;
    output.profile.sourceEvidenceNodes.push_back(request.sourceMask.nodeId);
    output.profile.derivationStamp.algorithmId = AlgorithmId;
    output.profile.derivationStamp.algorithmVersion = AlgorithmVersion;
    output.profile.derivationStamp.parameterSummary = parameterSummary(
        skeletonized.output.value(), request.graphProfile);
    output.profile.derivationStamp.inputs.push_back(pathStamp);
    output.profile.derivationStamp.inputs.push_back(request.sourceMask);
    output.profile.samples.reserve(graph.mainPath.size());
    for (std::size_t index = 0; index < graph.mainPath.size(); ++index) {
        const CenterlineBGraphSample& graphSample = graph.mainPath[index];
        VesselProfileSample sample;
        sample.sampleId = VesselSampleId(
            static_cast<VesselSampleId::ValueType>(index + 1));
        sample.arcLengthMm = graphSample.arcLengthMm;
        sample.positionMm = graphSample.positionMm;
        sample.unitTangent = tangentAt(graph.mainPath, index);
        sample.areaMm2 = kPi * graphSample.radiusMm * graphSample.radiusMm;
        sample.evidenceKind = VesselEvidenceKind::SegmentationDerived;
        sample.quality = VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = request.sourceMask.nodeId;
        output.profile.samples.push_back(sample);
    }

    result.profileValidation = VesselProfileValidator::validate(output.profile);
    if (!result.profileValidation.ok()) {
        result.status = Status::ProfileValidationFailed;
        return result;
    }

    output.profileContentFingerprint = profileFingerprint(output.profile);
    if (output.profileContentFingerprint.size()
        != sizeof(kProfileFingerprintPrefix) - 1 + 64) {
        result.status = Status::FingerprintFailed;
        return result;
    }

    DerivationInputStamp profileStamp;
    profileStamp.nodeId = request.outputProfileNode;
    profileStamp.contentRevision = 0;
    profileStamp.assetId = request.outputProfileAsset;
    profileStamp.assetFingerprint = output.profileContentFingerprint;
    VesselPathSnapshotService::Result snapshot =
        VesselPathSnapshotService::build(profileStamp, output.profile);
    result.snapshotStatus = snapshot.status;
    result.pathValidation = snapshot.pathValidation;
    if (!snapshot.ok()) {
        result.status = Status::SnapshotFailed;
        return result;
    }

    ShellGeometrySmokeService::Result smoke =
        ShellGeometrySmokeService::run(snapshot.path.value());
    result.smokeStatus = smoke.status;
    if (!smoke.ok()) {
        result.status = Status::GeometrySmokeFailed;
        return result;
    }

    output.moduleSnapshot = std::move(snapshot.path.value());
    output.geometrySmoke = std::move(smoke.summary);
    output.graphStats = graph.stats;
    output.inputForegroundVoxelCount =
        skeletonized.output->inputForegroundVoxelCount;
    output.skeletonVoxelCount = skeletonized.output->skeletonVoxelCount;
    output.thinningBackendId = skeletonized.output->thinningBackendId;
    output.thinningBackendVersion = skeletonized.output->thinningBackendVersion;
    output.distanceBackendId = skeletonized.output->distanceBackendId;
    output.distanceBackendVersion = skeletonized.output->distanceBackendVersion;

    result.status = Status::Ok;
    result.output.emplace(std::move(output));
    return result;
}

} // namespace xq
