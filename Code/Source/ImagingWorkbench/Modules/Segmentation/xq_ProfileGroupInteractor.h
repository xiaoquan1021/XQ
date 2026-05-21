// XQ ProfileGroupInteractor: event-driven contour editing via MITK state machine
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkDataInteractor.h>
#include <mitkPoint.h>

class xq_ProfileGroup;
class xq_LumenProfile;

class XQMODULESEGMENTATION_EXPORT xq_ProfileGroupInteractor : public mitk::DataInteractor
{
public:
    mitkClassMacro(xq_ProfileGroupInteractor, mitk::DataInteractor)
    itkFactorylessNewMacro(Self)

    void SetCurrentPathPosIndex(int index);
    int GetCurrentPathPosIndex() const;

protected:
    xq_ProfileGroupInteractor();
    ~xq_ProfileGroupInteractor() override = default;
    void ConnectActionsAndFunctions() override;

    void OnAddContourPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnSelectContourPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnMoveContourPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnReleaseContourPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnRemoveContourPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnFinishContour(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnDeselectAll(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnCancelContour(mitk::StateMachineAction* action, mitk::InteractionEvent* event);

    xq_ProfileGroup* GetProfileCollection() const;
    int FindNearestAnchorPoint(xq_LumenProfile* contour, const mitk::Point3D& worldPt, double maxDist) const;

    int m_CurrentPathPosIndex = 0;
    int m_PickedPointIndex = -1;
    xq_LumenProfile* m_ActiveProfile = nullptr;
};
