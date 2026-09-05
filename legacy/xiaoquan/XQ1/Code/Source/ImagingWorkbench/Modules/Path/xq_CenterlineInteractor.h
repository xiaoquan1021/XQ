#pragma once

#include <xqModulePathExports.h>

#include <mitkDataInteractor.h>
#include <mitkPoint.h>

class xq_VesselCenterline;

class XQMODULEPATH_EXPORT xq_CenterlineInteractor : public mitk::DataInteractor
{
public:
    mitkClassMacro(xq_CenterlineInteractor, mitk::DataInteractor)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

protected:
    xq_CenterlineInteractor();
    ~xq_CenterlineInteractor() override = default;

    void ConnectActionsAndFunctions() override;
    void DataNodeChanged() override;

    void PlaceAnchor(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void HighlightAnchor(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void RelocateAnchor(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void EraseAnchor(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void ClearHighlights(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void BeginDrag(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void EndDrag(mitk::StateMachineAction* action, mitk::InteractionEvent* event);

    bool IsNearAnchor(const mitk::InteractionEvent* event);
    bool HasHighlightedAnchor(const mitk::InteractionEvent* event);

private:
    [[nodiscard]] xq_VesselCenterline* GetCenterline() const;

    [[nodiscard]] int FindNearestAnchor(const mitk::Point3D& worldPoint,
                                              double maxDistance) const;

    mitk::Point3D m_DragOrigin{};
    double m_PickTolerance = 4.0;
};
