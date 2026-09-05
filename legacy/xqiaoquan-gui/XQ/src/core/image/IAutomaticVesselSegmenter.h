#ifndef XQ_CORE_IMAGE_I_AUTOMATIC_VESSEL_SEGMENTER_H
#define XQ_CORE_IMAGE_I_AUTOMATIC_VESSEL_SEGMENTER_H

#include "core/Diagnostics.h"
#include "core/XQSegmentationMask.h"
#include "core/image/IVascularPreprocessor.h"
#include "core/image/XQVascularRoiPrior.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class IVoxelSource;

// Legacy v1 profile retained only so artifact versions 1-4 remain readable.
// The production segmenter does not accept this type.
struct AutomaticVesselSegmentationProfileV1 {
    std::string profileId = "xq.portal-venous-ct.auto-segmentation.v1";
    std::uint32_t schemaVersion = 1;

    double candidateVesselnessQuantile = 0.990;
    double coreVesselnessQuantile = 0.9995;
    double candidateMaximumFractionOfMaximum = 0.003;
    double coreMaximumFractionOfMaximum = 0.080;
    std::uint32_t vesselnessHistogramBins = 4096;

    double hardIntensityLower = 120.0;
    double hardIntensityUpper = 300.0;
    double coreIntensityLowerQuantile = 0.05;
    double coreIntensityUpperQuantile = 0.995;
    double intensityMargin = 20.0;
    std::uint32_t intensityHistogramBins = 2048;
    std::size_t minimumIntensitySampleCount = 64;

    double automaticSeedAnchorNormalized[3] = {0.613, 0.423, 0.258};
    double automaticSeedSearchRadiusNormalized[3] = {0.10, 0.10, 0.18};
    double automaticSeedIntensityLower = 140.0;
    double automaticSeedIntensityUpper = 280.0;
    double automaticSeedDistancePenalty = 0.35;

    double confidenceMultiplier = 2.0;
    std::uint32_t confidenceIterations = 3;
    std::uint32_t confidenceInitialNeighborhoodRadiusVoxels = 1;
    double confidenceExpansionRadiusMm = 0.8;
    double maximumConfidenceForegroundFraction = 0.05;

    double closingRadiusMm = 0.0;
    double openingRadiusMm = 0.0;

    double minimumComponentVolumeMm3 = 20.0;
    double minimumCoreVolumeMm3 = 2.0;
    double maximumBoundaryFraction = 0.10;
    double minimumRelativeScore = 0.20;
    double additionalComponentScoreFraction = 1.0;
    std::uint32_t maximumRetainedComponents = 8;
    std::uint32_t maximumReportedComponents = 256;
};

AutomaticVesselSegmentationProfileV1
portalVenousCtAutomaticSegmentationProfileV1();

enum class AutomaticVesselSegmentationProfileValidationCode {
    Ok,
    MissingProfileId,
    UnsupportedSchemaVersion,
    InvalidVesselnessQuantiles,
    InvalidVesselnessFractions,
    InvalidHistogramBins,
    InvalidIntensityBounds,
    InvalidIntensityQuantiles,
    InvalidIntensityMargin,
    InvalidIntensitySampleCount,
    InvalidAutomaticSeedPolicy,
    InvalidConfidenceConnectedPolicy,
    InvalidMorphologyRadius,
    InvalidComponentPolicy
};

AutomaticVesselSegmentationProfileValidationCode
validateAutomaticVesselSegmentationProfile(
    const AutomaticVesselSegmentationProfileV1& profile);
const char* automaticVesselSegmentationProfileValidationToken(
    AutomaticVesselSegmentationProfileValidationCode code);

class AutomaticVesselSegmentationCancellation {
public:
    void requestCancellation() noexcept;
    bool isCancellationRequested() const noexcept;

private:
    std::atomic_bool requested_{false};
};

// The type name is retained for artifact compatibility. Schema 3 is the current
// production profile; schema-2 fields remain solely so artifact v5 is readable.
struct AutomaticVesselSegmentationProfileV2 {
    std::string profileId =
        "xq.portal-venous-ct.auto-segmentation.geodesic-active-contour.roi.v3";
    std::uint32_t schemaVersion = 3;

    double coarseDomainDilationMm = 2.0;
    double edgeSigmaMm = 1.0;
    double propagationScaling = 0.7;
    double advectionScaling = 1.0;
    double curvatureScaling = 1.0;
    double maximumRmsError = 0.02;
    std::uint32_t maximumIterations = 100;
    double isosurfaceValue = 0.0;
    bool useImageSpacing = true;

    // Schema 2 / artifact v5 legacy fields. The schema-3 segmenter ignores
    // these values and never enters the TubeTK execution path.
    bool makeHighResolutionIsotropic = true;
    double vesselnessMaskMinimum = 0.0;
    double vesselnessMaskMaximum = 1000.0;
    double inputBlurSigmaMm = 0.4;
    double inputWindowMinimum = 0.5;
    double inputWindowMaximum = 300.0;
    double inputWindowOutputMinimum = 0.0;
    double inputWindowOutputMaximum = 300.0;

    double seedBlurSigmaMm = 0.4;
    double seedMaskRangeMinimumMm = 0.1;
    double seedMaskRangeMaximumMm = 10.0;
    double seedExtractionMinimumProbability = 0.4;

    double minimumCurvature = 0.0;
    double minimumRoundness = 0.02;
    double minimumRidgeness = 0.5;
    double minimumLevelness = 0.0;
    double radiusInObjectSpaceMm = 0.8;
    std::uint32_t borderInIndexSpace = 3;
    bool optimizeRadius = true;
    bool useSeedMaskAsProbabilities = true;
    bool rasterizeWithRadius = true;
};

AutomaticVesselSegmentationProfileV2
portalVenousCtAutomaticSegmentationProfileV2();

enum class AutomaticVesselSegmentationProfileV2ValidationCode {
    Ok,
    MissingProfileId,
    UnsupportedSchemaVersion,
    InvalidWorkingGridPolicy,
    InvalidInputPreparationPolicy,
    InvalidSeedPolicy,
    InvalidTubeExtractionPolicy,
    InvalidRasterizationPolicy,
    InvalidDomainPolicy,
    InvalidEdgePotentialPolicy,
    InvalidLevelSetPolicy
};

AutomaticVesselSegmentationProfileV2ValidationCode
validateAutomaticVesselSegmentationProfileV2(
    const AutomaticVesselSegmentationProfileV2& profile);
const char* automaticVesselSegmentationProfileV2ValidationToken(
    AutomaticVesselSegmentationProfileV2ValidationCode code);

enum class AutomaticVesselSegmentationStatus {
    Ok,
    InvalidArgument,
    InvalidProfile,
    InvalidGeometry,
    InvalidSource,
    InvalidVesselness,
    InvalidRoiPrior,
    LineageMismatch,
    UnsupportedInput,
    Cancelled,
    AllocationFailed,
    ProcessingFailed,
    EmptyDomain,
    EmptySeed,
    EmptyTubeGroup,
    EmptyInitialSurface,
    EmptyEdgePotential,
    EmptyOutput
};

enum class AutomaticVesselSegmentationStage {
    None,
    ValidateInput,
    AcquireInput,
    ImportImages,
    BuildWorkingGrid,
    PrepareTubeInput,
    PrepareSeedMask,
    ExtractTubes,
    RasterizeTubes,
    BackMapOutput,
    BuildDomain,
    InitializeLevelSet,
    BuildEdgePotential,
    EvolveLevelSet,
    ThresholdLevelSet,
    ConnectedComponents,
    MaterializeOutput,
    Complete
};

enum class AutomaticVesselSegmentationDiagnosticCode {
    InvalidArgument,
    InvalidProfile,
    MissingGeometry,
    UnsupportedCoordinateSystem,
    InvalidDimensions,
    NonFiniteGeometry,
    NonPositiveSpacing,
    SingularDirection,
    GeometryMismatch,
    NotSingleComponent,
    UnsupportedScalarType,
    SourceMetadataInvalid,
    SourceMetadataMismatch,
    SourceAcquireFailed,
    SourceByteSizeMismatch,
    InputScalarNonFinite,
    VesselnessInvalid,
    VesselnessBufferInvalid,
    VesselnessScalarNonFinite,
    InvalidRoiPrior,
    MissingRoiRole,
    RoiGeometryMismatch,
    InputFingerprintMismatch,
    VesselnessFingerprintMismatch,
    RoiFingerprintMismatch,
    EmptyRoiDomain,
    EmptySeedMask,
    EmptyTubeGroup,
    EmptyInitialSurface,
    EmptyEdgePotential,
    EmptyRasterizedTubes,
    AllForegroundOutput,
    ItkWorkingGridFailed,
    ItkInputPreparationFailed,
    ItkSeedPreparationFailed,
    TubeTkExtractionFailed,
    TubeTkRasterizationFailed,
    ItkBackMappingFailed,
    ItkDomainCompositionFailed,
    ItkInitialSurfaceFailed,
    ItkEdgePotentialFailed,
    ItkLevelSetFailed,
    ItkLevelSetThresholdFailed,
    ItkConnectedComponentsFailed,
    Cancelled,
    AllocationFailed,
    ItkException,
    UnexpectedException,
    OutputGeometryMismatch,
    OutputMaskInvalid,
    EmptyOutput
};

const char* automaticVesselSegmentationStatusToken(
    AutomaticVesselSegmentationStatus status);
const char* automaticVesselSegmentationStageToken(
    AutomaticVesselSegmentationStage stage);
const char* automaticVesselSegmentationDiagnosticToken(
    AutomaticVesselSegmentationDiagnosticCode code);

struct AutomaticVesselSegmentationDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    AutomaticVesselSegmentationDiagnosticCode code =
        AutomaticVesselSegmentationDiagnosticCode::InvalidArgument;
    AutomaticVesselSegmentationStage stage =
        AutomaticVesselSegmentationStage::None;
    std::string messageKey;
    std::vector<double> numericContext;
};

struct AutomaticVesselFilterParameter {
    std::string name;
    double value = 0.0;
    std::string unit;

    bool isValid() const;
};

struct AutomaticVesselUpstreamStage {
    std::string stageId;
    std::string implementation;
    std::string implementationVersion;
    std::vector<AutomaticVesselFilterParameter> parameters;

    bool isValid() const;
};

struct XQAutomaticVesselSegmentationV2 {
    std::shared_ptr<XQSegmentationMask> mask;
    XQVascularRoiPriorV1 roiPrior;
    AutomaticVesselSegmentationProfileV2 profile;
    std::vector<AutomaticVesselUpstreamStage> upstreamStages;
    std::vector<std::size_t> componentVoxelCounts;

    std::string inputFingerprint;
    std::string vesselnessFingerprint;
    std::string roiPriorFingerprint;
    std::string profileFingerprint;
    std::string outputFingerprint;
    std::string algorithmId;
    std::string algorithmVersion;
    std::string itkVersion;
    // Present only when decoding legacy schema-2 artifact v5.
    std::string tubeTkVersion;
    bool hasDicomIdentity = false;
    DicomSeriesIdentity dicomIdentity;

    double workingSpacingMm = 0.0;
    int workingDimensions[3] = {0, 0, 0};
    std::size_t workingVoxelCount = 0;
    std::size_t domainVoxelCount = 0;
    std::size_t seedVoxelCount = 0;
    std::size_t extractedTubeCount = 0;
    std::size_t extractedTubePointCount = 0;
    std::size_t workingRasterizedVoxelCount = 0;
    std::size_t initialSurfaceVoxelCount = 0;
    std::size_t edgePotentialPositiveVoxelCount = 0;
    double edgePotentialMinimum = 0.0;
    double edgePotentialMaximum = 0.0;
    std::uint32_t levelSetElapsedIterations = 0;
    double levelSetRmsChange = 0.0;
    bool levelSetConverged = false;
    unsigned int coarseDomainDilationRadiusVoxels[3] = {0, 0, 0};
    std::size_t foregroundVoxelCount = 0;
    std::size_t componentCount = 0;
    double elapsedMilliseconds = 0.0;

    bool isValid() const;
};

struct AutomaticVesselSegmentationResult {
    AutomaticVesselSegmentationStatus status =
        AutomaticVesselSegmentationStatus::InvalidArgument;
    AutomaticVesselSegmentationStage stage =
        AutomaticVesselSegmentationStage::None;
    double elapsedMilliseconds = 0.0;
    std::optional<XQAutomaticVesselSegmentationV2> output;
    std::vector<AutomaticVesselSegmentationDiagnostic> diagnostics;

    bool ok() const;
};

class IAutomaticVesselSegmenter {
public:
    virtual ~IAutomaticVesselSegmenter() = default;

    virtual AutomaticVesselSegmentationResult run(
        const XQImageVolume& image,
        const IVoxelSource& source,
        const XQVesselnessVolume& vesselness,
        const XQVascularRoiPriorV1& roiPrior,
        const AutomaticVesselSegmentationProfileV2& profile,
        const AutomaticVesselSegmentationCancellation* cancellation = nullptr) const = 0;
};

// Legacy artifact model. It is never returned by the schema-3 product segmenter.
enum class AutomaticVesselComponentDecision {
    Selected,
    NoCoreContact,
    BelowMinimumVolume,
    BelowMinimumCoreVolume,
    BoundaryDominated,
    BelowRelativeScore,
    RetentionLimit
};

const char* automaticVesselComponentDecisionToken(
    AutomaticVesselComponentDecision decision);

struct AutomaticVesselComponentRecord {
    std::uint32_t label = 0;
    std::size_t voxelCount = 0;
    std::size_t coreVoxelCount = 0;
    double physicalVolumeMm3 = 0.0;
    double coreVolumeMm3 = 0.0;
    std::size_t boundaryVoxelCount = 0;
    double boundaryFraction = 0.0;
    double meanVesselness = 0.0;
    double maximumVesselness = 0.0;
    int minimumIndex[3] = {0, 0, 0};
    int maximumIndex[3] = {0, 0, 0};
    double physicalExtentMm[3] = {0.0, 0.0, 0.0};
    double centroidIndex[3] = {0.0, 0.0, 0.0};
    double automaticSeedDistanceNormalized = 0.0;
    double elongation = 0.0;
    double score = 0.0;
    bool containsAutomaticSeed = false;
    bool selected = false;
    AutomaticVesselComponentDecision decision =
        AutomaticVesselComponentDecision::NoCoreContact;
};

struct XQAutomaticVesselSegmentation {
    std::shared_ptr<XQSegmentationMask> mask;
    AutomaticVesselSegmentationProfileV1 profile;
    std::vector<AutomaticVesselComponentRecord> components;

    std::string inputFingerprint;
    std::string vesselnessFingerprint;
    std::string profileFingerprint;
    std::string outputFingerprint;
    std::string algorithmId;
    std::string algorithmVersion;
    std::string itkVersion;
    bool hasDicomIdentity = false;
    DicomSeriesIdentity dicomIdentity;

    double candidateVesselnessThreshold = 0.0;
    double coreVesselnessThreshold = 0.0;
    double automaticIntensityLower = 0.0;
    double automaticIntensityUpper = 0.0;
    int automaticSeedIndex[3] = {0, 0, 0};
    double automaticSeedIntensity = 0.0;
    double automaticSeedVesselness = 0.0;
    std::uint32_t automaticSeedComponentLabel = 0;
    double confidenceMean = 0.0;
    double confidenceVariance = 0.0;
    std::size_t confidenceVoxelCount = 0;
    std::size_t coreVoxelCount = 0;
    std::size_t rawCandidateVoxelCount = 0;
    std::size_t candidateVoxelCount = 0;
    std::size_t componentCount = 0;
    std::size_t retainedComponentCount = 0;
    std::size_t foregroundVoxelCount = 0;
    unsigned int closingRadiusVoxels[3] = {0, 0, 0};
    unsigned int openingRadiusVoxels[3] = {0, 0, 0};
    unsigned int confidenceExpansionRadiusVoxels[3] = {0, 0, 0};
    bool itkThresholdExecuted = false;
    bool itkConfidenceConnectedExecuted = false;
    bool itkConfidenceExpansionExecuted = false;
    bool itkMorphologyExecuted = false;
    bool itkConnectedComponentsExecuted = false;
    bool fullyConnected = false;
    double elapsedMilliseconds = 0.0;

    bool isValid() const;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_I_AUTOMATIC_VESSEL_SEGMENTER_H
