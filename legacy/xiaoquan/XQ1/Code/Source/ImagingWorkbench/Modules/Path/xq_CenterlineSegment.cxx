// XQ design: STL-algorithm-driven implementation.
// This implementation uses std::transform, std::find_if, std::for_each,
// and std::accumulate to express intent declaratively.

#include "xq_CenterlineSegment.h"
#include "xq_VtkParametricSpline.h"
#include "xq_SpatialMath.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>

// ---- Helper: reassign sequential IDs after insert/remove ----
static void ReindexAnchors(std::vector<xq_CenterlineSegment::AnchorNode>& pts)
{
    int idx = 0;
    std::for_each(pts.begin(), pts.end(),
                  [&idx](auto& cp) { cp.id = idx++; });
}

// ---- Construction / copy ----

xq_CenterlineSegment::xq_CenterlineSegment() = default;

xq_CenterlineSegment::xq_CenterlineSegment(const xq_CenterlineSegment& other)
    : m_Anchors(other.m_Anchors)
    , m_Vertices(other.m_Vertices)
    , m_InterpMode(other.m_InterpMode)
    , m_SampleDensity(other.m_SampleDensity)
    , m_StepSize(other.m_StepSize)
{
}

xq_CenterlineSegment& xq_CenterlineSegment::operator=(const xq_CenterlineSegment& other)
{
    if (this != &other)
    {
        m_Anchors     = other.m_Anchors;
        m_Vertices        = other.m_Vertices;
        m_InterpMode            = other.m_InterpMode;
        m_SampleDensity = other.m_SampleDensity;
        m_StepSize           = other.m_StepSize;
    }
    return *this;
}

xq_CenterlineSegment* xq_CenterlineSegment::Duplicate()
{
    return new xq_CenterlineSegment(*this);
}

// ---- Control-point mutation ----

void xq_CenterlineSegment::InsertAnchor(int index, const mitk::Point3D& point)
{
    const auto sz = static_cast<int>(m_Anchors.size());
    index = std::clamp(index, 0, sz);

    m_Anchors.insert(m_Anchors.begin() + index,
                           AnchorNode{index, false, point});

    ReindexAnchors(m_Anchors);
    Interpolate();
}

void xq_CenterlineSegment::RemoveAnchor(int index)
{
    if (index < 0 || index >= static_cast<int>(m_Anchors.size()))
        throw std::out_of_range("xq_CenterlineSegment::RemoveAnchor – index out of range");

    m_Anchors.erase(m_Anchors.begin() + index);
    ReindexAnchors(m_Anchors);
    Interpolate();
}

void xq_CenterlineSegment::UpdateAnchor(int index, const mitk::Point3D& point)
{
    if (index < 0 || index >= static_cast<int>(m_Anchors.size()))
        throw std::out_of_range("xq_CenterlineSegment::UpdateAnchor – index out of range");

    m_Anchors[index].point = point;
    Interpolate();
}

void xq_CenterlineSegment::ReplaceAnchors(const std::vector<mitk::Point3D>& points, bool update)
{
    m_Anchors.clear();
    m_Anchors.resize(points.size());

    // XQ design: std::transform replaces SV-style manual index loop
    int idx = 0;
    std::transform(points.cbegin(), points.cend(), m_Anchors.begin(),
                   [&idx](const mitk::Point3D& pt) -> AnchorNode {
                       return {idx++, false, pt};
                   });

    if (update)
        Interpolate();
}

// ---- Control-point queries ----

int xq_CenterlineSegment::GetAnchorCount() const
{
    return static_cast<int>(m_Anchors.size());
}

mitk::Point3D xq_CenterlineSegment::GetAnchorPosition(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_Anchors.size()))
        throw std::out_of_range("xq_CenterlineSegment::GetAnchorPosition – index out of range");

    return m_Anchors[index].point;
}

std::vector<mitk::Point3D> xq_CenterlineSegment::GetAnchorPositions() const
{
    // XQ design: std::transform instead of push_back loop
    std::vector<mitk::Point3D> pts(m_Anchors.size());
    std::transform(m_Anchors.cbegin(), m_Anchors.cend(), pts.begin(),
                   [](const AnchorNode& cp) { return cp.point; });
    return pts;
}

std::vector<xq_CenterlineSegment::AnchorNode> xq_CenterlineSegment::GetAnchorNodes() const
{
    return m_Anchors;
}

int xq_CenterlineSegment::GetSelectedAnchorIndex()
{
    // XQ design: std::find_if replaces manual index-counting loop
    const auto it = std::find_if(m_Anchors.cbegin(), m_Anchors.cend(),
                                 [](const AnchorNode& cp) { return cp.selected; });
    return (it != m_Anchors.cend())
               ? static_cast<int>(std::distance(m_Anchors.cbegin(), it))
               : -1;
}

void xq_CenterlineSegment::ClearAnchorSelection()
{
    // XQ design: std::for_each with lambda replaces range-for
    std::for_each(m_Anchors.begin(), m_Anchors.end(),
                  [](AnchorNode& cp) { cp.selected = false; });
}

void xq_CenterlineSegment::SetAnchorHighlighted(int index, bool selected)
{
    if (index < 0 || index >= static_cast<int>(m_Anchors.size()))
        return;

    ClearAnchorSelection();
    m_Anchors[index].selected = selected;
}

// ---- Path-point queries ----

std::vector<xq_CenterlineSegment::TraceVertex> xq_CenterlineSegment::GetTraceVertices() const
{
    return m_Vertices;
}

std::vector<mitk::Point3D> xq_CenterlineSegment::GetTracePositions() const
{
    // XQ design: std::transform extracts positions declaratively
    std::vector<mitk::Point3D> pts(m_Vertices.size());
    std::transform(m_Vertices.cbegin(), m_Vertices.cend(), pts.begin(),
                   [](const TraceVertex& pp) { return pp.pos; });
    return pts;
}

int xq_CenterlineSegment::GetTraceVertexCount() const
{
    return static_cast<int>(m_Vertices.size());
}

void xq_CenterlineSegment::SetTraceVertices(const std::vector<TraceVertex>& vertices)
{
    m_Vertices = vertices;
}

// ---- Calculation configuration ----

void xq_CenterlineSegment::SetInterpolationMode(InterpolationMode method)
{
    m_InterpMode = method;
}

xq_CenterlineSegment::InterpolationMode xq_CenterlineSegment::GetInterpolationMode() const
{
    return m_InterpMode;
}

void xq_CenterlineSegment::SetSampleDensity(int number)
{
    if (number > 0)
        m_SampleDensity = number;
}

int xq_CenterlineSegment::GetSampleDensity() const
{
    return m_SampleDensity;
}

void xq_CenterlineSegment::SetStepSize(double spacing)
{
    if (spacing > 0.0)
        m_StepSize = spacing;
}

double xq_CenterlineSegment::GetStepSize() const
{
    return m_StepSize;
}

// ---- Path interpolation ----

void xq_CenterlineSegment::Interpolate()
{
    m_Vertices.clear();

    const auto numCtrl = static_cast<int>(m_Anchors.size());
    if (numCtrl < 2)
    {
        // Degenerate case: single control point → synthetic frame
        if (numCtrl == 1)
        {
            TraceVertex pp;
            pp.id  = 0;
            pp.pos = m_Anchors[0].point;
            pp.tangent.Fill(0.0);
            pp.normal.Fill(0.0);
            pp.rotation.Fill(0.0);
            pp.tangent[2]  = 1.0;   // default Z-forward
            pp.normal[0]   = 1.0;
            pp.rotation[1] = 1.0;
            m_Vertices.push_back(pp);
        }
        return;
    }

    // XQ design: std::transform to extract positions (SV copies in manual loop)
    std::vector<mitk::Point3D> inputPts(numCtrl);
    std::transform(m_Anchors.cbegin(), m_Anchors.cend(), inputPts.begin(),
                   [](const AnchorNode& cp) { return cp.point; });

    // Determine output sample count
    int numOutputPts = m_SampleDensity;
    if (m_InterpMode == InterpolationMode::ARC_SAMPLE)
    {
        // XQ design: std::transform + std::accumulate for arc-length sum
        // (SV uses a manual running-sum loop)
        std::vector<double> segmentLengths(numCtrl - 1);
        std::transform(inputPts.cbegin(), std::prev(inputPts.cend()),
                       std::next(inputPts.cbegin()),
                       segmentLengths.begin(),
                       [](const mitk::Point3D& a, const mitk::Point3D& b) {
                           return xq_SpatialMath::EuclideanDistance3D(a, b);
                       });

        const double totalLen = std::accumulate(segmentLengths.cbegin(),
                                                segmentLengths.cend(), 0.0);
        numOutputPts = std::max(2, static_cast<int>(totalLen / m_StepSize) + 1);
    }

    // Interpolate via VTK parametric spline
    xq_VtkParametricSpline spline;
    spline.SetControlVertices(inputPts);
    spline.SetPeriodicBoundary(false);
    spline.SetResolution(numOutputPts);

    const auto framePoints = spline.GetSplineFramePoints();

    // XQ design: std::transform maps frame data → TraceVertex structs
    m_Vertices.resize(framePoints.size());
    int pathIdx = 0;
    std::transform(framePoints.cbegin(), framePoints.cend(), m_Vertices.begin(),
                   [&pathIdx](const auto& frame) -> TraceVertex {
                       return {pathIdx++, frame.position, frame.tangent,
                               frame.normal, frame.rotation};
                   });
}
