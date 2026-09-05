#ifndef XQ_CORE_IMAGE_I_VASCULAR_SEGMENTATION_EVALUATOR_H
#define XQ_CORE_IMAGE_I_VASCULAR_SEGMENTATION_EVALUATOR_H

#include "core/Diagnostics.h"
#include "core/XQSegmentationMask.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xq {

enum class VascularSegmentationEvaluationStatus {
    Ok,
    InvalidArgument,
    InvalidPrediction,
    InvalidReference,
    GeometryMismatch,
    EmptyPrediction,
    EmptyReference,
    ProcessingFailed
};

enum class VascularSegmentationEvaluationStage {
    None,
    ValidateInput,
    Overlap,
    ConnectedComponents,
    SurfaceDistance,
    Skeletonize,
    Complete
};

enum class VascularSegmentationEvaluationDiagnosticCode {
    InvalidArgument,
    InvalidPrediction,
    InvalidReference,
    GeometryMismatch,
    UnsupportedLabelValue,
    EmptyPrediction,
    EmptyReference,
    EmptySurface,
    EmptySkeleton,
    ItkException,
    UnexpectedException
};

const char* vascularSegmentationEvaluationStatusToken(
    VascularSegmentationEvaluationStatus status);
const char* vascularSegmentationEvaluationStageToken(
    VascularSegmentationEvaluationStage stage);
const char* vascularSegmentationEvaluationDiagnosticToken(
    VascularSegmentationEvaluationDiagnosticCode code);

struct VascularSegmentationEvaluationDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    VascularSegmentationEvaluationDiagnosticCode code =
        VascularSegmentationEvaluationDiagnosticCode::InvalidArgument;
    VascularSegmentationEvaluationStage stage =
        VascularSegmentationEvaluationStage::None;
    std::string messageKey;
    std::vector<double> numericContext;
};

struct VascularSegmentationMetrics {
    std::string evaluatorId;
    std::string evaluatorVersion;
    std::string itkVersion;
    std::string thinningBackendId;
    std::string thinningBackendVersion;

    std::size_t predictionForegroundVoxelCount = 0;
    std::size_t referenceForegroundVoxelCount = 0;
    std::size_t intersectionVoxelCount = 0;
    double dice = 0.0;

    std::size_t predictionComponentCount = 0;
    std::size_t largestPredictionComponentVoxelCount = 0;
    double largestPredictionComponentFraction = 0.0;

    std::size_t predictionSurfaceVoxelCount = 0;
    std::size_t referenceSurfaceVoxelCount = 0;
    double predictionToReferenceSurfaceP95Mm = 0.0;
    double referenceToPredictionSurfaceP95Mm = 0.0;
    double hd95Mm = 0.0;
    double assdMm = 0.0;

    std::size_t predictionSkeletonVoxelCount = 0;
    std::size_t referenceSkeletonVoxelCount = 0;
    double topologyPrecision = 0.0;
    double topologySensitivity = 0.0;
    double clDice = 0.0;
    double elapsedMilliseconds = 0.0;

    bool isValid() const;
};

struct VascularSegmentationEvaluationResult {
    VascularSegmentationEvaluationStatus status =
        VascularSegmentationEvaluationStatus::InvalidArgument;
    VascularSegmentationEvaluationStage stage =
        VascularSegmentationEvaluationStage::None;
    double elapsedMilliseconds = 0.0;
    std::optional<VascularSegmentationMetrics> metrics;
    std::vector<VascularSegmentationEvaluationDiagnostic> diagnostics;

    bool ok() const;
};

class IVascularSegmentationEvaluator {
public:
    virtual ~IVascularSegmentationEvaluator() = default;

    virtual VascularSegmentationEvaluationResult evaluate(
        const XQSegmentationMask& prediction,
        const XQSegmentationMask& reference) const = 0;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_I_VASCULAR_SEGMENTATION_EVALUATOR_H
