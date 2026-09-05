#include "core/image/IVascularSegmentationEvaluator.h"

#include <cmath>

namespace xq {

const char* vascularSegmentationEvaluationStatusToken(
    VascularSegmentationEvaluationStatus status)
{
    switch (status) {
    case VascularSegmentationEvaluationStatus::Ok: return "ok";
    case VascularSegmentationEvaluationStatus::InvalidArgument:
        return "invalid_argument";
    case VascularSegmentationEvaluationStatus::InvalidPrediction:
        return "invalid_prediction";
    case VascularSegmentationEvaluationStatus::InvalidReference:
        return "invalid_reference";
    case VascularSegmentationEvaluationStatus::GeometryMismatch:
        return "geometry_mismatch";
    case VascularSegmentationEvaluationStatus::EmptyPrediction:
        return "empty_prediction";
    case VascularSegmentationEvaluationStatus::EmptyReference:
        return "empty_reference";
    case VascularSegmentationEvaluationStatus::ProcessingFailed:
        return "processing_failed";
    }
    return "unknown";
}

const char* vascularSegmentationEvaluationStageToken(
    VascularSegmentationEvaluationStage stage)
{
    switch (stage) {
    case VascularSegmentationEvaluationStage::None: return "none";
    case VascularSegmentationEvaluationStage::ValidateInput:
        return "validate_input";
    case VascularSegmentationEvaluationStage::Overlap: return "overlap";
    case VascularSegmentationEvaluationStage::ConnectedComponents:
        return "connected_components";
    case VascularSegmentationEvaluationStage::SurfaceDistance:
        return "surface_distance";
    case VascularSegmentationEvaluationStage::Skeletonize:
        return "skeletonize";
    case VascularSegmentationEvaluationStage::Complete: return "complete";
    }
    return "unknown";
}

const char* vascularSegmentationEvaluationDiagnosticToken(
    VascularSegmentationEvaluationDiagnosticCode code)
{
    switch (code) {
    case VascularSegmentationEvaluationDiagnosticCode::InvalidArgument:
        return "vascular_segmentation_evaluation.invalid_argument";
    case VascularSegmentationEvaluationDiagnosticCode::InvalidPrediction:
        return "vascular_segmentation_evaluation.invalid_prediction";
    case VascularSegmentationEvaluationDiagnosticCode::InvalidReference:
        return "vascular_segmentation_evaluation.invalid_reference";
    case VascularSegmentationEvaluationDiagnosticCode::GeometryMismatch:
        return "vascular_segmentation_evaluation.geometry_mismatch";
    case VascularSegmentationEvaluationDiagnosticCode::UnsupportedLabelValue:
        return "vascular_segmentation_evaluation.unsupported_label_value";
    case VascularSegmentationEvaluationDiagnosticCode::EmptyPrediction:
        return "vascular_segmentation_evaluation.empty_prediction";
    case VascularSegmentationEvaluationDiagnosticCode::EmptyReference:
        return "vascular_segmentation_evaluation.empty_reference";
    case VascularSegmentationEvaluationDiagnosticCode::EmptySurface:
        return "vascular_segmentation_evaluation.empty_surface";
    case VascularSegmentationEvaluationDiagnosticCode::EmptySkeleton:
        return "vascular_segmentation_evaluation.empty_skeleton";
    case VascularSegmentationEvaluationDiagnosticCode::ItkException:
        return "vascular_segmentation_evaluation.itk_exception";
    case VascularSegmentationEvaluationDiagnosticCode::UnexpectedException:
        return "vascular_segmentation_evaluation.unexpected_exception";
    }
    return "vascular_segmentation_evaluation.unknown";
}

bool VascularSegmentationMetrics::isValid() const
{
    return !evaluatorId.empty() && !evaluatorVersion.empty()
        && !itkVersion.empty() && !thinningBackendId.empty()
        && !thinningBackendVersion.empty()
        && predictionForegroundVoxelCount > 0
        && referenceForegroundVoxelCount > 0
        && intersectionVoxelCount <= predictionForegroundVoxelCount
        && intersectionVoxelCount <= referenceForegroundVoxelCount
        && std::isfinite(dice) && dice >= 0.0 && dice <= 1.0
        && predictionComponentCount > 0
        && largestPredictionComponentVoxelCount > 0
        && largestPredictionComponentVoxelCount <= predictionForegroundVoxelCount
        && std::isfinite(largestPredictionComponentFraction)
        && largestPredictionComponentFraction > 0.0
        && largestPredictionComponentFraction <= 1.0
        && predictionSurfaceVoxelCount > 0 && referenceSurfaceVoxelCount > 0
        && std::isfinite(predictionToReferenceSurfaceP95Mm)
        && predictionToReferenceSurfaceP95Mm >= 0.0
        && std::isfinite(referenceToPredictionSurfaceP95Mm)
        && referenceToPredictionSurfaceP95Mm >= 0.0
        && std::isfinite(hd95Mm) && hd95Mm >= 0.0
        && std::isfinite(assdMm) && assdMm >= 0.0
        && predictionSkeletonVoxelCount > 0 && referenceSkeletonVoxelCount > 0
        && std::isfinite(topologyPrecision) && topologyPrecision >= 0.0
        && topologyPrecision <= 1.0
        && std::isfinite(topologySensitivity) && topologySensitivity >= 0.0
        && topologySensitivity <= 1.0
        && std::isfinite(clDice) && clDice >= 0.0 && clDice <= 1.0
        && std::isfinite(elapsedMilliseconds) && elapsedMilliseconds >= 0.0;
}

bool VascularSegmentationEvaluationResult::ok() const
{
    return status == VascularSegmentationEvaluationStatus::Ok
        && stage == VascularSegmentationEvaluationStage::Complete
        && metrics.has_value() && metrics->isValid();
}

} // namespace xq
