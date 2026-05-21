// XQ design: minimal operation class — destructor defaulted in header,
// constructor uses const-ref for Point3D (SV passes by value).

#include "xq_CenterlineOp.h"

xq_CenterlineOp::xq_CenterlineOp(mitk::OperationType opType,
                                   unsigned int timeStep,
                                   const mitk::Point3D& point,
                                   int index)
    : mitk::Operation(opType)
    , m_Position(point)
    , m_AnchorIdx(index)
    , m_FrameIdx(timeStep)
{
}

mitk::Point3D xq_CenterlineOp::GetPosition() const
{
    return m_Position;
}

int xq_CenterlineOp::GetAnchorIndex() const
{
    return m_AnchorIdx;
}

unsigned int xq_CenterlineOp::GetFrameIndex() const
{
    return m_FrameIdx;
}
