#include "xq_MitkSeg3DDataInteractor.h"
#include "xq_MitkSeg3D.h"
#include "xq_MitkSeg3DOperation.h"

#include <mitkInteractionPositionEvent.h>
#include <mitkBaseRenderer.h>
#include <mitkRenderingManager.h>

#include <algorithm>
#include <cmath>
#include <numeric>

xq_MitkSeg3DDataInteractor::xq_MitkSeg3DDataInteractor()
{
}

void xq_MitkSeg3DDataInteractor::SetSelectionAccuracy(double accuracy)
{
    m_SelectionAccuracy = accuracy;
}

double xq_MitkSeg3DDataInteractor::GetSelectionAccuracy() const
{
    return m_SelectionAccuracy;
}

xq_MitkSeg3D* xq_MitkSeg3DDataInteractor::GetSeg3D() const
{
    auto* node = this->GetDataNode();
    if (!node)
        return nullptr;

    return dynamic_cast<xq_MitkSeg3D*>(node->GetData());
}

int xq_MitkSeg3DDataInteractor::FindClosestSeedPoint(const mitk::Point3D& worldPt,
                                                       double maxDist) const
{
    auto* seg3d = GetSeg3D();
    if (!seg3d)
        return -1;

    auto seedPoints = seg3d->GetSeedPoints();
    if (seedPoints.empty())
        return -1;

    auto distanceTo = [&worldPt](const mitk::Point3D& pt) {
        auto dx = pt[0] - worldPt[0];
        auto dy = pt[1] - worldPt[1];
        auto dz = pt[2] - worldPt[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    auto closest = std::min_element(seedPoints.cbegin(), seedPoints.cend(),
        [&distanceTo](const auto& a, const auto& b) {
            return distanceTo(a) < distanceTo(b);
        });

    if (closest != seedPoints.cend() && distanceTo(*closest) < maxDist) {
        return static_cast<int>(std::distance(seedPoints.cbegin(), closest));
    }

    return -1;
}

void xq_MitkSeg3DDataInteractor::DataNodeChanged()
{
    m_SelectedSeedIndex = -1;
    m_IsSelected = false;
}

void xq_MitkSeg3DDataInteractor::ConnectActionsAndFunctions()
{
    CONNECT_FUNCTION("addSeedPoint", OnAddSeedPoint)
    CONNECT_FUNCTION("selectObject", OnSelectObject)
    CONNECT_FUNCTION("deselectObject", OnDeselectObject)
    CONNECT_FUNCTION("removeSeedPoint", OnRemoveSeedPoint)
}

void xq_MitkSeg3DDataInteractor::OnAddSeedPoint(mitk::StateMachineAction* /*action*/,
                                                  mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto worldPos = posEvent->GetPositionInWorld();

    auto* seg3d = GetSeg3D();
    if (!seg3d)
        return;

    xq_MitkSeg3DOperation op(OpADDSEEDPOINT, worldPos);
    seg3d->ExecuteOperation(&op);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSeg3DDataInteractor::OnSelectObject(mitk::StateMachineAction* /*action*/,
                                                  mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto worldPos = posEvent->GetPositionInWorld();

    auto index = FindClosestSeedPoint(worldPos, m_SelectionAccuracy);
    if (index >= 0)
    {
        m_SelectedSeedIndex = index;
        m_IsSelected = true;

        if (auto* node = this->GetDataNode())
            node->SetBoolProperty("selected", true);
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSeg3DDataInteractor::OnDeselectObject(mitk::StateMachineAction* /*action*/,
                                                    mitk::InteractionEvent* /*event*/)
{
    m_SelectedSeedIndex = -1;
    m_IsSelected = false;

    if (auto* node = this->GetDataNode())
        node->SetBoolProperty("selected", false);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSeg3DDataInteractor::OnRemoveSeedPoint(mitk::StateMachineAction* /*action*/,
                                                     mitk::InteractionEvent* /*event*/)
{
    if (m_SelectedSeedIndex >= 0)
    {
        auto* seg3d = GetSeg3D();
        if (seg3d)
        {
            auto seedPoints = seg3d->GetSeedPoints();

            xq_MitkSeg3DOperation clearOp(OpCLEARSEEDPOINTS);
            seg3d->ExecuteOperation(&clearOp);

            for (int i = 0; i < static_cast<int>(seedPoints.size()); ++i)
            {
                if (i == m_SelectedSeedIndex)
                    continue;

                xq_MitkSeg3DOperation addOp(OpADDSEEDPOINT, seedPoints[i]);
                seg3d->ExecuteOperation(&addOp);
            }

            m_SelectedSeedIndex = -1;
        }
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
