#include "xq_ProfileGroupInteractor.h"
#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"
#include "xq_PolygonalProfile.h"

#include <mitkInteractionPositionEvent.h>
#include <mitkBaseRenderer.h>
#include <mitkRenderingManager.h>

#include <cmath>

xq_ProfileGroupInteractor::xq_ProfileGroupInteractor()
{
}

void xq_ProfileGroupInteractor::SetCurrentPathPosIndex(int index)
{
    m_CurrentPathPosIndex = index;
}

int xq_ProfileGroupInteractor::GetCurrentPathPosIndex() const
{
    return m_CurrentPathPosIndex;
}

xq_ProfileGroup* xq_ProfileGroupInteractor::GetProfileCollection() const
{
    if (!GetDataNode())
        return nullptr;

    return dynamic_cast<xq_ProfileGroup*>(GetDataNode()->GetData());
}

int xq_ProfileGroupInteractor::FindNearestAnchorPoint(
    xq_LumenProfile* contour, const mitk::Point3D& worldPt, double maxDist) const
{
    if (!contour)
        return -1;

    int closestIdx = -1;
    auto closestDist = maxDist;

    for (int i = 0; i < contour->GetAnchorPointCount(); ++i)
    {
        auto cp = contour->GetAnchorPoint(i);
        double dx = cp[0] - worldPt[0];
        double dy = cp[1] - worldPt[1];
        double dz = cp[2] - worldPt[2];
        auto d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d < closestDist)
        {
            closestDist = d;
            closestIdx = i;
        }
    }
    return closestIdx;
}

void xq_ProfileGroupInteractor::ConnectActionsAndFunctions()
{
    CONNECT_FUNCTION("addContourPoint",     OnAddContourPoint)
    CONNECT_FUNCTION("selectContourPoint",  OnSelectContourPoint)
    CONNECT_FUNCTION("moveContourPoint",    OnMoveContourPoint)
    CONNECT_FUNCTION("releaseContourPoint", OnReleaseContourPoint)
    CONNECT_FUNCTION("removeContourPoint",  OnRemoveContourPoint)
    CONNECT_FUNCTION("finishContour",       OnFinishContour)
    CONNECT_FUNCTION("deselectAll",         OnDeselectAll)
    CONNECT_FUNCTION("cancelContour",       OnCancelContour)
}

// ---------------------------------------------------------------------------

void xq_ProfileGroupInteractor::OnAddContourPoint(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* group = GetProfileCollection();
    if (!group)
        return;

    auto worldPt = posEvent->GetPositionInWorld();

    auto* contour = group->FetchProfile(m_CurrentPathPosIndex);
    if (!contour)
    {
        // ProfileGroup takes ownership via unique_ptr
        auto* polygon = new xq_PolygonalProfile();

        auto* renderer = posEvent->GetSender();
        if (renderer && renderer->GetCurrentWorldPlaneGeometry())
        {
            polygon->SetSlicePlane(
                renderer->GetCurrentWorldPlaneGeometry()->Clone());
        }

        group->AppendProfile(polygon, m_CurrentPathPosIndex);
        contour = polygon;
    }

    contour->InsertAnchorPoint(contour->GetAnchorPointCount(), worldPt);
    contour->GenerateProfilePoints();
    m_ActiveProfile = contour;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnSelectContourPoint(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* group = GetProfileCollection();
    if (!group)
        return;

    auto worldPt = posEvent->GetPositionInWorld();
    auto* contour = group->FetchProfile(m_CurrentPathPosIndex);
    if (!contour)
        return;

    m_PickedPointIndex = FindNearestAnchorPoint(contour, worldPt, 4.0);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnMoveContourPoint(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* group = GetProfileCollection();
    if (!group)
        return;

    auto* contour = group->FetchProfile(m_CurrentPathPosIndex);
    if (!contour || m_PickedPointIndex < 0
        || m_PickedPointIndex >= contour->GetAnchorPointCount())
        return;

    auto worldPt = posEvent->GetPositionInWorld();
    contour->SetAnchorPoint(m_PickedPointIndex, worldPt);
    contour->GenerateProfilePoints();

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnReleaseContourPoint(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* /*event*/)
{
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnRemoveContourPoint(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* /*event*/)
{
    auto* group = GetProfileCollection();
    if (!group)
        return;

    auto* contour = group->FetchProfile(m_CurrentPathPosIndex);
    if (!contour)
        return;

    if (m_PickedPointIndex >= 0 &&
        m_PickedPointIndex < contour->GetAnchorPointCount())
    {
        contour->RemoveAnchorPoint(m_PickedPointIndex);
        contour->GenerateProfilePoints();
    }

    m_PickedPointIndex = -1;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnFinishContour(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* /*event*/)
{
    m_ActiveProfile = nullptr;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnDeselectAll(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* /*event*/)
{
    m_PickedPointIndex = -1;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ProfileGroupInteractor::OnCancelContour(
    mitk::StateMachineAction* /*action*/, mitk::InteractionEvent* /*event*/)
{
    auto* group = GetProfileCollection();

    if (m_ActiveProfile && group)
    {
        group->RemoveProfile(m_CurrentPathPosIndex);
    }

    m_ActiveProfile = nullptr;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
