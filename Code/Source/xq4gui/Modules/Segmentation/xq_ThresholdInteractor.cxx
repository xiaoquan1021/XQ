#include "xq_ThresholdInteractor.h"
#include "xq_ThresholdContour.h"

#include <mitkBaseRenderer.h>
#include <mitkInteractionPositionEvent.h>
#include <mitkRenderingManager.h>

#include <vtkImageData.h>

xq_ThresholdInteractor::xq_ThresholdInteractor() = default;

void xq_ThresholdInteractor::ConnectActionsAndFunctions()
{
    CONNECT_FUNCTION("addContourPoint", OnAddPoint)
    CONNECT_FUNCTION("finishContour", OnFinishContour)
}

void xq_ThresholdInteractor::SetThresholdRange(double lower, double upper)
{
    m_LowerThreshold = lower;
    m_UpperThreshold = upper;
}

double xq_ThresholdInteractor::GetLowerThreshold() const
{
    return m_LowerThreshold;
}

double xq_ThresholdInteractor::GetUpperThreshold() const
{
    return m_UpperThreshold;
}

void xq_ThresholdInteractor::SetWorkingImage(mitk::Image::Pointer image)
{
    m_WorkingImage = image;
}

void xq_ThresholdInteractor::OnAddPoint(mitk::StateMachineAction* /*action*/,
                                                     mitk::InteractionEvent* event)
{
    auto* posEvent = dynamic_cast<mitk::InteractionPositionEvent*>(event);
    if (!posEvent || !GetDataNode())
        return;

    auto* contourModel = dynamic_cast<xq_ThresholdContour*>(GetDataNode()->GetData());
    if (!contourModel)
        return;

    const auto worldPoint = posEvent->GetPositionInWorld();
    const auto timeStep = posEvent->GetSender()->GetTimeStep(contourModel);

    // Validate pixel value against threshold range
    if (m_WorkingImage.IsNotNull())
    {
        vtkImageData* vtkImg = m_WorkingImage->GetVtkImageData();
        if (!vtkImg)
            return;

        mitk::Point3D indexPoint;
        m_WorkingImage->GetGeometry()->WorldToIndex(worldPoint, indexPoint);

        int ix = static_cast<int>(indexPoint[0] + 0.5);
        int iy = static_cast<int>(indexPoint[1] + 0.5);
        int iz = static_cast<int>(indexPoint[2] + 0.5);

        int* dims = vtkImg->GetDimensions();
        if (ix < 0 || ix >= dims[0] ||
            iy < 0 || iy >= dims[1] ||
            iz < 0 || iz >= dims[2])
            return;

        double pixelValue = vtkImg->GetScalarComponentAsDouble(ix, iy, iz, 0);

        if (pixelValue < m_LowerThreshold || pixelValue > m_UpperThreshold)
            return;
    }

    contourModel->AddVertex(worldPoint, static_cast<mitk::TimeStepType>(timeStep));
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_ThresholdInteractor::OnFinishContour(mitk::StateMachineAction* /*action*/,
                                                          mitk::InteractionEvent* event)
{
    if (!GetDataNode())
        return;

    auto* contourModel = dynamic_cast<xq_ThresholdContour*>(GetDataNode()->GetData());
    if (!contourModel)
        return;

    const auto timeStep = event->GetSender()->GetTimeStep(contourModel);
    contourModel->Close(timeStep);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
