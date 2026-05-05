// XQ ThresholdInteractor: pixel-value-gated contour interaction with range validation
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkDataInteractor.h>
#include <mitkImage.h>

class XQMODULESEGMENTATION_EXPORT xq_ThresholdInteractor : public mitk::DataInteractor
{
public:
    mitkClassMacro(xq_ThresholdInteractor, mitk::DataInteractor)
    itkFactorylessNewMacro(Self)

    void SetThresholdRange(double lower, double upper);
    double GetLowerThreshold() const;
    double GetUpperThreshold() const;
    void SetWorkingImage(mitk::Image::Pointer image);

protected:
    xq_ThresholdInteractor();
    ~xq_ThresholdInteractor() override = default;
    void ConnectActionsAndFunctions() override;

    void OnAddPoint(mitk::StateMachineAction* action, mitk::InteractionEvent* event);
    void OnFinishContour(mitk::StateMachineAction* action, mitk::InteractionEvent* event);

    double m_LowerThreshold = 0.0;
    double m_UpperThreshold = 1000.0;
    mitk::Image::Pointer m_WorkingImage = nullptr;
};
