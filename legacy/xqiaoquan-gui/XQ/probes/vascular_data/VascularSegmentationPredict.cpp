#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkAutomaticVesselSegmenter.h"
#include "adapters/itk/ItkVascularPreprocessor.h"
#include "adapters/itk/ItkVascularRoiPriorReader.h"
#include "core/source/ResidentVoxelSource.h"
#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* severityToken(xq::DiagnosticSeverity severity)
{
    switch (severity) {
    case xq::DiagnosticSeverity::Info: return "info";
    case xq::DiagnosticSeverity::Warning: return "warning";
    case xq::DiagnosticSeverity::Error: return "error";
    }
    return "unknown";
}

void printUsage(const char* executable)
{
    std::fprintf(
        stderr,
        "Usage: %s <ct-dicom-directory> <organ-roi.nii.gz> <coarse-vessel-roi.nii.gz> <output.xqvmask> [series-uid]\n",
        executable != nullptr ? executable : "xq_vascular_segment_predict");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || argc > 6 || argv == nullptr || argv[1] == nullptr
        || argv[2] == nullptr || argv[3] == nullptr || argv[4] == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }
    const std::string dicomDirectory(argv[1]);
    const std::string organRoiPath(argv[2]);
    const std::string coarseVesselRoiPath(argv[3]);
    const std::string outputPath(argv[4]);
    std::string seriesUid;
    if (argc == 6 && argv[5] != nullptr) {
        seriesUid = argv[5];
    }

    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery =
        reader.discover(dicomDirectory);
    std::printf("dicom.discovery_status=%s\n",
                xq::dicomSeriesStatusToken(discovery.status));
    std::printf("dicom.series_count=%zu\n", discovery.series.size());
    if (!discovery.ok()) {
        return 2;
    }
    if (seriesUid.empty()) {
        if (discovery.series.size() != 1) {
            std::fprintf(
                stderr,
                "A series UID is required when the directory contains more than one series.\n");
            return 3;
        }
        seriesUid = discovery.series.front().identity.seriesInstanceUid;
    }

    const xq::DicomSeriesReadResult read = reader.read(dicomDirectory, seriesUid);
    std::printf("dicom.read_status=%s\n",
                xq::dicomSeriesStatusToken(read.status));
    std::printf("dicom.diagnostic_count=%zu\n", read.diagnostics.size());
    for (std::size_t index = 0; index < read.diagnostics.size(); ++index) {
        std::printf("dicom.diagnostic.%zu.severity=%s\n", index,
                    severityToken(read.diagnostics[index].severity()));
        std::printf("dicom.diagnostic.%zu.code=%s\n", index,
                    read.diagnostics[index].code().c_str());
    }
    if (!read.ok()) {
        return 4;
    }
    std::printf("dicom.series_uid=%s\n",
                read.descriptor.identity.seriesInstanceUid.c_str());
    std::printf("dicom.frame_uid=%s\n",
                read.descriptor.identity.frameOfReferenceUid.c_str());
    std::printf("dicom.series_fingerprint=%s\n",
                read.descriptor.contentFingerprint.c_str());

    xq::ResidentVoxelSource source(read.buffer);
    xq::ItkVascularPreprocessor preprocessor;
    const xq::VascularPreprocessProfileV1 preprocessProfile =
        xq::portalVenousCtPreprocessProfileV1();
    std::printf("preprocess.begin=true\n");
    std::fflush(stdout);
    const xq::VascularPreprocessResult preprocessed =
        preprocessor.run(read.volume, source, preprocessProfile);
    std::printf("preprocess.status=%s\n",
                xq::vascularPreprocessStatusToken(preprocessed.status));
    std::printf("preprocess.stage=%s\n",
                xq::vascularPreprocessStageToken(preprocessed.stage));
    std::printf("preprocess.elapsed_ms=%.17g\n",
                preprocessed.elapsedMilliseconds);
    for (std::size_t index = 0; index < preprocessed.diagnostics.size(); ++index) {
        const xq::VascularPreprocessDiagnostic& diagnostic =
            preprocessed.diagnostics[index];
        std::printf("preprocess.diagnostic.%zu.code=%s\n", index,
                    diagnostic.messageKey.c_str());
    }
    if (!preprocessed.ok()) {
        return 5;
    }
    std::printf("preprocess.input_fingerprint=%s\n",
                preprocessed.output->inputFingerprint.c_str());
    std::printf("preprocess.output_fingerprint=%s\n",
                preprocessed.output->outputFingerprint.c_str());

    const std::vector<xq::VascularRoiFileInput> roiInputs = {
        {xq::VascularRoiRole::Organ, organRoiPath,
         "TotalSegmentator", "2.15.0"},
        {xq::VascularRoiRole::CoarseVessel, coarseVesselRoiPath,
         "TotalSegmentator", "2.15.0"}
    };
    xq::ItkVascularRoiPriorReader roiReader;
    const xq::VascularRoiPriorReadResult roi = roiReader.read(
        read.volume, preprocessed.output->inputFingerprint, roiInputs);
    std::printf("roi.status=%s\n",
                xq::vascularRoiPriorReadStatusToken(roi.status));
    std::printf("roi.diagnostic_count=%zu\n", roi.diagnostics.size());
    for (std::size_t index = 0; index < roi.diagnostics.size(); ++index) {
        std::printf("roi.diagnostic.%zu.code=%s\n", index,
                    roi.diagnostics[index].code().c_str());
    }
    if (!roi.ok()) {
        return 6;
    }
    std::printf("roi.prior_fingerprint=%s\n",
                roi.prior->priorFingerprint.c_str());
    for (std::size_t index = 0; index < roi.prior->layers.size(); ++index) {
        const xq::XQVascularRoiLayer& layer = roi.prior->layers[index];
        std::printf("roi.layer.%zu.role=%s\n", index,
                    xq::vascularRoiRoleToken(layer.role));
        std::printf("roi.layer.%zu.source_fingerprint=%s\n", index,
                    layer.sourceFingerprint.c_str());
        std::printf("roi.layer.%zu.aligned_fingerprint=%s\n", index,
                    layer.alignedFingerprint.c_str());
        std::printf("roi.layer.%zu.aligned_voxels=%zu\n", index,
                    layer.alignedForegroundVoxelCount);
        std::printf("roi.layer.%zu.resampled=%s\n", index,
                    layer.resampledToReference ? "true" : "false");
    }

    xq::ItkAutomaticVesselSegmenter segmenter;
    const xq::AutomaticVesselSegmentationProfileV2 segmentationProfile =
        xq::portalVenousCtAutomaticSegmentationProfileV2();
    std::printf("segmentation.profile_id=%s\n",
                segmentationProfile.profileId.c_str());
    std::printf("segmentation.coarse_domain_dilation_mm=%.17g\n",
                segmentationProfile.coarseDomainDilationMm);
    std::printf("segmentation.edge_sigma_mm=%.17g\n",
                segmentationProfile.edgeSigmaMm);
    std::printf("segmentation.level_set_scaling=%.17g,%.17g,%.17g\n",
                segmentationProfile.propagationScaling,
                segmentationProfile.advectionScaling,
                segmentationProfile.curvatureScaling);
    std::printf("segmentation.maximum_rms_error=%.17g\n",
                segmentationProfile.maximumRmsError);
    std::printf("segmentation.maximum_iterations=%u\n",
                segmentationProfile.maximumIterations);
    std::printf("segmentation.begin=true\n");
    std::fflush(stdout);
    const xq::AutomaticVesselSegmentationResult segmented = segmenter.run(
        read.volume, source, *preprocessed.output, *roi.prior,
        segmentationProfile);
    std::printf("segmentation.status=%s\n",
                xq::automaticVesselSegmentationStatusToken(segmented.status));
    std::printf("segmentation.stage=%s\n",
                xq::automaticVesselSegmentationStageToken(segmented.stage));
    std::printf("segmentation.elapsed_ms=%.17g\n",
                segmented.elapsedMilliseconds);
    for (std::size_t index = 0; index < segmented.diagnostics.size(); ++index) {
        const xq::AutomaticVesselSegmentationDiagnostic& diagnostic =
            segmented.diagnostics[index];
        std::printf("segmentation.diagnostic.%zu.code=%s\n", index,
                    diagnostic.messageKey.c_str());
        std::printf("segmentation.diagnostic.%zu.stage=%s\n", index,
                    xq::automaticVesselSegmentationStageToken(diagnostic.stage));
    }
    if (!segmented.ok()) {
        return 7;
    }

    const xq::XQAutomaticVesselSegmentationV2& output = *segmented.output;
    std::printf("segmentation.algorithm_id=%s\n", output.algorithmId.c_str());
    std::printf("segmentation.algorithm_version=%s\n",
                output.algorithmVersion.c_str());
    std::printf("segmentation.itk_version=%s\n", output.itkVersion.c_str());
    std::printf("segmentation.input_fingerprint=%s\n",
                output.inputFingerprint.c_str());
    std::printf("segmentation.vesselness_fingerprint=%s\n",
                output.vesselnessFingerprint.c_str());
    std::printf("segmentation.roi_prior_fingerprint=%s\n",
                output.roiPriorFingerprint.c_str());
    std::printf("segmentation.profile_fingerprint=%s\n",
                output.profileFingerprint.c_str());
    std::printf("segmentation.output_fingerprint=%s\n",
                output.outputFingerprint.c_str());
    std::printf("segmentation.domain_voxels=%zu\n", output.domainVoxelCount);
    std::printf("segmentation.initial_surface_voxels=%zu\n",
                output.initialSurfaceVoxelCount);
    std::printf("segmentation.edge_potential_positive_voxels=%zu\n",
                output.edgePotentialPositiveVoxelCount);
    std::printf("segmentation.edge_potential_range=%.17g,%.17g\n",
                output.edgePotentialMinimum, output.edgePotentialMaximum);
    std::printf("segmentation.level_set_elapsed_iterations=%u\n",
                output.levelSetElapsedIterations);
    std::printf("segmentation.level_set_rms_change=%.17g\n",
                output.levelSetRmsChange);
    std::printf("segmentation.level_set_converged=%s\n",
                output.levelSetConverged ? "true" : "false");
    std::printf("segmentation.component_count=%zu\n", output.componentCount);
    std::printf("segmentation.foreground_voxels=%zu\n",
                output.foregroundVoxelCount);
    for (std::size_t index = 0; index < output.upstreamStages.size(); ++index) {
        std::printf("segmentation.upstream.%zu=%s,%s,%s\n", index,
                    output.upstreamStages[index].stageId.c_str(),
                    output.upstreamStages[index].implementation.c_str(),
                    output.upstreamStages[index].implementationVersion.c_str());
    }

    const xq::AutomaticVesselSegmentationArtifactWriteResult written =
        xq::writeAutomaticVesselSegmentationArtifact(outputPath, output);
    std::printf("artifact.write_status=%s\n",
                xq::automaticVesselSegmentationArtifactStatusToken(written.status));
    if (!written.ok()) {
        return 8;
    }
    std::printf("artifact.sha256=%s\n", written.artifactSha256.c_str());

    const xq::AutomaticVesselSegmentationArtifactReadResult reopened =
        xq::readAutomaticVesselSegmentationArtifact(outputPath);
    std::printf("artifact.reopen_status=%s\n",
                xq::automaticVesselSegmentationArtifactStatusToken(reopened.status));
    if (!reopened.ok() || reopened.formatVersion != 6
        || reopened.isLegacy() || !reopened.segmentation.has_value()
        || reopened.artifactSha256 != written.artifactSha256
        || reopened.segmentation->outputFingerprint != output.outputFingerprint
        || reopened.segmentation->roiPriorFingerprint
            != output.roiPriorFingerprint) {
        return 9;
    }
    std::printf("artifact.format_version=%u\n", reopened.formatVersion);
    std::printf("artifact.reopen_integrity=true\n");
    return 0;
}
