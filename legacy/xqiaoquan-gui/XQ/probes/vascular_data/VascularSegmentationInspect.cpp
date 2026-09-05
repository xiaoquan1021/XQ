#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include <cstddef>
#include <cstdio>

namespace {

void printIdentity(bool present, const xq::DicomSeriesIdentity& identity)
{
    std::printf("dicom.identity_present=%s\n", present ? "true" : "false");
    std::printf("dicom.study_uid=%s\n", identity.studyInstanceUid.c_str());
    std::printf("dicom.series_uid=%s\n", identity.seriesInstanceUid.c_str());
    std::printf("dicom.frame_uid=%s\n", identity.frameOfReferenceUid.c_str());
}

void printLegacy(
    const xq::AutomaticVesselSegmentationArtifactReadResult& result)
{
    const xq::XQAutomaticVesselSegmentation& output =
        *result.legacySegmentation;
    std::printf("artifact.legacy=true\n");
    std::printf("artifact.legacy_production_valid=%s\n",
                result.legacyProductionValid ? "true" : "false");
    std::printf("algorithm.id=%s\n", output.algorithmId.c_str());
    std::printf("algorithm.version=%s\n", output.algorithmVersion.c_str());
    std::printf("algorithm.itk_version=%s\n", output.itkVersion.c_str());
    std::printf("lineage.input=%s\n", output.inputFingerprint.c_str());
    std::printf("lineage.vesselness=%s\n",
                output.vesselnessFingerprint.c_str());
    std::printf("lineage.profile=%s\n", output.profileFingerprint.c_str());
    std::printf("lineage.output=%s\n", output.outputFingerprint.c_str());
    printIdentity(output.hasDicomIdentity, output.dicomIdentity);
    std::printf("profile.id=%s\n", output.profile.profileId.c_str());
    std::printf("mask.component_count=%zu\n", output.componentCount);
    std::printf("mask.foreground_voxels=%zu\n",
                output.mask->foregroundVoxelCount());
}

void printV2(const xq::XQAutomaticVesselSegmentationV2& output)
{
    std::printf("artifact.legacy=false\n");
    std::printf("algorithm.id=%s\n", output.algorithmId.c_str());
    std::printf("algorithm.version=%s\n", output.algorithmVersion.c_str());
    std::printf("algorithm.itk_version=%s\n", output.itkVersion.c_str());
    if (output.profile.schemaVersion == 2) {
        std::printf("algorithm.tubetk_version=%s\n",
                    output.tubeTkVersion.c_str());
    }
    std::printf("lineage.input=%s\n", output.inputFingerprint.c_str());
    std::printf("lineage.vesselness=%s\n",
                output.vesselnessFingerprint.c_str());
    std::printf("lineage.roi_prior=%s\n",
                output.roiPriorFingerprint.c_str());
    std::printf("lineage.profile=%s\n", output.profileFingerprint.c_str());
    std::printf("lineage.output=%s\n", output.outputFingerprint.c_str());
    printIdentity(output.hasDicomIdentity, output.dicomIdentity);

    std::printf("profile.id=%s\n", output.profile.profileId.c_str());
    std::printf("profile.schema_version=%u\n", output.profile.schemaVersion);
    std::printf("mask.domain_voxels=%zu\n", output.domainVoxelCount);
    if (output.profile.schemaVersion == 2) {
        std::printf("profile.high_resolution_isotropic=%s\n",
                    output.profile.makeHighResolutionIsotropic
                        ? "true" : "false");
        std::printf("profile.vesselness_mask_range=%.17g,%.17g\n",
                    output.profile.vesselnessMaskMinimum,
                    output.profile.vesselnessMaskMaximum);
        std::printf("profile.input_blur_sigma_mm=%.17g\n",
                    output.profile.inputBlurSigmaMm);
        std::printf("profile.input_window=%.17g,%.17g,%.17g,%.17g\n",
                    output.profile.inputWindowMinimum,
                    output.profile.inputWindowMaximum,
                    output.profile.inputWindowOutputMinimum,
                    output.profile.inputWindowOutputMaximum);
        std::printf("profile.seed_blur_sigma_mm=%.17g\n",
                    output.profile.seedBlurSigmaMm);
        std::printf("profile.seed_mask_range_mm=%.17g,%.17g\n",
                    output.profile.seedMaskRangeMinimumMm,
                    output.profile.seedMaskRangeMaximumMm);
        std::printf("profile.seed_extraction_minimum_probability=%.17g\n",
                    output.profile.seedExtractionMinimumProbability);
        std::printf("profile.ridge_minima=%.17g,%.17g,%.17g,%.17g\n",
                    output.profile.minimumCurvature,
                    output.profile.minimumRoundness,
                    output.profile.minimumRidgeness,
                    output.profile.minimumLevelness);
        std::printf("profile.radius_in_object_space_mm=%.17g\n",
                    output.profile.radiusInObjectSpaceMm);
        std::printf("profile.border_in_index_space=%u\n",
                    output.profile.borderInIndexSpace);
        std::printf("profile.optimize_radius=%s\n",
                    output.profile.optimizeRadius ? "true" : "false");
        std::printf("profile.use_seed_mask_as_probabilities=%s\n",
                    output.profile.useSeedMaskAsProbabilities
                        ? "true" : "false");
        std::printf("profile.rasterize_with_radius=%s\n",
                    output.profile.rasterizeWithRadius ? "true" : "false");
        std::printf("working.spacing_mm=%.17g\n", output.workingSpacingMm);
        std::printf("working.dimensions=%d,%d,%d\n",
                    output.workingDimensions[0], output.workingDimensions[1],
                    output.workingDimensions[2]);
        std::printf("working.voxels=%zu\n", output.workingVoxelCount);
        std::printf("seed.voxels=%zu\n", output.seedVoxelCount);
        std::printf("tube.count=%zu\n", output.extractedTubeCount);
        std::printf("tube.point_count=%zu\n", output.extractedTubePointCount);
        std::printf("working.rasterized_voxels=%zu\n",
                    output.workingRasterizedVoxelCount);
    } else {
        std::printf("profile.coarse_domain_dilation_mm=%.17g\n",
                    output.profile.coarseDomainDilationMm);
        std::printf("profile.edge_sigma_mm=%.17g\n",
                    output.profile.edgeSigmaMm);
        std::printf("profile.level_set_scaling=%.17g,%.17g,%.17g\n",
                    output.profile.propagationScaling,
                    output.profile.advectionScaling,
                    output.profile.curvatureScaling);
        std::printf("profile.maximum_rms_error=%.17g\n",
                    output.profile.maximumRmsError);
        std::printf("profile.maximum_iterations=%u\n",
                    output.profile.maximumIterations);
        std::printf("profile.isosurface_value=%.17g\n",
                    output.profile.isosurfaceValue);
        std::printf("profile.use_image_spacing=%s\n",
                    output.profile.useImageSpacing ? "true" : "false");
        std::printf("initial_surface.voxels=%zu\n",
                    output.initialSurfaceVoxelCount);
        std::printf("edge_potential.positive_voxels=%zu\n",
                    output.edgePotentialPositiveVoxelCount);
        std::printf("edge_potential.range=%.17g,%.17g\n",
                    output.edgePotentialMinimum,
                    output.edgePotentialMaximum);
        std::printf("level_set.elapsed_iterations=%u\n",
                    output.levelSetElapsedIterations);
        std::printf("level_set.rms_change=%.17g\n",
                    output.levelSetRmsChange);
        std::printf("level_set.converged=%s\n",
                    output.levelSetConverged ? "true" : "false");
    }
    std::printf("mask.component_count=%zu\n", output.componentCount);
    std::printf("mask.foreground_voxels=%zu\n", output.foregroundVoxelCount);

    for (std::size_t index = 0; index < output.roiPrior.layers.size(); ++index) {
        const xq::XQVascularRoiLayer& layer = output.roiPrior.layers[index];
        std::printf("roi.layer.%zu.role=%s\n", index,
                    xq::vascularRoiRoleToken(layer.role));
        std::printf("roi.layer.%zu.generator=%s\n", index,
                    layer.generatorId.c_str());
        std::printf("roi.layer.%zu.generator_version=%s\n", index,
                    layer.generatorVersion.c_str());
        std::printf("roi.layer.%zu.source_fingerprint=%s\n", index,
                    layer.sourceFingerprint.c_str());
        std::printf("roi.layer.%zu.aligned_fingerprint=%s\n", index,
                    layer.alignedFingerprint.c_str());
        std::printf("roi.layer.%zu.source_voxels=%zu\n", index,
                    layer.sourceForegroundVoxelCount);
        std::printf("roi.layer.%zu.aligned_voxels=%zu\n", index,
                    layer.alignedForegroundVoxelCount);
        std::printf("roi.layer.%zu.resampled=%s\n", index,
                    layer.resampledToReference ? "true" : "false");
    }
    for (std::size_t index = 0; index < output.upstreamStages.size(); ++index) {
        const xq::AutomaticVesselUpstreamStage& stage =
            output.upstreamStages[index];
        std::printf("upstream.%zu.stage_id=%s\n", index,
                    stage.stageId.c_str());
        std::printf("upstream.%zu.implementation=%s\n", index,
                    stage.implementation.c_str());
        std::printf("upstream.%zu.version=%s\n", index,
                    stage.implementationVersion.c_str());
        for (std::size_t parameterIndex = 0;
             parameterIndex < stage.parameters.size(); ++parameterIndex) {
            const xq::AutomaticVesselFilterParameter& parameter =
                stage.parameters[parameterIndex];
            std::printf("upstream.%zu.parameter.%zu=%s,%.17g,%s\n", index,
                        parameterIndex, parameter.name.c_str(), parameter.value,
                        parameter.unit.c_str());
        }
    }
    for (std::size_t index = 0; index < output.componentVoxelCounts.size();
         ++index) {
        std::printf("component.%zu.voxels=%zu\n", index,
                    output.componentVoxelCounts[index]);
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2 || argv == nullptr || argv[1] == nullptr) {
        std::fprintf(stderr,
                     "Usage: xq_vascular_segment_inspect <prediction.xqvmask>\n");
        return 1;
    }
    const xq::AutomaticVesselSegmentationArtifactReadResult result =
        xq::readAutomaticVesselSegmentationArtifact(argv[1]);
    std::printf("artifact.status=%s\n",
                xq::automaticVesselSegmentationArtifactStatusToken(result.status));
    if (!result.ok()) {
        return 2;
    }
    std::printf("artifact.format_version=%u\n", result.formatVersion);
    std::printf("artifact.sha256=%s\n", result.artifactSha256.c_str());
    if (result.isLegacy()) {
        printLegacy(result);
    } else {
        printV2(*result.segmentation);
    }
    return 0;
}
