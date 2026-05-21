#include "xq_SegUndoActor.h"
#include <vtkImageData.h>

xq_SegUndoActor* xq_SegUndoActor::GetInstance()
{
    static xq_SegUndoActor instance;
    return &instance;
}

void xq_SegUndoActor::ExecuteOperation(mitk::Operation* op)
{
    if (!op || op->GetOperationType() != OpRESTORE_IMAGE)
        return;
    if (!m_Target)
        return;
    auto* restoreOp = dynamic_cast<RestoreImageOp*>(op);
    if (!restoreOp)
        return;

    auto* vtkImg = m_Target->GetVtkImageData();
    if (vtkImg && restoreOp->GetState())
    {
        vtkImg->DeepCopy(restoreOp->GetState());
        m_Target->Modified();
    }
}
