#include "xq_ModelDataInteractor.h"
#include "xq_Model.h"

#include <mitkInteractionPositionEvent.h>
#include <mitkBaseRenderer.h>
#include <mitkDataNode.h>
#include <mitkProperties.h>

#include <vtkPolyData.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkCellLocator.h>
#include <vtkSmartPointer.h>
#include <vtkGenericCell.h>

namespace
{

struct PickResult
{
    vtkIdType cellId = -1;
    double dist2 = 0.0;
};

[[nodiscard]] PickResult pickClosestCell(vtkPolyData* polyData, const mitk::Point3D& worldPos)
{
    auto cellLocator = vtkSmartPointer<vtkCellLocator>::New();
    cellLocator->SetDataSet(polyData);
    cellLocator->BuildLocator();

    double closestPoint[3];
    int subId = 0;
    PickResult pick;
    double point[3] = {worldPos[0], worldPos[1], worldPos[2]};
    cellLocator->FindClosestPoint(point, closestPoint, pick.cellId, subId, pick.dist2);
    return pick;
}

} // anonymous namespace

xq_ModelDataInteractor::xq_ModelDataInteractor() = default;

void xq_ModelDataInteractor::ConnectActionsAndFunctions()
{
    CONNECT_FUNCTION("selectFace", SelectFace);
    CONNECT_FUNCTION("deselectAll", DeselectAll);
    CONNECT_CONDITION("checkFaceClick", CheckFaceClick);
}

void xq_ModelDataInteractor::DataNodeChanged()
{
    m_SelectedFaceId = -1;
}

void xq_ModelDataInteractor::SelectFace(mitk::StateMachineAction* /*action*/,
                                         mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return;

    auto* node = this->GetDataNode();
    if (!node)
        return;

    auto* model = dynamic_cast<xq_Model*>(node->GetData());
    if (!model)
        return;

    auto* element = model->GetModelElement(0);
    if (!element)
        return;

    auto polyData = element->GetWholeVtkPolyData();
    if (!polyData || polyData->GetNumberOfCells() == 0)
        return;

    auto* faceIds = vtkIntArray::SafeDownCast(
        polyData->GetCellData()->GetArray("FaceIds"));
    if (!faceIds)
        return;

    const auto pick = pickClosestCell(polyData, posEvent->GetPositionInWorld());
    const double threshold = m_SelectionAccuracy * m_SelectionAccuracy;

    if (pick.cellId >= 0 && pick.dist2 < threshold)
    {
        m_SelectedFaceId = faceIds->GetValue(pick.cellId);
        node->SetIntProperty("selectedFaceId", m_SelectedFaceId);
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}

bool xq_ModelDataInteractor::CheckFaceClick(const mitk::InteractionEvent* event)
{
    const auto* posEvent = dynamic_cast<const mitk::InteractionPositionEvent*>(event);
    if (!posEvent)
        return false;

    auto* node = this->GetDataNode();
    if (!node)
        return false;

    auto* model = dynamic_cast<xq_Model*>(node->GetData());
    if (!model)
        return false;

    auto* element = model->GetModelElement(0);
    if (!element)
        return false;

    auto polyData = element->GetWholeVtkPolyData();
    if (!polyData || polyData->GetNumberOfCells() == 0)
        return false;

    if (!vtkIntArray::SafeDownCast(polyData->GetCellData()->GetArray("FaceIds")))
        return false;

    const auto pick = pickClosestCell(polyData, posEvent->GetPositionInWorld());
    const double threshold = m_SelectionAccuracy * m_SelectionAccuracy;
    return pick.cellId >= 0 && pick.dist2 < threshold;
}

void xq_ModelDataInteractor::DeselectAll(mitk::StateMachineAction* /*action*/,
                                          mitk::InteractionEvent* /*event*/)
{
    m_SelectedFaceId = -1;

    if (auto* node = this->GetDataNode())
    {
        node->SetIntProperty("selectedFaceId", -1);
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
}
