#include "core/image/IVascularPreprocessor.h"

#include "core/XQMemoryImageBufferHandle.h"

#include <cmath>

namespace xq {

VascularPreprocessProfileV1 portalVenousCtPreprocessProfileV1()
{
    return VascularPreprocessProfileV1{};
}

VascularPreprocessProfileValidationCode validateVascularPreprocessProfile(
    const VascularPreprocessProfileV1& profile)
{
    if (profile.profileId.empty()) {
        return VascularPreprocessProfileValidationCode::MissingProfileId;
    }
    if (profile.schemaVersion != 1) {
        return VascularPreprocessProfileValidationCode::UnsupportedSchemaVersion;
    }
    if (profile.diffusionIterations == 0 || profile.diffusionIterations > 1000) {
        return VascularPreprocessProfileValidationCode::InvalidDiffusionIterations;
    }
    if (!std::isfinite(profile.diffusionTimeStep)
        || profile.diffusionTimeStep <= 0.0
        || profile.diffusionTimeStep > 0.0625) {
        return VascularPreprocessProfileValidationCode::InvalidDiffusionTimeStep;
    }
    if (!std::isfinite(profile.diffusionConductance)
        || profile.diffusionConductance <= 0.0) {
        return VascularPreprocessProfileValidationCode::InvalidDiffusionConductance;
    }
    if (!std::isfinite(profile.sigmaMinimumMm)
        || !std::isfinite(profile.sigmaMaximumMm)
        || profile.sigmaMinimumMm <= 0.0
        || profile.sigmaMaximumMm < profile.sigmaMinimumMm) {
        return VascularPreprocessProfileValidationCode::InvalidSigmaRange;
    }
    if (profile.sigmaSteps == 0 || profile.sigmaSteps > 64
        || (profile.sigmaMaximumMm > profile.sigmaMinimumMm
            && profile.sigmaSteps < 2)) {
        return VascularPreprocessProfileValidationCode::InvalidSigmaSteps;
    }
    switch (profile.sigmaStepMethod) {
    case VascularSigmaStepMethod::Logarithmic:
    case VascularSigmaStepMethod::Equispaced:
        break;
    default:
        return VascularPreprocessProfileValidationCode::InvalidSigmaStepMethod;
    }
    if (!std::isfinite(profile.alpha) || profile.alpha <= 0.0
        || !std::isfinite(profile.beta) || profile.beta <= 0.0
        || !std::isfinite(profile.gamma) || profile.gamma <= 0.0) {
        return VascularPreprocessProfileValidationCode::InvalidObjectnessParameters;
    }
    return VascularPreprocessProfileValidationCode::Ok;
}

const char* vascularPreprocessProfileValidationToken(
    VascularPreprocessProfileValidationCode code)
{
    switch (code) {
    case VascularPreprocessProfileValidationCode::Ok: return "ok";
    case VascularPreprocessProfileValidationCode::MissingProfileId:
        return "missing_profile_id";
    case VascularPreprocessProfileValidationCode::UnsupportedSchemaVersion:
        return "unsupported_schema_version";
    case VascularPreprocessProfileValidationCode::InvalidDiffusionIterations:
        return "invalid_diffusion_iterations";
    case VascularPreprocessProfileValidationCode::InvalidDiffusionTimeStep:
        return "invalid_diffusion_time_step";
    case VascularPreprocessProfileValidationCode::InvalidDiffusionConductance:
        return "invalid_diffusion_conductance";
    case VascularPreprocessProfileValidationCode::InvalidSigmaRange:
        return "invalid_sigma_range";
    case VascularPreprocessProfileValidationCode::InvalidSigmaSteps:
        return "invalid_sigma_steps";
    case VascularPreprocessProfileValidationCode::InvalidSigmaStepMethod:
        return "invalid_sigma_step_method";
    case VascularPreprocessProfileValidationCode::InvalidObjectnessParameters:
        return "invalid_objectness_parameters";
    }
    return "unknown";
}

void VascularPreprocessCancellation::requestCancellation() noexcept
{
    requested_.store(true, std::memory_order_relaxed);
}

bool VascularPreprocessCancellation::isCancellationRequested() const noexcept
{
    return requested_.load(std::memory_order_relaxed);
}

const char* vascularPreprocessStatusToken(VascularPreprocessStatus status)
{
    switch (status) {
    case VascularPreprocessStatus::Ok: return "ok";
    case VascularPreprocessStatus::InvalidArgument: return "invalid_argument";
    case VascularPreprocessStatus::InvalidProfile: return "invalid_profile";
    case VascularPreprocessStatus::InvalidGeometry: return "invalid_geometry";
    case VascularPreprocessStatus::InvalidSource: return "invalid_source";
    case VascularPreprocessStatus::UnsupportedInput: return "unsupported_input";
    case VascularPreprocessStatus::Cancelled: return "cancelled";
    case VascularPreprocessStatus::AllocationFailed: return "allocation_failed";
    case VascularPreprocessStatus::ProcessingFailed: return "processing_failed";
    case VascularPreprocessStatus::EmptyOutput: return "empty_output";
    }
    return "unknown";
}

const char* vascularPreprocessStageToken(VascularPreprocessStage stage)
{
    switch (stage) {
    case VascularPreprocessStage::None: return "none";
    case VascularPreprocessStage::ValidateInput: return "validate_input";
    case VascularPreprocessStage::AcquireInput: return "acquire_input";
    case VascularPreprocessStage::ImportImage: return "import_image";
    case VascularPreprocessStage::Diffusion: return "diffusion";
    case VascularPreprocessStage::Vesselness: return "vesselness";
    case VascularPreprocessStage::MaterializeOutput: return "materialize_output";
    case VascularPreprocessStage::Complete: return "complete";
    }
    return "unknown";
}

const char* vascularPreprocessDiagnosticToken(
    VascularPreprocessDiagnosticCode code)
{
    switch (code) {
    case VascularPreprocessDiagnosticCode::InvalidArgument:
        return "vascular_preprocess.invalid_argument";
    case VascularPreprocessDiagnosticCode::InvalidProfile:
        return "vascular_preprocess.invalid_profile";
    case VascularPreprocessDiagnosticCode::MissingGeometry:
        return "vascular_preprocess.missing_geometry";
    case VascularPreprocessDiagnosticCode::UnsupportedCoordinateSystem:
        return "vascular_preprocess.unsupported_coordinate_system";
    case VascularPreprocessDiagnosticCode::InvalidDimensions:
        return "vascular_preprocess.invalid_dimensions";
    case VascularPreprocessDiagnosticCode::NonFiniteGeometry:
        return "vascular_preprocess.non_finite_geometry";
    case VascularPreprocessDiagnosticCode::NonPositiveSpacing:
        return "vascular_preprocess.non_positive_spacing";
    case VascularPreprocessDiagnosticCode::SingularDirection:
        return "vascular_preprocess.singular_direction";
    case VascularPreprocessDiagnosticCode::NotSingleComponent:
        return "vascular_preprocess.not_single_component";
    case VascularPreprocessDiagnosticCode::UnsupportedScalarType:
        return "vascular_preprocess.unsupported_scalar_type";
    case VascularPreprocessDiagnosticCode::SourceMetadataInvalid:
        return "vascular_preprocess.source_metadata_invalid";
    case VascularPreprocessDiagnosticCode::SourceMetadataMismatch:
        return "vascular_preprocess.source_metadata_mismatch";
    case VascularPreprocessDiagnosticCode::SourceAcquireFailed:
        return "vascular_preprocess.source_acquire_failed";
    case VascularPreprocessDiagnosticCode::SourceByteSizeMismatch:
        return "vascular_preprocess.source_byte_size_mismatch";
    case VascularPreprocessDiagnosticCode::InputScalarNonFinite:
        return "vascular_preprocess.input_scalar_non_finite";
    case VascularPreprocessDiagnosticCode::InputScalarOutOfFloatRange:
        return "vascular_preprocess.input_scalar_out_of_float_range";
    case VascularPreprocessDiagnosticCode::DiffusionTimeStepUnstable:
        return "vascular_preprocess.diffusion_time_step_unstable";
    case VascularPreprocessDiagnosticCode::Cancelled:
        return "vascular_preprocess.cancelled";
    case VascularPreprocessDiagnosticCode::AllocationFailed:
        return "vascular_preprocess.allocation_failed";
    case VascularPreprocessDiagnosticCode::ItkException:
        return "vascular_preprocess.itk_exception";
    case VascularPreprocessDiagnosticCode::UnexpectedException:
        return "vascular_preprocess.unexpected_exception";
    case VascularPreprocessDiagnosticCode::OutputGeometryMismatch:
        return "vascular_preprocess.output_geometry_mismatch";
    case VascularPreprocessDiagnosticCode::OutputBufferInvalid:
        return "vascular_preprocess.output_buffer_invalid";
    case VascularPreprocessDiagnosticCode::OutputScalarNonFinite:
        return "vascular_preprocess.output_scalar_non_finite";
    case VascularPreprocessDiagnosticCode::EmptyVesselness:
        return "vascular_preprocess.empty_vesselness";
    }
    return "vascular_preprocess.unknown";
}

bool XQVesselnessVolume::isValid() const
{
    if (!image.hasGeometry()
        || image.geometry().coordinateSystem != ImageCoordinateSystem::LPS
        || image.scalarType() != ScalarType::Float32
        || image.componentCount() != 1 || buffer == nullptr
        || !buffer->is_valid()
        || validateVascularPreprocessProfile(profile)
            != VascularPreprocessProfileValidationCode::Ok) {
        return false;
    }
    const ImageGeometry& geometry = image.geometry();
    if (buffer->dimensionX() != geometry.dimensions[0]
        || buffer->dimensionY() != geometry.dimensions[1]
        || buffer->dimensionZ() != geometry.dimensions[2]
        || buffer->scalarType() != ScalarType::Float32
        || buffer->componentCount() != 1) {
        return false;
    }
    return !inputFingerprint.empty() && !profileFingerprint.empty()
        && !outputFingerprint.empty() && !algorithmId.empty()
        && !algorithmVersion.empty() && !itkVersion.empty()
        && std::isfinite(inputScalarMinimum)
        && std::isfinite(inputScalarMaximum)
        && inputScalarMaximum >= inputScalarMinimum
        && std::isfinite(scalarMinimum) && std::isfinite(scalarMaximum)
        && scalarMaximum >= scalarMinimum && positiveVoxelCount > 0
        && positiveVoxelCount <= buffer->voxelCount()
        && std::isfinite(elapsedMilliseconds) && elapsedMilliseconds >= 0.0;
}

bool VascularPreprocessResult::ok() const
{
    return status == VascularPreprocessStatus::Ok
        && stage == VascularPreprocessStage::Complete
        && output.has_value() && output->isValid();
}

} // namespace xq
