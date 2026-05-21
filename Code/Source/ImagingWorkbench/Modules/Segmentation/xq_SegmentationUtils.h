// XQ SegmentationUtils: STL-algorithm-driven geometric computations for contour analysis
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkBaseData.h>
#include <mitkPoint.h>
#include <mitkDataNode.h>
#include <mitkVector.h>
#include <mitkPlaneGeometry.h>
#include <mitkProportionalTimeGeometry.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include <memory>
#include <string_view>
#include <array>
#include <set>
#include <vector>

class xq_LumenProfile;
class xq_ProfileGroup;

struct XQMODULESEGMENTATION_EXPORT xq_ProfileStatistics
{
    int pointCount = 0;
    double perimeter = 0.0;
    double area = 0.0;
    std::array<double, 6> boundingBox = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct XQMODULESEGMENTATION_EXPORT xq_ProfileGroupReadinessReport
{
    bool loftReady = false;
    bool modelingReady = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    int profileCount = 0;
    int missingCount = 0;
    int openContourCount = 0;
};

struct XQMODULESEGMENTATION_EXPORT xq_ProfilePlacementFrame
{
    int pathPosIndex = -1;
    mitk::Point3D position;
    mitk::Vector3D tangent;
    mitk::Vector3D rotation;
};

// ---------------------------------------------------------------------------
// Preprocessing-target context resolution
// ---------------------------------------------------------------------------
//
// Answers three UI questions without touching any Qt type:
//   1. Is there an active preprocessing target?        → IsResolved()
//   2. Should detached preview be blocked?             → ShouldBlockDetachedPreview()
//   3. What explicit reason should be shown to users?  → blockingReason (non-empty when not resolved)
//
// Resolution rules:
//   NoTargetAvailable — group pointer is null; no canonical target exists.
//   TargetIneligible  — group exists but is path-bound (RequiresPathPlacement)
//                       and its loft-readiness is blocked, so it cannot
//                       participate as a canonical preprocessing target yet.
//   TargetReady       — group is usable: either unbound (free to accept any
//                       new profile) or path-bound and loft-ready.

enum class XQMODULESEGMENTATION_EXPORT xq_PreprocTargetState
{
    NoTargetAvailable, // nullptr / no group selected
    TargetIneligible,  // group exists but cannot participate (placement/loft blocked)
    TargetReady        // valid, usable preprocessing target
};

struct XQMODULESEGMENTATION_EXPORT xq_PreprocTargetResolution
{
    xq_PreprocTargetState state = xq_PreprocTargetState::NoTargetAvailable;
    std::string blockingReason; // non-empty when state != TargetReady

    // Returns true when a canonical ProfileGroup target is available and the
    // segmentation result should be written to it instead of a detached node.
    bool IsResolved() const
    {
        return state == xq_PreprocTargetState::TargetReady;
    }

    // Returns true when a canonical target exists and is ready, meaning the
    // view must write to it and must NOT create a detached preview node.
    bool ShouldBlockDetachedPreview() const
    {
        return state == xq_PreprocTargetState::TargetReady;
    }
};

enum class XQMODULESEGMENTATION_EXPORT xq_CanonicalWritebackWarningContext
{
    ThresholdContour,
    AutoSegmentation
};

enum class XQMODULESEGMENTATION_EXPORT xq_CanonicalWritebackWarningReason
{
    None,
    SlicePlacementUnavailable,
    PlacementUnresolved,
    ExtractionYieldedNoValidContour
};

enum class XQMODULESEGMENTATION_EXPORT xq_CanonicalWritebackFlow
{
    Proceed,
    WarnAndContinue,
    WarnAndReturn
};

struct XQMODULESEGMENTATION_EXPORT xq_CanonicalWritebackViewDecision
{
    xq_CanonicalWritebackWarningReason warningReason = xq_CanonicalWritebackWarningReason::None;
    xq_CanonicalWritebackFlow flow = xq_CanonicalWritebackFlow::Proceed;

    bool ShouldWarn() const
    {
        return warningReason != xq_CanonicalWritebackWarningReason::None;
    }

    bool ShouldAttemptCanonicalWriteback() const
    {
        return flow == xq_CanonicalWritebackFlow::Proceed;
    }

    bool ShouldReturnEarly() const
    {
        return flow == xq_CanonicalWritebackFlow::WarnAndReturn;
    }

    bool ShouldCreateSurfacePreview() const
    {
        return flow != xq_CanonicalWritebackFlow::WarnAndReturn;
    }
};

class XQMODULESEGMENTATION_EXPORT xq_SegmentationUtils
{
public:
    static double CalculateContourArea(const std::vector<mitk::Point3D>& contourPoints);

    static double CalculateContourPerimeter(const std::vector<mitk::Point3D>& contourPoints);

    static mitk::Vector3D ComputeContourNormal(const std::vector<mitk::Point3D>& contourPoints);

    static mitk::Point3D ComputeContourCentroid(const std::vector<mitk::Point3D>& contourPoints);

    static bool IsPointInsideContour(const mitk::Point3D& point,
                                      const std::vector<mitk::Point3D>& contourPoints,
                                      const mitk::Vector3D& normal);

    static vtkSmartPointer<vtkPolyData> LoftContours(
        const std::vector<std::vector<mitk::Point3D>>& contourSets);

    static std::vector<mitk::Point3D> ResampleContour(
        const std::vector<mitk::Point3D>& contourPoints,
        int targetPointCount);

    static vtkSmartPointer<vtkPolyData> ContourToPolyData(
        const std::vector<mitk::Point3D>& contourPoints);

    static std::vector<mitk::Point3D> ExtractSurfaceContourOnPlane(
        vtkPolyData* surface,
        const mitk::PlaneGeometry* planeGeometry);

    static mitk::DataNode::Pointer CreateLegacyThresholdContourNode(
        const std::vector<mitk::Point3D>& contourPoints,
        std::string_view parentGroupName,
        int contourIndex);

    static std::unique_ptr<xq_LumenProfile> CreatePresetProfile(
        std::string_view contourType,
        const mitk::Point3D& center,
        double primarySize,
        double secondarySize = 0.0,
        int subdivisionCount = 36);

    static vtkSmartPointer<vtkPolyData> LoftProfileGroup(
        const xq_ProfileGroup* group,
        unsigned int timeStep = 0);

    static std::unique_ptr<xq_LumenProfile> CloneProfileWithOffset(
        const xq_LumenProfile* profile,
        double zOffset);

    static std::unique_ptr<xq_LumenProfile> ScaleProfile(
        const xq_LumenProfile* profile,
        double factor);

    static std::unique_ptr<xq_LumenProfile> CreateEditableProfile(
        std::string_view contourType,
        const mitk::Point3D& center);

    static std::unique_ptr<xq_LumenProfile> CreateProfileFromContourPoints(
        const std::vector<mitk::Point3D>& contourPoints,
        std::string_view method,
        std::string_view contourType = "Polygon");

    static bool FindNearestPlacementFrame(
        const std::vector<xq_ProfilePlacementFrame>& frames,
        const mitk::Point3D& referencePoint,
        xq_ProfilePlacementFrame& outFrame);

    static bool FindNearestPlacementFrameIndex(
        const std::vector<xq_ProfilePlacementFrame>& frames,
        const mitk::Point3D& referencePoint,
        int& outFrameIndex,
        const std::set<int>* excludedFrameIndices = nullptr);

    static mitk::PlaneGeometry::Pointer CreatePlacementPlane(
        const xq_ProfilePlacementFrame& frame,
        const mitk::PlaneGeometry* referencePlane = nullptr);
    static mitk::PlaneGeometry::Pointer CreateReslicePlaneGeometry(
        const xq_ProfilePlacementFrame& frame,
        mitk::BaseData* baseData,
        double size,
        bool useOnlyMinimumSpacing = false);
    static mitk::ProportionalTimeGeometry::Pointer CreateSlicedGeometry(
        const std::vector<xq_ProfilePlacementFrame>& frames,
        mitk::BaseData* baseData,
        double size,
        bool useOnlyMinimumSpacing = false);

    static void ApplyPlacementFrame(
        xq_LumenProfile* profile,
        const xq_ProfilePlacementFrame& frame,
        const mitk::PlaneGeometry* referencePlane = nullptr);

    static bool RebindProfileGroupToPlacementFrames(
        xq_ProfileGroup* group,
        const std::vector<xq_ProfilePlacementFrame>& frames);

    static bool RequiresPathPlacement(const xq_ProfileGroup* group);

    static bool ResolveCanonicalPathPosIndex(
        const xq_ProfileGroup* group,
        const xq_ProfilePlacementFrame* placementFrame,
        int& outPathPosIndex);

    static std::string GetLoftReadinessBlockingReason(
        const xq_ProfileGroup* group,
        unsigned int timeStep = 0);

    // Returns a non-empty human-readable reason when a canonical profile group
    // is not yet finalized for modeling: either its loft cache is dirty (profiles
    // were edited since the last stored loft) or no stored loft exists at all.
    // An empty return means the group carries a clean, up-to-date loft and is
    // ready to use as input to the modeling phase.
    static std::string GetModelingPhaseBoundaryBlockingReason(
        const xq_ProfileGroup* group,
        unsigned int timeStep = 0);

    static xq_CanonicalWritebackWarningReason GetCanonicalWritebackWarningReason(
        const xq_ProfileGroup* group,
        bool hasReferencePlane,
        bool hasPlacementFrame);

    static std::string_view GetCanonicalWritebackWarningMessage(
        xq_CanonicalWritebackWarningContext context,
        xq_CanonicalWritebackWarningReason reason);

    static xq_CanonicalWritebackViewDecision GetThresholdContourWritebackDecision(
        const xq_ProfileGroup* group,
        bool hasReferencePlane,
        bool hasPlacementFrame);

    static xq_CanonicalWritebackViewDecision GetAutoSegmentationWritebackDecision(
        const xq_ProfileGroup* group,
        bool hasReferencePlane,
        bool hasPlacementFrame);

    static xq_CanonicalWritebackViewDecision GetAutoSegmentationWritebackDecisionForPlacement(
        const xq_ProfileGroup* group,
        const mitk::PlaneGeometry* referencePlane,
        const xq_ProfilePlacementFrame* placementFrame);

    static xq_CanonicalWritebackViewDecision ResolveAutoSegmentationWritebackDecisionForPlacementFrames(
        const xq_ProfileGroup* group,
        const mitk::PlaneGeometry* referencePlane,
        const std::vector<xq_ProfilePlacementFrame>& placementFrames,
        xq_ProfilePlacementFrame* resolvedPlacementFrame = nullptr);

    static xq_CanonicalWritebackViewDecision GetExtractionFailedWritebackDecision();

    // Returns a preprocessing-target resolution that answers, without any Qt
    // dependency, whether a canonical ProfileGroup target is active and usable
    // for writing new segmentation results, or why it is blocked.
    static xq_PreprocTargetResolution ResolvePreprocessingTarget(
        const xq_ProfileGroup* group,
        unsigned int timeStep = 0);

    static void OrientPresetProfileToPlacement(
        xq_LumenProfile* profile,
        double primarySize,
        double secondarySize = 0.0);

    static xq_ProfileGroupReadinessReport BuildReadinessReport(
        const xq_ProfileGroup* group,
        unsigned int timeStep = 0);

    static xq_ProfileStatistics ComputeProfileStatistics(
        xq_LumenProfile* profile);

    static std::unique_ptr<xq_LumenProfile> SmoothProfile(
        xq_LumenProfile* profile,
        int iterations,
        double relaxation);

    static std::unique_ptr<xq_LumenProfile> ResampleProfile(
        xq_LumenProfile* profile,
        int targetPointCount);
};
