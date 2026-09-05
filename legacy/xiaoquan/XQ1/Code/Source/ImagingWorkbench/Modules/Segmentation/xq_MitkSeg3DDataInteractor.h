// XQ MitkSeg3DDataInteractor: STL-algorithm-based seed point management
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkDataInteractor.h>
#include <mitkPoint.h>

class xq_MitkSeg3D;

class XQMODULESEGMENTATION_EXPORT xq_MitkSeg3DDataInteractor : public mitk::DataInteractor
{
public:
    mitkClassMacro(xq_MitkSeg3DDataInteractor, mitk::DataInteractor)
    itkFactorylessNewMacro(Self)

    void SetSelectionAccuracy(double accuracy);
    double GetSelectionAccuracy() const;

protected:
    xq_MitkSeg3DDataInteractor();
    ~xq_MitkSeg3DDataInteractor() override = default;
    void ConnectActionsAndFunctions() override;
    void DataNodeChanged() override;

    void OnAddSeedPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnSelectObject(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnDeselectObject(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnRemoveSeedPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);

    xq_MitkSeg3D* GetSeg3D() const;
    int FindClosestSeedPoint(const mitk::Point3D& worldPt, double maxDist) const;

    double m_SelectionAccuracy = 4.0;
    int m_SelectedSeedIndex = -1;
    bool m_IsSelected = false;
};
