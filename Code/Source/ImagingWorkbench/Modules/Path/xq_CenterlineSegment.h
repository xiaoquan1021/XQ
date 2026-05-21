#pragma once

// XQ design: single-segment centerline with STL-driven internals.
// XQ leverages C++17 value semantics, STL algorithms, and in-class init.

#include <xqModulePathExports.h>

#include <mitkPoint.h>
#include <mitkVector.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>

class XQMODULEPATH_EXPORT xq_CenterlineSegment
{
public:
    struct AnchorNode {
        int id = -1;
        bool selected = false;
        mitk::Point3D point;
    };

    struct TraceVertex {
        int id = -1;
        mitk::Point3D pos;
        mitk::Vector3D tangent;
        mitk::Vector3D normal;
        mitk::Vector3D rotation;
    };

    // XQ design: scoped enum prevents implicit int conversion (SV uses plain enum).
    // Backward-compatible constants are provided below for existing callers.
    enum class InterpolationMode { CUBIC_SPLINE = 0, ARC_SAMPLE };

    // Backward-compatible unscoped aliases so callers using e.g.
    // xq_CenterlineSegment::CUBIC_SPLINE or static_cast<InterpolationMode>(int) still compile.
    static constexpr InterpolationMode CUBIC_SPLINE      = InterpolationMode::CUBIC_SPLINE;
    static constexpr InterpolationMode ARC_SAMPLE  = InterpolationMode::ARC_SAMPLE;

    xq_CenterlineSegment();
    virtual ~xq_CenterlineSegment() = default;
    xq_CenterlineSegment(const xq_CenterlineSegment& other);
    xq_CenterlineSegment& operator=(const xq_CenterlineSegment& other);

    // Returns raw pointer for ABI compat; caller (xq_VesselCenterline) owns the result.
    xq_CenterlineSegment* Duplicate();

    // --- Control point management ---
    void InsertAnchor(int index, const mitk::Point3D& point);
    void RemoveAnchor(int index);
    void UpdateAnchor(int index, const mitk::Point3D& point);
    void ReplaceAnchors(const std::vector<mitk::Point3D>& points, bool update = true);

    [[nodiscard]] int GetAnchorCount() const;
    [[nodiscard]] mitk::Point3D GetAnchorPosition(int index) const;
    [[nodiscard]] std::vector<mitk::Point3D> GetAnchorPositions() const;
    [[nodiscard]] std::vector<AnchorNode> GetAnchorNodes() const;
    [[nodiscard]] int GetSelectedAnchorIndex();
    void ClearAnchorSelection();
    void SetAnchorHighlighted(int index, bool selected);

    // --- Path points (derived from control points via spline interpolation) ---
    [[nodiscard]] std::vector<TraceVertex> GetTraceVertices() const;
    [[nodiscard]] std::vector<mitk::Point3D> GetTracePositions() const;
    [[nodiscard]] int GetTraceVertexCount() const;
    void SetTraceVertices(const std::vector<TraceVertex>& vertices);

    // --- Calculation configuration ---
    void SetInterpolationMode(InterpolationMode method);
    [[nodiscard]] InterpolationMode GetInterpolationMode() const;
    void SetSampleDensity(int number);
    [[nodiscard]] int GetSampleDensity() const;
    void Interpolate();

    void SetStepSize(double spacing);
    [[nodiscard]] double GetStepSize() const;

protected:
    // XQ design: in-class member initialization (SV initialises in constructor body).
    std::vector<AnchorNode> m_Anchors;
    std::vector<TraceVertex>    m_Vertices;
    InterpolationMode         m_InterpMode           = InterpolationMode::CUBIC_SPLINE;
    int                       m_SampleDensity = 100;
    double                    m_StepSize           = 0.5;
};
