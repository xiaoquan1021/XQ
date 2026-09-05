#include "core/image/IAutomaticVesselSegmenter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>

namespace xq {
namespace {

bool sameGeometry(const ImageGeometry& left, const ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || left.spacing[axis] != right.spacing[axis]
            || left.origin[axis] != right.origin[axis]) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (left.direction[axis][column]
                != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

bool hasImplementation(const std::vector<AutomaticVesselUpstreamStage>& stages,
                       const char* implementation)
{
    return std::any_of(
        stages.begin(), stages.end(),
        [implementation](const AutomaticVesselUpstreamStage& stage) {
            return stage.implementation == implementation;
        });
}

bool checkedVoxelCount(const int dimensions[3], std::size_t* count)
{
    if (count == nullptr) {
        return false;
    }
    std::size_t value = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimensions[axis] <= 0) {
            return false;
        }
        const std::size_t dimension =
            static_cast<std::size_t>(dimensions[axis]);
        if (value > (std::numeric_limits<std::size_t>::max)() / dimension) {
            return false;
        }
        value *= dimension;
    }
    *count = value;
    return true;
}

} // namespace

AutomaticVesselSegmentationProfileV1
portalVenousCtAutomaticSegmentationProfileV1()
{
    return AutomaticVesselSegmentationProfileV1{};
}

AutomaticVesselSegmentationProfileValidationCode
validateAutomaticVesselSegmentationProfile(
    const AutomaticVesselSegmentationProfileV1& profile)
{
    if (profile.profileId.empty()) {
        return AutomaticVesselSegmentationProfileValidationCode::MissingProfileId;
    }
    if (profile.schemaVersion != 1) {
        return AutomaticVesselSegmentationProfileValidationCode::UnsupportedSchemaVersion;
    }
    if (!std::isfinite(profile.candidateVesselnessQuantile)
        || !std::isfinite(profile.coreVesselnessQuantile)
        || profile.candidateVesselnessQuantile <= 0.0
        || profile.candidateVesselnessQuantile >= 1.0
        || profile.coreVesselnessQuantile <= profile.candidateVesselnessQuantile
        || profile.coreVesselnessQuantile >= 1.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidVesselnessQuantiles;
    }
    if (!std::isfinite(profile.candidateMaximumFractionOfMaximum)
        || !std::isfinite(profile.coreMaximumFractionOfMaximum)
        || profile.candidateMaximumFractionOfMaximum <= 0.0
        || profile.candidateMaximumFractionOfMaximum >= 1.0
        || profile.coreMaximumFractionOfMaximum
            <= profile.candidateMaximumFractionOfMaximum
        || profile.coreMaximumFractionOfMaximum >= 1.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidVesselnessFractions;
    }
    if (profile.vesselnessHistogramBins < 64
        || profile.vesselnessHistogramBins > 65536
        || profile.intensityHistogramBins < 64
        || profile.intensityHistogramBins > 65536) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidHistogramBins;
    }
    if (!std::isfinite(profile.hardIntensityLower)
        || !std::isfinite(profile.hardIntensityUpper)
        || profile.hardIntensityUpper <= profile.hardIntensityLower) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityBounds;
    }
    if (!std::isfinite(profile.coreIntensityLowerQuantile)
        || !std::isfinite(profile.coreIntensityUpperQuantile)
        || profile.coreIntensityLowerQuantile < 0.0
        || profile.coreIntensityUpperQuantile > 1.0
        || profile.coreIntensityUpperQuantile
            <= profile.coreIntensityLowerQuantile) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityQuantiles;
    }
    if (!std::isfinite(profile.intensityMargin) || profile.intensityMargin < 0.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityMargin;
    }
    if (profile.minimumIntensitySampleCount == 0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidIntensitySampleCount;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(profile.automaticSeedAnchorNormalized[axis])
            || !std::isfinite(profile.automaticSeedSearchRadiusNormalized[axis])
            || profile.automaticSeedAnchorNormalized[axis] < 0.0
            || profile.automaticSeedAnchorNormalized[axis] > 1.0
            || profile.automaticSeedSearchRadiusNormalized[axis] <= 0.0
            || profile.automaticSeedSearchRadiusNormalized[axis] > 1.0) {
            return AutomaticVesselSegmentationProfileValidationCode::InvalidAutomaticSeedPolicy;
        }
    }
    if (!std::isfinite(profile.automaticSeedIntensityLower)
        || !std::isfinite(profile.automaticSeedIntensityUpper)
        || profile.automaticSeedIntensityUpper <= profile.automaticSeedIntensityLower
        || !std::isfinite(profile.automaticSeedDistancePenalty)
        || profile.automaticSeedDistancePenalty < 0.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidAutomaticSeedPolicy;
    }
    if (!std::isfinite(profile.confidenceMultiplier)
        || profile.confidenceMultiplier <= 0.0
        || profile.confidenceMultiplier > 10.0
        || profile.confidenceIterations == 0
        || profile.confidenceIterations > 32
        || profile.confidenceInitialNeighborhoodRadiusVoxels == 0
        || profile.confidenceInitialNeighborhoodRadiusVoxels > 32
        || !std::isfinite(profile.confidenceExpansionRadiusMm)
        || profile.confidenceExpansionRadiusMm <= 0.0
        || profile.confidenceExpansionRadiusMm > 20.0
        || !std::isfinite(profile.maximumConfidenceForegroundFraction)
        || profile.maximumConfidenceForegroundFraction <= 0.0
        || profile.maximumConfidenceForegroundFraction > 1.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidConfidenceConnectedPolicy;
    }
    if (!std::isfinite(profile.closingRadiusMm)
        || !std::isfinite(profile.openingRadiusMm)
        || profile.closingRadiusMm < 0.0 || profile.openingRadiusMm < 0.0
        || profile.closingRadiusMm > 20.0 || profile.openingRadiusMm > 20.0) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidMorphologyRadius;
    }
    if (!std::isfinite(profile.minimumComponentVolumeMm3)
        || !std::isfinite(profile.minimumCoreVolumeMm3)
        || !std::isfinite(profile.maximumBoundaryFraction)
        || !std::isfinite(profile.minimumRelativeScore)
        || profile.minimumComponentVolumeMm3 <= 0.0
        || profile.minimumCoreVolumeMm3 <= 0.0
        || profile.maximumBoundaryFraction < 0.0
        || profile.maximumBoundaryFraction > 1.0
        || profile.minimumRelativeScore < 0.0
        || profile.minimumRelativeScore > 1.0
        || !std::isfinite(profile.additionalComponentScoreFraction)
        || profile.additionalComponentScoreFraction < 0.0
        || profile.additionalComponentScoreFraction > 1.0
        || profile.maximumRetainedComponents == 0
        || profile.maximumReportedComponents < profile.maximumRetainedComponents) {
        return AutomaticVesselSegmentationProfileValidationCode::InvalidComponentPolicy;
    }
    return AutomaticVesselSegmentationProfileValidationCode::Ok;
}

const char* automaticVesselSegmentationProfileValidationToken(
    AutomaticVesselSegmentationProfileValidationCode code)
{
    switch (code) {
    case AutomaticVesselSegmentationProfileValidationCode::Ok: return "ok";
    case AutomaticVesselSegmentationProfileValidationCode::MissingProfileId:
        return "missing_profile_id";
    case AutomaticVesselSegmentationProfileValidationCode::UnsupportedSchemaVersion:
        return "unsupported_schema_version";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidVesselnessQuantiles:
        return "invalid_vesselness_quantiles";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidVesselnessFractions:
        return "invalid_vesselness_fractions";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidHistogramBins:
        return "invalid_histogram_bins";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityBounds:
        return "invalid_intensity_bounds";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityQuantiles:
        return "invalid_intensity_quantiles";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidIntensityMargin:
        return "invalid_intensity_margin";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidIntensitySampleCount:
        return "invalid_intensity_sample_count";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidAutomaticSeedPolicy:
        return "invalid_automatic_seed_policy";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidConfidenceConnectedPolicy:
        return "invalid_confidence_connected_policy";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidMorphologyRadius:
        return "invalid_morphology_radius";
    case AutomaticVesselSegmentationProfileValidationCode::InvalidComponentPolicy:
        return "invalid_component_policy";
    }
    return "unknown";
}

AutomaticVesselSegmentationProfileV2
portalVenousCtAutomaticSegmentationProfileV2()
{
    return AutomaticVesselSegmentationProfileV2{};
}

AutomaticVesselSegmentationProfileV2ValidationCode
validateAutomaticVesselSegmentationProfileV2(
    const AutomaticVesselSegmentationProfileV2& profile)
{
    if (profile.profileId.empty()) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::MissingProfileId;
    }
    if (profile.schemaVersion == 3) {
        if (!std::isfinite(profile.coarseDomainDilationMm)
            || profile.coarseDomainDilationMm <= 0.0
            || profile.coarseDomainDilationMm > 20.0) {
            return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidDomainPolicy;
        }
        if (!std::isfinite(profile.edgeSigmaMm)
            || profile.edgeSigmaMm <= 0.0 || profile.edgeSigmaMm > 20.0) {
            return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidEdgePotentialPolicy;
        }
        if (!std::isfinite(profile.propagationScaling)
            || !std::isfinite(profile.advectionScaling)
            || !std::isfinite(profile.curvatureScaling)
            || !std::isfinite(profile.maximumRmsError)
            || !std::isfinite(profile.isosurfaceValue)
            || profile.propagationScaling <= 0.0
            || profile.propagationScaling > 100.0
            || profile.advectionScaling < 0.0
            || profile.advectionScaling > 100.0
            || profile.curvatureScaling < 0.0
            || profile.curvatureScaling > 100.0
            || profile.maximumRmsError <= 0.0
            || profile.maximumRmsError > 1.0
            || profile.maximumIterations == 0
            || profile.maximumIterations > 100000
            || profile.isosurfaceValue != 0.0
            || !profile.useImageSpacing) {
            return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidLevelSetPolicy;
        }
        return AutomaticVesselSegmentationProfileV2ValidationCode::Ok;
    }
    if (profile.schemaVersion != 2) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::UnsupportedSchemaVersion;
    }
    if (!profile.makeHighResolutionIsotropic) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidWorkingGridPolicy;
    }
    const double floatMaximum =
        static_cast<double>((std::numeric_limits<float>::max)());
    if (!std::isfinite(profile.vesselnessMaskMinimum)
        || !std::isfinite(profile.vesselnessMaskMaximum)
        || profile.vesselnessMaskMaximum <= profile.vesselnessMaskMinimum
        || profile.vesselnessMaskMinimum < -floatMaximum
        || profile.vesselnessMaskMaximum > floatMaximum
        || !std::isfinite(profile.inputBlurSigmaMm)
        || profile.inputBlurSigmaMm <= 0.0
        || !std::isfinite(profile.inputWindowMinimum)
        || !std::isfinite(profile.inputWindowMaximum)
        || !std::isfinite(profile.inputWindowOutputMinimum)
        || !std::isfinite(profile.inputWindowOutputMaximum)
        || profile.inputWindowMaximum <= profile.inputWindowMinimum
        || profile.inputWindowOutputMaximum
            <= profile.inputWindowOutputMinimum) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidInputPreparationPolicy;
    }
    if (!std::isfinite(profile.seedBlurSigmaMm)
        || profile.seedBlurSigmaMm <= 0.0
        || !std::isfinite(profile.seedMaskRangeMinimumMm)
        || !std::isfinite(profile.seedMaskRangeMaximumMm)
        || profile.seedMaskRangeMinimumMm <= 0.0
        || profile.seedMaskRangeMaximumMm
            <= profile.seedMaskRangeMinimumMm
        || !std::isfinite(profile.seedExtractionMinimumProbability)
        || profile.seedExtractionMinimumProbability < 0.0
        || profile.seedExtractionMinimumProbability > 1.0) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidSeedPolicy;
    }
    const auto validRidgeMeasure = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!validRidgeMeasure(profile.minimumCurvature)
        || !validRidgeMeasure(profile.minimumRoundness)
        || !validRidgeMeasure(profile.minimumRidgeness)
        || !validRidgeMeasure(profile.minimumLevelness)
        || !std::isfinite(profile.radiusInObjectSpaceMm)
        || profile.radiusInObjectSpaceMm <= 0.0
        || profile.radiusInObjectSpaceMm > 100.0
        || profile.borderInIndexSpace == 0
        || profile.borderInIndexSpace
            > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())
        || !profile.optimizeRadius || !profile.useSeedMaskAsProbabilities) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidTubeExtractionPolicy;
    }
    if (!profile.rasterizeWithRadius) {
        return AutomaticVesselSegmentationProfileV2ValidationCode::InvalidRasterizationPolicy;
    }
    return AutomaticVesselSegmentationProfileV2ValidationCode::Ok;
}

const char* automaticVesselSegmentationProfileV2ValidationToken(
    AutomaticVesselSegmentationProfileV2ValidationCode code)
{
    switch (code) {
    case AutomaticVesselSegmentationProfileV2ValidationCode::Ok: return "ok";
    case AutomaticVesselSegmentationProfileV2ValidationCode::MissingProfileId:
        return "missing_profile_id";
    case AutomaticVesselSegmentationProfileV2ValidationCode::UnsupportedSchemaVersion:
        return "unsupported_schema_version";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidWorkingGridPolicy:
        return "invalid_working_grid_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidInputPreparationPolicy:
        return "invalid_input_preparation_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidSeedPolicy:
        return "invalid_seed_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidTubeExtractionPolicy:
        return "invalid_tube_extraction_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidRasterizationPolicy:
        return "invalid_rasterization_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidDomainPolicy:
        return "invalid_domain_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidEdgePotentialPolicy:
        return "invalid_edge_potential_policy";
    case AutomaticVesselSegmentationProfileV2ValidationCode::InvalidLevelSetPolicy:
        return "invalid_level_set_policy";
    }
    return "unknown";
}

void AutomaticVesselSegmentationCancellation::requestCancellation() noexcept
{
    requested_.store(true, std::memory_order_relaxed);
}

bool AutomaticVesselSegmentationCancellation::isCancellationRequested() const noexcept
{
    return requested_.load(std::memory_order_relaxed);
}

const char* automaticVesselSegmentationStatusToken(
    AutomaticVesselSegmentationStatus status)
{
    switch (status) {
    case AutomaticVesselSegmentationStatus::Ok: return "ok";
    case AutomaticVesselSegmentationStatus::InvalidArgument: return "invalid_argument";
    case AutomaticVesselSegmentationStatus::InvalidProfile: return "invalid_profile";
    case AutomaticVesselSegmentationStatus::InvalidGeometry: return "invalid_geometry";
    case AutomaticVesselSegmentationStatus::InvalidSource: return "invalid_source";
    case AutomaticVesselSegmentationStatus::InvalidVesselness: return "invalid_vesselness";
    case AutomaticVesselSegmentationStatus::InvalidRoiPrior: return "invalid_roi_prior";
    case AutomaticVesselSegmentationStatus::LineageMismatch: return "lineage_mismatch";
    case AutomaticVesselSegmentationStatus::UnsupportedInput: return "unsupported_input";
    case AutomaticVesselSegmentationStatus::Cancelled: return "cancelled";
    case AutomaticVesselSegmentationStatus::AllocationFailed: return "allocation_failed";
    case AutomaticVesselSegmentationStatus::ProcessingFailed: return "processing_failed";
    case AutomaticVesselSegmentationStatus::EmptyDomain: return "empty_domain";
    case AutomaticVesselSegmentationStatus::EmptySeed: return "empty_seed";
    case AutomaticVesselSegmentationStatus::EmptyTubeGroup: return "empty_tube_group";
    case AutomaticVesselSegmentationStatus::EmptyInitialSurface:
        return "empty_initial_surface";
    case AutomaticVesselSegmentationStatus::EmptyEdgePotential:
        return "empty_edge_potential";
    case AutomaticVesselSegmentationStatus::EmptyOutput: return "empty_output";
    }
    return "unknown";
}

const char* automaticVesselSegmentationStageToken(
    AutomaticVesselSegmentationStage stage)
{
    switch (stage) {
    case AutomaticVesselSegmentationStage::None: return "none";
    case AutomaticVesselSegmentationStage::ValidateInput: return "validate_input";
    case AutomaticVesselSegmentationStage::AcquireInput: return "acquire_input";
    case AutomaticVesselSegmentationStage::ImportImages: return "import_images";
    case AutomaticVesselSegmentationStage::BuildWorkingGrid:
        return "build_working_grid";
    case AutomaticVesselSegmentationStage::PrepareTubeInput:
        return "prepare_tube_input";
    case AutomaticVesselSegmentationStage::PrepareSeedMask:
        return "prepare_seed_mask";
    case AutomaticVesselSegmentationStage::ExtractTubes: return "extract_tubes";
    case AutomaticVesselSegmentationStage::RasterizeTubes:
        return "rasterize_tubes";
    case AutomaticVesselSegmentationStage::BackMapOutput:
        return "back_map_output";
    case AutomaticVesselSegmentationStage::BuildDomain: return "build_domain";
    case AutomaticVesselSegmentationStage::InitializeLevelSet:
        return "initialize_level_set";
    case AutomaticVesselSegmentationStage::BuildEdgePotential:
        return "build_edge_potential";
    case AutomaticVesselSegmentationStage::EvolveLevelSet:
        return "evolve_level_set";
    case AutomaticVesselSegmentationStage::ThresholdLevelSet:
        return "threshold_level_set";
    case AutomaticVesselSegmentationStage::ConnectedComponents:
        return "connected_components";
    case AutomaticVesselSegmentationStage::MaterializeOutput:
        return "materialize_output";
    case AutomaticVesselSegmentationStage::Complete: return "complete";
    }
    return "unknown";
}

const char* automaticVesselSegmentationDiagnosticToken(
    AutomaticVesselSegmentationDiagnosticCode code)
{
    switch (code) {
    case AutomaticVesselSegmentationDiagnosticCode::InvalidArgument:
        return "automatic_vessel_segmentation.invalid_argument";
    case AutomaticVesselSegmentationDiagnosticCode::InvalidProfile:
        return "automatic_vessel_segmentation.invalid_profile";
    case AutomaticVesselSegmentationDiagnosticCode::MissingGeometry:
        return "automatic_vessel_segmentation.missing_geometry";
    case AutomaticVesselSegmentationDiagnosticCode::UnsupportedCoordinateSystem:
        return "automatic_vessel_segmentation.unsupported_coordinate_system";
    case AutomaticVesselSegmentationDiagnosticCode::InvalidDimensions:
        return "automatic_vessel_segmentation.invalid_dimensions";
    case AutomaticVesselSegmentationDiagnosticCode::NonFiniteGeometry:
        return "automatic_vessel_segmentation.non_finite_geometry";
    case AutomaticVesselSegmentationDiagnosticCode::NonPositiveSpacing:
        return "automatic_vessel_segmentation.non_positive_spacing";
    case AutomaticVesselSegmentationDiagnosticCode::SingularDirection:
        return "automatic_vessel_segmentation.singular_direction";
    case AutomaticVesselSegmentationDiagnosticCode::GeometryMismatch:
        return "automatic_vessel_segmentation.geometry_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::NotSingleComponent:
        return "automatic_vessel_segmentation.not_single_component";
    case AutomaticVesselSegmentationDiagnosticCode::UnsupportedScalarType:
        return "automatic_vessel_segmentation.unsupported_scalar_type";
    case AutomaticVesselSegmentationDiagnosticCode::SourceMetadataInvalid:
        return "automatic_vessel_segmentation.source_metadata_invalid";
    case AutomaticVesselSegmentationDiagnosticCode::SourceMetadataMismatch:
        return "automatic_vessel_segmentation.source_metadata_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::SourceAcquireFailed:
        return "automatic_vessel_segmentation.source_acquire_failed";
    case AutomaticVesselSegmentationDiagnosticCode::SourceByteSizeMismatch:
        return "automatic_vessel_segmentation.source_byte_size_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::InputScalarNonFinite:
        return "automatic_vessel_segmentation.input_scalar_non_finite";
    case AutomaticVesselSegmentationDiagnosticCode::VesselnessInvalid:
        return "automatic_vessel_segmentation.vesselness_invalid";
    case AutomaticVesselSegmentationDiagnosticCode::VesselnessBufferInvalid:
        return "automatic_vessel_segmentation.vesselness_buffer_invalid";
    case AutomaticVesselSegmentationDiagnosticCode::VesselnessScalarNonFinite:
        return "automatic_vessel_segmentation.vesselness_scalar_non_finite";
    case AutomaticVesselSegmentationDiagnosticCode::InvalidRoiPrior:
        return "automatic_vessel_segmentation.invalid_roi_prior";
    case AutomaticVesselSegmentationDiagnosticCode::MissingRoiRole:
        return "automatic_vessel_segmentation.missing_roi_role";
    case AutomaticVesselSegmentationDiagnosticCode::RoiGeometryMismatch:
        return "automatic_vessel_segmentation.roi_geometry_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::InputFingerprintMismatch:
        return "automatic_vessel_segmentation.input_fingerprint_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::VesselnessFingerprintMismatch:
        return "automatic_vessel_segmentation.vesselness_fingerprint_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::RoiFingerprintMismatch:
        return "automatic_vessel_segmentation.roi_fingerprint_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyRoiDomain:
        return "automatic_vessel_segmentation.empty_roi_domain";
    case AutomaticVesselSegmentationDiagnosticCode::EmptySeedMask:
        return "automatic_vessel_segmentation.empty_seed_mask";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyTubeGroup:
        return "automatic_vessel_segmentation.empty_tube_group";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyInitialSurface:
        return "automatic_vessel_segmentation.empty_initial_surface";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyEdgePotential:
        return "automatic_vessel_segmentation.empty_edge_potential";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyRasterizedTubes:
        return "automatic_vessel_segmentation.empty_rasterized_tubes";
    case AutomaticVesselSegmentationDiagnosticCode::AllForegroundOutput:
        return "automatic_vessel_segmentation.all_foreground_output";
    case AutomaticVesselSegmentationDiagnosticCode::ItkWorkingGridFailed:
        return "automatic_vessel_segmentation.itk_working_grid_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkInputPreparationFailed:
        return "automatic_vessel_segmentation.itk_input_preparation_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkSeedPreparationFailed:
        return "automatic_vessel_segmentation.itk_seed_preparation_failed";
    case AutomaticVesselSegmentationDiagnosticCode::TubeTkExtractionFailed:
        return "automatic_vessel_segmentation.tubetk_extraction_failed";
    case AutomaticVesselSegmentationDiagnosticCode::TubeTkRasterizationFailed:
        return "automatic_vessel_segmentation.tubetk_rasterization_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkBackMappingFailed:
        return "automatic_vessel_segmentation.itk_back_mapping_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkDomainCompositionFailed:
        return "automatic_vessel_segmentation.itk_domain_composition_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkInitialSurfaceFailed:
        return "automatic_vessel_segmentation.itk_initial_surface_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkEdgePotentialFailed:
        return "automatic_vessel_segmentation.itk_edge_potential_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetFailed:
        return "automatic_vessel_segmentation.itk_level_set_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetThresholdFailed:
        return "automatic_vessel_segmentation.itk_level_set_threshold_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkConnectedComponentsFailed:
        return "automatic_vessel_segmentation.itk_connected_components_failed";
    case AutomaticVesselSegmentationDiagnosticCode::Cancelled:
        return "automatic_vessel_segmentation.cancelled";
    case AutomaticVesselSegmentationDiagnosticCode::AllocationFailed:
        return "automatic_vessel_segmentation.allocation_failed";
    case AutomaticVesselSegmentationDiagnosticCode::ItkException:
        return "automatic_vessel_segmentation.itk_exception";
    case AutomaticVesselSegmentationDiagnosticCode::UnexpectedException:
        return "automatic_vessel_segmentation.unexpected_exception";
    case AutomaticVesselSegmentationDiagnosticCode::OutputGeometryMismatch:
        return "automatic_vessel_segmentation.output_geometry_mismatch";
    case AutomaticVesselSegmentationDiagnosticCode::OutputMaskInvalid:
        return "automatic_vessel_segmentation.output_mask_invalid";
    case AutomaticVesselSegmentationDiagnosticCode::EmptyOutput:
        return "automatic_vessel_segmentation.empty_output";
    }
    return "automatic_vessel_segmentation.unknown";
}

bool AutomaticVesselFilterParameter::isValid() const
{
    return !name.empty() && std::isfinite(value) && !unit.empty();
}

bool AutomaticVesselUpstreamStage::isValid() const
{
    if (stageId.empty() || implementation.empty()
        || implementationVersion.empty()) {
        return false;
    }
    return std::all_of(
        parameters.begin(), parameters.end(),
        [](const AutomaticVesselFilterParameter& parameter) {
            return parameter.isValid();
        });
}

bool XQAutomaticVesselSegmentationV2::isValid() const
{
    if (mask == nullptr || !mask->is_valid() || !mask->hasGeometry()
        || mask->foregroundVoxelCount() == 0 || !roiPrior.isValid()
        || validateAutomaticVesselSegmentationProfileV2(profile)
            != AutomaticVesselSegmentationProfileV2ValidationCode::Ok
        || inputFingerprint.empty() || vesselnessFingerprint.empty()
        || roiPriorFingerprint.empty() || profileFingerprint.empty()
        || outputFingerprint.empty() || algorithmId.empty()
        || algorithmVersion.empty() || itkVersion.empty()
        || roiPriorFingerprint != roiPrior.priorFingerprint
        || inputFingerprint != roiPrior.ctInputFingerprint) {
        return false;
    }
    const XQVascularRoiLayer* organ = roiPrior.layer(VascularRoiRole::Organ);
    const XQVascularRoiLayer* coarse =
        roiPrior.layer(VascularRoiRole::CoarseVessel);
    if (organ == nullptr || coarse == nullptr || organ->mask == nullptr
        || coarse->mask == nullptr
        || !sameGeometry(mask->geometry(), organ->mask->geometry())
        || !sameGeometry(mask->geometry(), coarse->mask->geometry())) {
        return false;
    }
    if (hasDicomIdentity
        && (dicomIdentity.studyInstanceUid.empty()
            || dicomIdentity.seriesInstanceUid.empty())) {
        return false;
    }
    if (foregroundVoxelCount == 0
        || foregroundVoxelCount != mask->foregroundVoxelCount()
        || componentCount == 0
        || componentCount != componentVoxelCounts.size()
        || !std::isfinite(elapsedMilliseconds) || elapsedMilliseconds < 0.0) {
        return false;
    }
    const std::size_t componentTotal = std::accumulate(
        componentVoxelCounts.begin(), componentVoxelCounts.end(),
        static_cast<std::size_t>(0));
    if (componentTotal != foregroundVoxelCount) {
        return false;
    }
    std::unordered_set<std::string> stageIds;
    for (const AutomaticVesselUpstreamStage& stage : upstreamStages) {
        if (!stage.isValid() || !stageIds.insert(stage.stageId).second) {
            return false;
        }
    }
    if (profile.schemaVersion == 2) {
        std::size_t expectedWorkingVoxelCount = 0;
        if (tubeTkVersion.empty()
            || !checkedVoxelCount(workingDimensions,
                                  &expectedWorkingVoxelCount)
            || !std::isfinite(workingSpacingMm) || workingSpacingMm <= 0.0
            || workingVoxelCount != expectedWorkingVoxelCount
            || domainVoxelCount == 0 || domainVoxelCount > workingVoxelCount
            || seedVoxelCount == 0 || seedVoxelCount > workingVoxelCount
            || extractedTubeCount == 0 || extractedTubePointCount == 0
            || extractedTubePointCount < extractedTubeCount
            || workingRasterizedVoxelCount == 0
            || workingRasterizedVoxelCount > workingVoxelCount) {
            return false;
        }
        return hasImplementation(upstreamStages, "tube::ResampleImage")
            && hasImplementation(upstreamStages, "itk::ResampleImageFilter")
            && hasImplementation(upstreamStages, "itk::OrImageFilter")
            && hasImplementation(upstreamStages, "itk::BinaryThresholdImageFilter")
            && hasImplementation(upstreamStages, "itk::MaskImageFilter")
            && hasImplementation(upstreamStages, "itk::RecursiveGaussianImageFilter")
            && hasImplementation(upstreamStages, "itk::IntensityWindowingImageFilter")
            && hasImplementation(upstreamStages, "itk::DanielssonDistanceMapImageFilter")
            && hasImplementation(upstreamStages, "itk::ThresholdImageFilter")
            && hasImplementation(upstreamStages, "tube::SegmentTubes")
            && hasImplementation(upstreamStages, "tube::ConvertTubesToImage")
            && hasImplementation(upstreamStages, "itk::ConnectedComponentImageFilter")
            && hasImplementation(upstreamStages, "itk::RelabelComponentImageFilter")
            && !hasImplementation(upstreamStages,
                                  "itk::ConfidenceConnectedImageFilter")
            && !hasImplementation(
                upstreamStages,
                "itk::BinaryReconstructionByDilationImageFilter");
    }
    if (profile.schemaVersion != 3 || !tubeTkVersion.empty()
        || workingSpacingMm != 0.0 || workingVoxelCount != 0
        || seedVoxelCount != 0 || extractedTubeCount != 0
        || extractedTubePointCount != 0
        || workingRasterizedVoxelCount != 0
        || domainVoxelCount == 0 || domainVoxelCount > mask->voxelCount()
        || initialSurfaceVoxelCount == 0
        || initialSurfaceVoxelCount > domainVoxelCount
        || edgePotentialPositiveVoxelCount != domainVoxelCount
        || !std::isfinite(edgePotentialMinimum)
        || !std::isfinite(edgePotentialMaximum)
        || edgePotentialMinimum <= 0.0
        || edgePotentialMaximum < edgePotentialMinimum
        || edgePotentialMaximum > 1.0
        || levelSetElapsedIterations == 0
        || levelSetElapsedIterations > profile.maximumIterations
        || !std::isfinite(levelSetRmsChange) || levelSetRmsChange < 0.0
        || levelSetConverged
            != (levelSetRmsChange <= profile.maximumRmsError)) {
        return false;
    }
    for (unsigned int radius : coarseDomainDilationRadiusVoxels) {
        if (radius == 0) {
            return false;
        }
    }
    return hasImplementation(upstreamStages, "itk::BinaryDilateImageFilter")
        && hasImplementation(upstreamStages, "itk::OrImageFilter")
        && hasImplementation(
            upstreamStages, "itk::SignedMaurerDistanceMapImageFilter")
        && hasImplementation(upstreamStages, "itk::RescaleIntensityImageFilter")
        && hasImplementation(
            upstreamStages,
            "itk::GradientMagnitudeRecursiveGaussianImageFilter")
        && hasImplementation(upstreamStages,
                             "itk::BoundedReciprocalImageFilter")
        && hasImplementation(upstreamStages, "itk::MaskImageFilter")
        && hasImplementation(
            upstreamStages,
            "itk::GeodesicActiveContourLevelSetImageFilter")
        && hasImplementation(upstreamStages, "itk::BinaryThresholdImageFilter")
        && hasImplementation(upstreamStages, "itk::AndImageFilter")
        && hasImplementation(upstreamStages,
                             "itk::ConnectedComponentImageFilter")
        && hasImplementation(upstreamStages,
                             "itk::RelabelComponentImageFilter")
        && !hasImplementation(upstreamStages, "tube::SegmentTubes")
        && !hasImplementation(upstreamStages, "tube::ConvertTubesToImage")
        && !hasImplementation(upstreamStages,
                              "itk::ConfidenceConnectedImageFilter")
        && !hasImplementation(
            upstreamStages,
            "itk::BinaryReconstructionByDilationImageFilter");
}

bool AutomaticVesselSegmentationResult::ok() const
{
    return status == AutomaticVesselSegmentationStatus::Ok
        && stage == AutomaticVesselSegmentationStage::Complete
        && output.has_value() && output->isValid();
}

const char* automaticVesselComponentDecisionToken(
    AutomaticVesselComponentDecision decision)
{
    switch (decision) {
    case AutomaticVesselComponentDecision::Selected: return "selected";
    case AutomaticVesselComponentDecision::NoCoreContact: return "no_core_contact";
    case AutomaticVesselComponentDecision::BelowMinimumVolume:
        return "below_minimum_volume";
    case AutomaticVesselComponentDecision::BelowMinimumCoreVolume:
        return "below_minimum_core_volume";
    case AutomaticVesselComponentDecision::BoundaryDominated:
        return "boundary_dominated";
    case AutomaticVesselComponentDecision::BelowRelativeScore:
        return "below_relative_score";
    case AutomaticVesselComponentDecision::RetentionLimit:
        return "retention_limit";
    }
    return "unknown";
}

bool XQAutomaticVesselSegmentation::isValid() const
{
    if (mask == nullptr || !mask->is_valid() || !mask->hasGeometry()
        || mask->foregroundVoxelCount() == 0
        || validateAutomaticVesselSegmentationProfile(profile)
            != AutomaticVesselSegmentationProfileValidationCode::Ok
        || inputFingerprint.empty() || vesselnessFingerprint.empty()
        || profileFingerprint.empty() || outputFingerprint.empty()
        || algorithmId.empty() || algorithmVersion.empty() || itkVersion.empty()) {
        return false;
    }
    if (hasDicomIdentity
        && (dicomIdentity.studyInstanceUid.empty()
            || dicomIdentity.seriesInstanceUid.empty())) {
        return false;
    }
    if (!std::isfinite(candidateVesselnessThreshold)
        || !std::isfinite(coreVesselnessThreshold)
        || candidateVesselnessThreshold <= 0.0
        || coreVesselnessThreshold <= candidateVesselnessThreshold
        || !std::isfinite(automaticIntensityLower)
        || !std::isfinite(automaticIntensityUpper)
        || automaticIntensityUpper <= automaticIntensityLower
        || automaticSeedComponentLabel == 0
        || !std::isfinite(automaticSeedIntensity)
        || !std::isfinite(automaticSeedVesselness)
        || automaticSeedVesselness <= 0.0
        || !std::isfinite(confidenceMean)
        || !std::isfinite(confidenceVariance)
        || confidenceVariance < 0.0 || confidenceVoxelCount == 0
        || coreVoxelCount == 0 || rawCandidateVoxelCount == 0
        || candidateVoxelCount == 0 || componentCount == 0
        || retainedComponentCount == 0
        || foregroundVoxelCount != mask->foregroundVoxelCount()
        || !itkThresholdExecuted || !itkConfidenceConnectedExecuted
        || !itkConfidenceExpansionExecuted || !itkMorphologyExecuted
        || !itkConnectedComponentsExecuted || !fullyConnected
        || !std::isfinite(elapsedMilliseconds) || elapsedMilliseconds < 0.0) {
        return false;
    }
    std::size_t selected = 0;
    for (const AutomaticVesselComponentRecord& component : components) {
        if (component.selected) {
            ++selected;
        }
    }
    return selected == retainedComponentCount;
}

} // namespace xq
