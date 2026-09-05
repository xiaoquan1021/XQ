#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkOperationActor.h>
#include <mitkImage.h>
#include <mitkOperation.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

const int OpRESTORE_IMAGE = 60001;

class RestoreImageOp : public mitk::Operation
{
public:
    RestoreImageOp(vtkSmartPointer<vtkImageData> state)
        : mitk::Operation(OpRESTORE_IMAGE), m_State(state) {}
    vtkSmartPointer<vtkImageData> GetState() const { return m_State; }
private:
    vtkSmartPointer<vtkImageData> m_State;
};

class XQMODULESEGMENTATION_EXPORT xq_SegUndoActor : public mitk::OperationActor
{
public:
    static xq_SegUndoActor* GetInstance();
    void SetTargetImage(mitk::Image* img) { m_Target = img; }
    void ExecuteOperation(mitk::Operation* op) override;

private:
    xq_SegUndoActor() = default;
    mitk::Image* m_Target = nullptr;
};
