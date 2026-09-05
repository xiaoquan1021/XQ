#include "adapters/gdcm/GdcmItkDicomLabelReader.h"
#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkVascularSegmentationEvaluator.h"
#include "io/blob/Sha256.h"
#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

void printUsage(const char* executable)
{
    std::fprintf(
        stderr,
        "Usage: %s <prediction.xqvmask> <gold-dicom-directory> <baseline-file> <gold-foreground-value> <label-name> [series-uid]\n",
        executable != nullptr ? executable : "xq_vascular_segment_evaluate");
}

bool parseDouble(const char* text, double* value)
{
    if (text == nullptr || value == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (errno != 0 || end == text || end == nullptr || *end != '\0') {
        return false;
    }
    *value = parsed;
    return true;
}

std::string hashFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    xq::Sha256 hash;
    std::vector<char> buffer(1024 * 1024);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            hash.update(buffer.data(), static_cast<std::size_t>(count));
        }
    }
    if (!input.eof()) {
        return {};
    }
    return hash.finalHex();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 6 || argc > 7 || argv == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }
    double foregroundValue = 0.0;
    if (!parseDouble(argv[4], &foregroundValue)) {
        printUsage(argv[0]);
        return 1;
    }
    const std::string predictionPath(argv[1]);
    const std::string goldDirectory(argv[2]);
    const std::string baselinePath(argv[3]);
    const std::string labelName(argv[5]);
    std::string seriesUid;
    if (argc == 7 && argv[6] != nullptr) {
        seriesUid = argv[6];
    }

    // The prediction is opened and hashed before any gold reader is invoked.
    const xq::AutomaticVesselSegmentationArtifactReadResult prediction =
        xq::readAutomaticVesselSegmentationArtifact(predictionPath);
    std::printf("prediction.read_status=%s\n",
                xq::automaticVesselSegmentationArtifactStatusToken(
                    prediction.status));
    if (!prediction.ok()) {
        return 2;
    }
    std::printf("prediction.artifact_sha256=%s\n",
                prediction.artifactSha256.c_str());
    std::printf("prediction.artifact_format_version=%u\n",
                prediction.formatVersion);
    const xq::XQSegmentationMask* predictionMask = nullptr;
    const xq::DicomSeriesIdentity* predictionIdentity = nullptr;
    bool predictionHasIdentity = false;
    const std::string* predictionOutputFingerprint = nullptr;
    const std::string* predictionInputFingerprint = nullptr;
    const std::string* predictionProfileFingerprint = nullptr;
    if (prediction.isLegacy()) {
        predictionMask = prediction.legacySegmentation->mask.get();
        predictionHasIdentity = prediction.legacySegmentation->hasDicomIdentity;
        predictionIdentity = &prediction.legacySegmentation->dicomIdentity;
        predictionOutputFingerprint =
            &prediction.legacySegmentation->outputFingerprint;
        predictionInputFingerprint =
            &prediction.legacySegmentation->inputFingerprint;
        predictionProfileFingerprint =
            &prediction.legacySegmentation->profileFingerprint;
    } else {
        predictionMask = prediction.segmentation->mask.get();
        predictionHasIdentity = prediction.segmentation->hasDicomIdentity;
        predictionIdentity = &prediction.segmentation->dicomIdentity;
        predictionOutputFingerprint = &prediction.segmentation->outputFingerprint;
        predictionInputFingerprint = &prediction.segmentation->inputFingerprint;
        predictionProfileFingerprint = &prediction.segmentation->profileFingerprint;
    }
    if (predictionMask == nullptr || predictionIdentity == nullptr
        || predictionOutputFingerprint == nullptr
        || predictionInputFingerprint == nullptr
        || predictionProfileFingerprint == nullptr) {
        return 2;
    }
    std::printf("prediction.mask_fingerprint=%s\n",
                predictionOutputFingerprint->c_str());
    std::printf("prediction.input_fingerprint=%s\n",
                predictionInputFingerprint->c_str());
    std::printf("prediction.profile_fingerprint=%s\n",
                predictionProfileFingerprint->c_str());

    const std::string baselineHash = hashFile(baselinePath);
    if (baselineHash.size() != 64) {
        std::printf("baseline.read_status=io_error\n");
        return 3;
    }
    std::printf("baseline.read_status=ok\n");
    std::printf("baseline.sha256=%s\n", baselineHash.c_str());

    std::printf("gold.open_begin=true\n");
    std::fflush(stdout);
    xq::GdcmItkDicomSeriesReader seriesReader;
    const xq::DicomSeriesDiscoveryResult discovery =
        seriesReader.discover(goldDirectory);
    std::printf("gold.discovery_status=%s\n",
                xq::dicomSeriesStatusToken(discovery.status));
    if (!discovery.ok()) {
        return 4;
    }
    if (seriesUid.empty()) {
        if (discovery.series.size() != 1) {
            std::fprintf(stderr,
                         "A gold series UID is required when the directory contains more than one series.\n");
            return 5;
        }
        seriesUid = discovery.series.front().identity.seriesInstanceUid;
    }
    xq::DicomBinaryLabelProfile labelProfile;
    labelProfile.backgroundValue = 0.0;
    labelProfile.foregroundValue = foregroundValue;
    labelProfile.outputLabel = 1;
    labelProfile.labelName = labelName;
    xq::GdcmItkDicomLabelReader labelReader;
    const xq::DicomLabelReadResult gold =
        labelReader.read(goldDirectory, seriesUid, labelProfile);
    std::printf("gold.read_status=%s\n",
                xq::dicomLabelStatusToken(gold.status));
    if (!gold.ok()) {
        return 6;
    }
    std::printf("gold.series_uid=%s\n",
                gold.descriptor.identity.seriesInstanceUid.c_str());
    std::printf("gold.frame_uid=%s\n",
                gold.descriptor.identity.frameOfReferenceUid.c_str());
    std::printf("gold.series_fingerprint=%s\n",
                gold.descriptor.contentFingerprint.c_str());
    std::printf("gold.mask_sha256=%s\n",
                xq::Sha256::hashHex(gold.mask->voxels()).c_str());
    std::printf("gold.foreground_voxels=%zu\n", gold.foregroundVoxelCount);
    if (predictionHasIdentity
        && !predictionIdentity->frameOfReferenceUid.empty()
        && predictionIdentity->frameOfReferenceUid
            != gold.descriptor.identity.frameOfReferenceUid) {
        std::printf("evaluation.status=frame_uid_mismatch\n");
        return 7;
    }

    xq::ItkVascularSegmentationEvaluator evaluator;
    const xq::VascularSegmentationEvaluationResult evaluated = evaluator.evaluate(
        *predictionMask, *gold.mask);
    std::printf("evaluation.status=%s\n",
                xq::vascularSegmentationEvaluationStatusToken(evaluated.status));
    std::printf("evaluation.stage=%s\n",
                xq::vascularSegmentationEvaluationStageToken(evaluated.stage));
    std::printf("evaluation.elapsed_ms=%.17g\n", evaluated.elapsedMilliseconds);
    for (std::size_t index = 0; index < evaluated.diagnostics.size(); ++index) {
        std::printf("evaluation.diagnostic.%zu.code=%s\n", index,
                    evaluated.diagnostics[index].messageKey.c_str());
    }
    if (!evaluated.ok()) {
        return 8;
    }
    const xq::VascularSegmentationMetrics& metrics = *evaluated.metrics;
    std::printf("metric.evaluator_id=%s\n", metrics.evaluatorId.c_str());
    std::printf("metric.evaluator_version=%s\n",
                metrics.evaluatorVersion.c_str());
    std::printf("metric.itk_version=%s\n", metrics.itkVersion.c_str());
    std::printf("metric.thinning_backend=%s\n",
                metrics.thinningBackendId.c_str());
    std::printf("metric.thinning_version=%s\n",
                metrics.thinningBackendVersion.c_str());
    std::printf("metric.prediction_foreground=%zu\n",
                metrics.predictionForegroundVoxelCount);
    std::printf("metric.reference_foreground=%zu\n",
                metrics.referenceForegroundVoxelCount);
    std::printf("metric.intersection=%zu\n", metrics.intersectionVoxelCount);
    std::printf("metric.dice=%.17g\n", metrics.dice);
    std::printf("metric.voxel_precision=%.17g\n",
                static_cast<double>(metrics.intersectionVoxelCount)
                    / static_cast<double>(
                        metrics.predictionForegroundVoxelCount));
    std::printf("metric.voxel_recall=%.17g\n",
                static_cast<double>(metrics.intersectionVoxelCount)
                    / static_cast<double>(
                        metrics.referenceForegroundVoxelCount));
    std::printf("metric.prediction_components=%zu\n",
                metrics.predictionComponentCount);
    std::printf("metric.largest_component_fraction=%.17g\n",
                metrics.largestPredictionComponentFraction);
    std::printf("metric.prediction_surface_voxels=%zu\n",
                metrics.predictionSurfaceVoxelCount);
    std::printf("metric.reference_surface_voxels=%zu\n",
                metrics.referenceSurfaceVoxelCount);
    std::printf("metric.prediction_to_reference_p95_mm=%.17g\n",
                metrics.predictionToReferenceSurfaceP95Mm);
    std::printf("metric.reference_to_prediction_p95_mm=%.17g\n",
                metrics.referenceToPredictionSurfaceP95Mm);
    std::printf("metric.hd95_mm=%.17g\n", metrics.hd95Mm);
    std::printf("metric.assd_mm=%.17g\n", metrics.assdMm);
    std::printf("metric.prediction_skeleton_voxels=%zu\n",
                metrics.predictionSkeletonVoxelCount);
    std::printf("metric.reference_skeleton_voxels=%zu\n",
                metrics.referenceSkeletonVoxelCount);
    std::printf("metric.topology_precision=%.17g\n",
                metrics.topologyPrecision);
    std::printf("metric.topology_sensitivity=%.17g\n",
                metrics.topologySensitivity);
    std::printf("metric.cldice=%.17g\n", metrics.clDice);
    return 0;
}
