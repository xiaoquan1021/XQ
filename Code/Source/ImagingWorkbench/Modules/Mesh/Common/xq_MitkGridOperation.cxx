#include "xq_MitkGridOperation.h"

xq_MitkGridOperation::xq_MitkGridOperation(
    mitk::OperationType opType, unsigned int timeStep, xq_Grid* mesh)
    : mitk::Operation(opType)
    , m_TimeStep(timeStep)
    , m_Mesh(mesh)
{
}

unsigned int xq_MitkGridOperation::GetTimeStep() const
{
    return m_TimeStep;
}

xq_Grid* xq_MitkGridOperation::GetMesh() const
{
    return m_Mesh;
}
