#include "xq_GeometryOp.h"

xq_GeometryOp::xq_GeometryOp(mitk::OperationType opType,
                              unsigned int timeStep,
                              xq_VascularGeometry* element,
                              int faceId)
    : mitk::Operation(opType)
    , m_TimeStep(timeStep)
    , m_ModelElement(element)
    , m_FaceId(faceId)
{
}

unsigned int xq_GeometryOp::GetTimeStep() const
{
    return m_TimeStep;
}

xq_VascularGeometry* xq_GeometryOp::GetModelElement() const
{
    return m_ModelElement;
}

int xq_GeometryOp::GetFaceId() const
{
    return m_FaceId;
}
