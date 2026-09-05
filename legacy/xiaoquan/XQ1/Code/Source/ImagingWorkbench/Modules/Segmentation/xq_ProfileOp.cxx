#include "xq_ProfileOp.h"

xq_ProfileOp::xq_ProfileOp(mitk::OperationType operationType,
                            xq_LumenProfile* contour,
                            int pathPosIndex,
                            unsigned int timeStep)
    : mitk::Operation(operationType)
    , m_Profile(contour)
    , m_PathPosIndex(pathPosIndex)
    , m_TemporalIndex(timeStep)
{
}

// Delegating constructor for remove operations (no contour needed)
xq_ProfileOp::xq_ProfileOp(mitk::OperationType operationType,
                            int pathPosIndex,
                            unsigned int timeStep)
    : xq_ProfileOp(operationType, nullptr, pathPosIndex, timeStep)
{
}

xq_LumenProfile* xq_ProfileOp::GetProfile() const
{
    return m_Profile;
}

int xq_ProfileOp::GetPathPosIndex() const
{
    return m_PathPosIndex;
}

unsigned int xq_ProfileOp::GetTemporalIndex() const
{
    return m_TemporalIndex;
}
