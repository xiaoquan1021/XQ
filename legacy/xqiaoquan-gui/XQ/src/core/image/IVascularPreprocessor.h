#ifndef XQ_CORE_IMAGE_I_VASCULAR_PREPROCESSOR_H
#define XQ_CORE_IMAGE_I_VASCULAR_PREPROCESSOR_H

#include "core/Diagnostics.h"
#include "core/XQImageVolume.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class IVoxelSource;
class XQMemoryImageBufferHandle;

enum class VascularSigmaStepMethod {
    Logarithmic,
    Equispaced
};

// Versioned physical-parameter contract for the first production CT/CTA
// preprocessing baseline. Spatial scales are millimetres.
struct VascularPreprocessProfileV1 {
    std::string profileId = "xq.portal-venous-ct.preprocess.v1";
    std::uint32_t schemaVersion = 1;

    std::uint32_t diffusionIterations = 5;
    double diffusionTimeStep = 0.03;
    double diffusionConductance = 3.0;

    double sigmaMinimumMm = 0.6;
    double sigmaMaximumMm = 4.0;
    std::uint32_t sigmaSteps = 6;
    VascularSigmaStepMethod sigmaStepMethod = VascularSigmaStepMethod::Logarithmic;

    double alpha = 0.5;
    double beta = 0.5;
    double gamma = 5.0;
    bool brightObject = true;
    bool scaleObjectness = true;
};

VascularPreprocessProfileV1 portalVenousCtPreprocessProfileV1();

enum class VascularPreprocessProfileValidationCode {
    Ok,
    MissingProfileId,
    UnsupportedSchemaVersion,
    InvalidDiffusionIterations,
    InvalidDiffusionTimeStep,
    InvalidDiffusionConductance,
    InvalidSigmaRange,
    InvalidSigmaSteps,
    InvalidSigmaStepMethod,
    InvalidObjectnessParameters
};

VascularPreprocessProfileValidationCode validateVascularPreprocessProfile(
    const VascularPreprocessProfileV1& profile);
const char* vascularPreprocessProfileValidationToken(
    VascularPreprocessProfileValidationCode code);

// Shared between a service worker and its caller. The adapter checks this flag
// before acquisition and between the materialized ITK stages.
class VascularPreprocessCancellation {
public:
    void requestCancellation() noexcept;
    bool isCancellationRequested() const noexcept;

private:
    std::atomic_bool requested_{false};
};

enum class VascularPreprocessStatus {
    Ok,
    InvalidArgument,
    InvalidProfile,
    InvalidGeometry,
    InvalidSource,
    UnsupportedInput,
    Cancelled,
    AllocationFailed,
    ProcessingFailed,
    EmptyOutput
};

enum class VascularPreprocessStage {
    None,
    ValidateInput,
    AcquireInput,
    ImportImage,
    Diffusion,
    Vesselness,
    MaterializeOutput,
    Complete
};

enum class VascularPreprocessDiagnosticCode {
    InvalidArgument,
    InvalidProfile,
    MissingGeometry,
    UnsupportedCoordinateSystem,
    InvalidDimensions,
    NonFiniteGeometry,
    NonPositiveSpacing,
    SingularDirection,
    NotSingleComponent,
    UnsupportedScalarType,
    SourceMetadataInvalid,
    SourceMetadataMismatch,
    SourceAcquireFailed,
    SourceByteSizeMismatch,
    InputScalarNonFinite,
    InputScalarOutOfFloatRange,
    DiffusionTimeStepUnstable,
    Cancelled,
    AllocationFailed,
    ItkException,
    UnexpectedException,
    OutputGeometryMismatch,
    OutputBufferInvalid,
    OutputScalarNonFinite,
    EmptyVesselness
};

const char* vascularPreprocessStatusToken(VascularPreprocessStatus status);
const char* vascularPreprocessStageToken(VascularPreprocessStage stage);
const char* vascularPreprocessDiagnosticToken(
    VascularPreprocessDiagnosticCode code);

struct VascularPreprocessDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    VascularPreprocessDiagnosticCode code =
        VascularPreprocessDiagnosticCode::InvalidArgument;
    VascularPreprocessStage stage = VascularPreprocessStage::None;
    std::string messageKey;
    std::vector<double> numericContext;
};

// Complete XQ-owned output. The buffer is immutable after construction and the
// ITK pipeline has been destroyed before this value crosses the adapter boundary.
struct XQVesselnessVolume {
    XQImageVolume image;
    std::shared_ptr<const XQMemoryImageBufferHandle> buffer;
    VascularPreprocessProfileV1 profile;

    std::string inputFingerprint;
    std::string profileFingerprint;
    std::string outputFingerprint;
    std::string algorithmId;
    std::string algorithmVersion;
    std::string itkVersion;

    double inputScalarMinimum = 0.0;
    double inputScalarMaximum = 0.0;
    double scalarMinimum = 0.0;
    double scalarMaximum = 0.0;
    std::size_t positiveVoxelCount = 0;
    double elapsedMilliseconds = 0.0;

    bool isValid() const;
};

struct VascularPreprocessResult {
    VascularPreprocessStatus status = VascularPreprocessStatus::InvalidArgument;
    VascularPreprocessStage stage = VascularPreprocessStage::None;
    double elapsedMilliseconds = 0.0;
    std::optional<XQVesselnessVolume> output;
    std::vector<VascularPreprocessDiagnostic> diagnostics;

    bool ok() const;
};

class IVascularPreprocessor {
public:
    virtual ~IVascularPreprocessor() = default;

    virtual VascularPreprocessResult run(
        const XQImageVolume& image,
        const IVoxelSource& source,
        const VascularPreprocessProfileV1& profile,
        const VascularPreprocessCancellation* cancellation = nullptr) const = 0;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_I_VASCULAR_PREPROCESSOR_H
