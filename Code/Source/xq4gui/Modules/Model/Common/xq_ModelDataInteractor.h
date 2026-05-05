#pragma once

#include <xqModelCommonExports.h>

#include <mitkDataInteractor.h>

class XQMODELCOMMON_EXPORT xq_ModelDataInteractor : public mitk::DataInteractor
{
public:
    mitkClassMacro(xq_ModelDataInteractor, mitk::DataInteractor)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

protected:
    xq_ModelDataInteractor();
    ~xq_ModelDataInteractor() override = default;

    void ConnectActionsAndFunctions() override;
    void DataNodeChanged() override;

    void SelectFace(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void DeselectAll(mitk::StateMachineAction* action, mitk::InteractionEvent* event);

    bool CheckFaceClick(const mitk::InteractionEvent* event);

private:
    int m_SelectedFaceId = -1;
    double m_SelectionAccuracy = 2.0;
};
