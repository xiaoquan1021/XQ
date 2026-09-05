#include "xq_VesselCenterline.h"
#include "xq_CenterlineOp.h"
#include "xq_SpatialMath.h"

#include <mitkInteractionConst.h>
#include <mitkLog.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <numeric>
#include <unordered_map>

// Event macro definitions
itkEventMacroDefinition(xq_PathEvent, itk::AnyEvent);
itkEventMacroDefinition(xq_PathPointEvent, xq_PathEvent);
itkEventMacroDefinition(xq_PathPointSelectEvent, xq_PathPointEvent);
itkEventMacroDefinition(xq_PathPointMoveEvent, xq_PathPointEvent);
itkEventMacroDefinition(xq_PathSizeChangeEvent, xq_PathEvent);

// XQ Design: default constructor relies on in-class initializer for m_InsertionStrategy
xq_VesselCenterline::xq_VesselCenterline()
    : mitk::BaseData()
{
    Expand(1);
    // XQ fix: MITK's rendering pipeline culls any BaseData whose
    // TimeGeometry has never been initialized — the VtkPropRenderer
    // computes bounding boxes through TimeGeometry and treats a missing
    // one as "no geometry" → node silently excluded from rendering.
    // mitk::PointSet calls Superclass::InitializeTimeGeometry(1) for the
    // same reason in its ctor; replicate it here so newly constructed
    // centerlines participate in the renderer walk.
    Superclass::InitializeTimeGeometry(1);
}

// XQ Design: deep-copy via std::transform — each segment is independently cloned
xq_VesselCenterline::xq_VesselCenterline(const xq_VesselCenterline& other)
    : mitk::BaseData(other)
    , m_InsertionStrategy(other.m_InsertionStrategy)
    , m_Attributes(other.m_Attributes)
{
    m_Segments.reserve(other.m_Segments.size());
    std::transform(
        other.m_Segments.cbegin(),
        other.m_Segments.cend(),
        std::back_inserter(m_Segments),
        [](const auto& elem) -> std::unique_ptr<xq_CenterlineSegment> {
            return elem
                ? std::unique_ptr<xq_CenterlineSegment>(elem->Duplicate())
                : std::make_unique<xq_CenterlineSegment>();
        });
}

// XQ Design: unique_ptrs handle cleanup automatically
void xq_VesselCenterline::ClearData()
{
    m_Segments.clear();
}

void xq_VesselCenterline::InitializeEmpty()
{
    ClearData();
    Expand(1);
}

// XQ Design: generate_n fills new time steps with default-constructed segments
void xq_VesselCenterline::Expand(unsigned int timeSteps)
{
    if (m_Segments.size() < timeSteps)
    {
        const auto count = timeSteps - m_Segments.size();
        std::generate_n(
            std::back_inserter(m_Segments),
            count,
            []() { return std::make_unique<xq_CenterlineSegment>(); });
    }
}

// XQ Design: dispatch map replaces switch-case for operation routing.
// Each operation is a composable lambda — easy to extend with new ops.
void xq_VesselCenterline::ExecuteOperation(mitk::Operation* operation)
{
    if (!operation)
        return;

    auto* pathOp = dynamic_cast<xq_CenterlineOp*>(operation);
    if (!pathOp)
        return;

    const auto timeStep = pathOp->GetFrameIndex();
    if (timeStep >= m_Segments.size())
        return;

    auto* elem = m_Segments[timeStep].get();
    if (!elem)
        return;

    // XQ Design: dispatch table — maps operation type to handler lambda
    using Handler = std::function<void()>;
    const std::unordered_map<int, Handler> dispatchMap = {
        { xq_CenterlineOp::ActInsertAnchor, [&]() {
            auto idx = pathOp->GetAnchorIndex();
            if (idx < 0)
            {
                // AUTO_LOCATE insertion: find best index based on distance
                idx = xq_SpatialMath::FindNearestSegmentInsertionPoint(
                          elem->GetAnchorPositions(), pathOp->GetPosition());
            }
            elem->InsertAnchor(idx, pathOp->GetPosition());
            elem->ClearAnchorSelection();
            elem->SetAnchorHighlighted(idx, true);
            InvokeEvent(xq_PathSizeChangeEvent());
        }},
        { xq_CenterlineOp::ActRemoveAnchor, [&]() {
            elem->RemoveAnchor(pathOp->GetAnchorIndex());
            InvokeEvent(xq_PathSizeChangeEvent());
        }},
        { xq_CenterlineOp::ActRelocateAnchor, [&]() {
            elem->UpdateAnchor(pathOp->GetAnchorIndex(), pathOp->GetPosition());
            InvokeEvent(xq_PathPointMoveEvent());
        }},
        { xq_CenterlineOp::ActHighlightAnchor, [&]() {
            elem->SetAnchorHighlighted(pathOp->GetAnchorIndex(), true);
            InvokeEvent(xq_PathPointSelectEvent());
        }},
        { xq_CenterlineOp::ActClearHighlights, [&]() {
            elem->ClearAnchorSelection();
            InvokeEvent(xq_PathPointSelectEvent());
        }}
    };

    if (const auto it = dispatchMap.find(pathOp->GetOperationType());
        it != dispatchMap.end())
    {
        it->second();
    }
    else
    {
        MITK_ERROR << "xq_VesselCenterline::ExecuteOperation: unknown operation type "
                   << pathOp->GetOperationType();
    }

    this->Modified();
}

bool xq_VesselCenterline::IsEmptyTimeStep(unsigned int t) const
{
    if (t >= m_Segments.size())
        return true;

    return m_Segments[t]->GetAnchorCount() == 0;
}

// XQ Design: observer access — returns non-owning raw pointer via .get()
xq_CenterlineSegment* xq_VesselCenterline::GetSegment(unsigned int t) const
{
    if (t >= m_Segments.size())
        return nullptr;

    return m_Segments[t].get();
}

// XQ Design: takes ownership of the raw pointer via unique_ptr::reset
void xq_VesselCenterline::SetSegment(xq_CenterlineSegment* pathElement, unsigned int t)
{
    if (t >= m_Segments.size())
        Expand(t + 1);

    m_Segments[t].reset(pathElement);
    this->Modified();
}

xq_VesselCenterline::InsertionStrategy xq_VesselCenterline::GetInsertionStrategy() const
{
    return m_InsertionStrategy;
}

void xq_VesselCenterline::SetInsertionStrategy(InsertionStrategy mode)
{
    m_InsertionStrategy = mode;
}

void xq_VesselCenterline::SetAttribute(const std::string& key, const std::string& value)
{
    m_Attributes[key] = value;
}

std::string xq_VesselCenterline::GetAttribute(const std::string& key) const
{
    const auto it = m_Attributes.find(key);
    return (it != m_Attributes.end()) ? it->second : std::string();
}

std::map<std::string, std::string> xq_VesselCenterline::GetAttributes() const
{
    return m_Attributes;
}

int xq_VesselCenterline::GetNodeCount() const
{
    if (m_Segments.empty())
        return 0;

    return m_Segments[0]->GetAnchorCount();
}

// XQ Design: bounding-box computation uses std::accumulate to fold over all points,
// producing [min,max] per axis in a single pass — no manual index bookkeeping.
void xq_VesselCenterline::UpdateOutputInformation()
{
    if (this->GetSource())
        this->GetSource()->UpdateOutputInformation();

    auto timeGeometry = this->GetTimeGeometry();
    if (timeGeometry == nullptr)
        return;

    // XQ Design: range-based for over unique_ptr vector; index tracked separately
    // for geometry lookup
    unsigned int timeIdx = 0;
    for (const auto& segmentPtr : m_Segments)
    {
        const auto controlPts = segmentPtr->GetAnchorPositions();

        // Only use anchor positions for bounds — trace points may not be computed yet
        const auto& allPts = controlPts;

        if (allPts.empty())
        {
            ++timeIdx;
            continue;
        }

        // XQ Design: accumulate computes axis-aligned bounding box in one pass
        struct BoundsAccum { double xMin, xMax, yMin, yMax, zMin, zMax; };
        const auto& firstPt = allPts.front();
        const auto initBounds = BoundsAccum{
            firstPt[0], firstPt[0],
            firstPt[1], firstPt[1],
            firstPt[2], firstPt[2]
        };

        const auto finalBounds = std::accumulate(
            allPts.cbegin(), allPts.cend(), initBounds,
            [](const BoundsAccum& acc, const mitk::Point3D& pt) {
                return BoundsAccum{
                    std::min(acc.xMin, pt[0]), std::max(acc.xMax, pt[0]),
                    std::min(acc.yMin, pt[1]), std::max(acc.yMax, pt[1]),
                    std::min(acc.zMin, pt[2]), std::max(acc.zMax, pt[2])
                };
            });

        // Pad degenerate axes to avoid zero-thickness bounds
        auto padAxis = [](double lo, double hi) -> std::pair<double, double> {
            return (lo == hi) ? std::make_pair(lo - 0.5, hi + 0.5)
                              : std::make_pair(lo, hi);
        };

        const auto [xLo, xHi] = padAxis(finalBounds.xMin, finalBounds.xMax);
        const auto [yLo, yHi] = padAxis(finalBounds.yMin, finalBounds.yMax);
        const auto [zLo, zHi] = padAxis(finalBounds.zMin, finalBounds.zMax);

        double bounds[6] = { xLo, xHi, yLo, yHi, zLo, zHi };

        auto geometry = timeGeometry->GetGeometryForTimeStep(timeIdx);
        if (geometry.IsNotNull())
        {
            geometry->SetFloatBounds(bounds);
        }

        ++timeIdx;
    }
}

void xq_VesselCenterline::SetRequestedRegionToLargestPossibleRegion()
{
    // Nothing to do – entire path is always available
}

bool xq_VesselCenterline::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_VesselCenterline::VerifyRequestedRegion()
{
    return true;
}

void xq_VesselCenterline::SetRequestedRegion(const itk::DataObject* /*data*/)
{
    // Not applicable for path data
}
