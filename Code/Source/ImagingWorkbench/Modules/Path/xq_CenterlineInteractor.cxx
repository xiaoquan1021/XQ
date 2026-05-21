#include "xq_CenterlineInteractor.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineOp.h"
#include "xq_SpatialMath.h"
#include "xq_UndoHelper.h"

#include <mitkInteractionPositionEvent.h>
#include <mitkInternalEvent.h>
#include <mitkBaseRenderer.h>
#include <mitkRenderingManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>
#include <unordered_map>

xq_CenterlineInteractor::xq_CenterlineInteractor()
{
    m_DragOrigin.Fill(0.0);
}

void xq_CenterlineInteractor::ConnectActionsAndFunctions()
{
    CONNECT_FUNCTION("addPoint",    PlaceAnchor)
    CONNECT_FUNCTION("selectPoint", HighlightAnchor)
    CONNECT_FUNCTION("movePoint",   RelocateAnchor)
    CONNECT_FUNCTION("removePoint", EraseAnchor)
    CONNECT_FUNCTION("deselectAll", ClearHighlights)
    CONNECT_FUNCTION("initMove",    BeginDrag)
    CONNECT_FUNCTION("finishMove",  EndDrag)

    CONNECT_CONDITION("checkPointClick", IsNearAnchor)
    CONNECT_CONDITION("checkSelection",  HasHighlightedAnchor)
}

void xq_CenterlineInteractor::DataNodeChanged()
{
    m_DragOrigin.Fill(0.0);

    auto* node = GetDataNode();
    if (!node || !node->GetData())
    {
        return;
    }

    auto* path = dynamic_cast<xq_VesselCenterline*>(node->GetData());
    if (path && !path->GetSegment(0))
    {
        path->Expand(1);
    }
}

xq_VesselCenterline* xq_CenterlineInteractor::GetCenterline() const
{
    if (auto* node = GetDataNode(); node)
        return dynamic_cast<xq_VesselCenterline*>(node->GetData());
    return nullptr;
}

int xq_CenterlineInteractor::FindNearestAnchor(
    const mitk::Point3D& worldPoint, double maxDistance) const
{
    auto* path = GetCenterline();
    if (!path)
        return -1;

    auto* elem = path->GetSegment(0);
    if (!elem)
        return -1;

    const int numPts = elem->GetAnchorCount();
    if (numPts <= 0)
        return -1;

    // Build index sequence and find minimum distance using STL
    int closestIdx = -1;
    double closestDist = maxDistance;

    for (int i = 0; i < numPts; ++i)
    {
        if (const double d = xq_SpatialMath::EuclideanDistance3D(worldPoint, elem->GetAnchorPosition(i));
            d < closestDist)
        {
            closestDist = d;
            closestIdx = i;
        }
    }
    return closestIdx;
}

void xq_CenterlineInteractor::PlaceAnchor(mitk::StateMachineAction* /*action*/,
                                        mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* path = GetCenterline();
    if (!path)
        return;

    auto worldPt = posEvent->GetPositionInWorld();
    auto* elem = path->GetSegment(0);

    // Lookup-based insert index resolution (C++17 pattern)
    using Mode = xq_VesselCenterline;
    const int sel = elem ? elem->GetSelectedAnchorIndex() : -1;
    const int numCtrl = elem ? elem->GetAnchorCount() : 0;

    // Map adding modes to insert indices
    const auto mode = path->GetInsertionStrategy();
    int insertIdx = -1;

    if (mode == Mode::PREPEND)
        insertIdx = 0;
    else if (mode == Mode::APPEND)
        insertIdx = numCtrl;
    else if (mode == Mode::INSERT_PRIOR)
        insertIdx = (sel >= 0) ? sel : 0;
    else if (mode == Mode::INSERT_NEXT)
        insertIdx = (sel >= 0) ? sel + 1 : numCtrl;
    // AUTO_LOCATE or default: insertIdx remains -1

    insertIdx = std::min(insertIdx, static_cast<int>(numCtrl));

    auto* doOp = new xq_CenterlineOp(xq_CenterlineOp::ActInsertAnchor, 0, worldPt, insertIdx);
    auto* undoOp = new xq_CenterlineOp(xq_CenterlineOp::ActRemoveAnchor, 0, worldPt, insertIdx);
    xq_UndoHelper::SubmitUndoableOperation(path, doOp, undoOp, "Insert Path Point");
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_CenterlineInteractor::HighlightAnchor(mitk::StateMachineAction* /*action*/,
                                           mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* path = GetCenterline();
    if (!path)
        return;

    auto worldPt = posEvent->GetPositionInWorld();
    if (auto idx = FindNearestAnchor(worldPt, m_PickTolerance); idx >= 0)
    {
        auto* op = new xq_CenterlineOp(xq_CenterlineOp::ActHighlightAnchor, 0, worldPt, idx);
        path->ExecuteOperation(op);
        delete op;
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
    else
    {
        mitk::Point3D dummy{};
        dummy.Fill(0.0);
        auto* op = new xq_CenterlineOp(xq_CenterlineOp::ActClearHighlights, 0, dummy, -1);
        path->ExecuteOperation(op);
        delete op;
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}

void xq_CenterlineInteractor::RelocateAnchor(mitk::StateMachineAction* /*action*/,
                                         mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* path = GetCenterline();
    if (!path)
        return;

    auto* elem = path->GetSegment(0);
    if (!elem)
        return;

    if (auto selIdx = elem->GetSelectedAnchorIndex(); selIdx >= 0)
    {
        auto worldPt = posEvent->GetPositionInWorld();
        if (std::isnan(worldPt[0]) || std::isnan(worldPt[1]) || std::isnan(worldPt[2]) ||
            std::isinf(worldPt[0]) || std::isinf(worldPt[1]) || std::isinf(worldPt[2]))
            return;
        auto* doOp = new xq_CenterlineOp(xq_CenterlineOp::ActRelocateAnchor, 0, worldPt, selIdx);
        auto* undoOp = new xq_CenterlineOp(xq_CenterlineOp::ActRelocateAnchor, 0, m_DragOrigin, selIdx);
        xq_UndoHelper::SubmitUndoableOperation(path, doOp, undoOp, "Move Path Point");
        m_DragOrigin = worldPt;
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}

void xq_CenterlineInteractor::EraseAnchor(mitk::StateMachineAction* /*action*/,
                                           mitk::InteractionEvent* /*event*/)
{
    auto* path = GetCenterline();
    if (!path)
        return;

    if (auto* elem = path->GetSegment(0);
        elem && elem->GetSelectedAnchorIndex() >= 0)
    {
        auto selIdx = elem->GetSelectedAnchorIndex();
        auto pos = elem->GetAnchorPosition(selIdx);
        auto* doOp = new xq_CenterlineOp(xq_CenterlineOp::ActRemoveAnchor, 0, pos, selIdx);
        auto* undoOp = new xq_CenterlineOp(xq_CenterlineOp::ActInsertAnchor, 0, pos, selIdx);
        xq_UndoHelper::SubmitUndoableOperation(path, doOp, undoOp, "Remove Path Point");
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}

void xq_CenterlineInteractor::ClearHighlights(mitk::StateMachineAction* /*action*/,
                                           mitk::InteractionEvent* /*event*/)
{
    if (auto* path = GetCenterline(); path)
    {
        mitk::Point3D dummy{};
        dummy.Fill(0.0);
        auto* op = new xq_CenterlineOp(xq_CenterlineOp::ActClearHighlights, 0, dummy, -1);
        path->ExecuteOperation(op);
        delete op;
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}

void xq_CenterlineInteractor::BeginDrag(mitk::StateMachineAction* /*action*/,
                                        mitk::InteractionEvent* event)
{
    if (auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event); posEvent)
        m_DragOrigin = posEvent->GetPositionInWorld();
}

void xq_CenterlineInteractor::EndDrag(mitk::StateMachineAction* /*action*/,
                                          mitk::InteractionEvent* /*event*/)
{
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

bool xq_CenterlineInteractor::IsNearAnchor(const mitk::InteractionEvent* event)
{
    const auto* posEvent = dynamic_cast<const mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return false;

    return FindNearestAnchor(posEvent->GetPositionInWorld(), m_PickTolerance) >= 0;
}

bool xq_CenterlineInteractor::HasHighlightedAnchor(const mitk::InteractionEvent* /*event*/)
{
    if (auto* path = GetCenterline(); path)
    {
        if (auto* elem = path->GetSegment(0); elem)
            return elem->GetSelectedAnchorIndex() >= 0;
    }
    return false;
}
