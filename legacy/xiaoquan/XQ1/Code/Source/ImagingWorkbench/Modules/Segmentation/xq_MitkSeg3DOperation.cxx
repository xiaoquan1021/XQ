#include "xq_MitkSeg3DOperation.h"

xq_MitkSeg3DOperation::xq_MitkSeg3DOperation(mitk::OperationType opType,
                                               vtkSmartPointer<vtkPolyData> polyData,
                                               const mitk::Point3D& seedPoint,
                                               int methodId)
    : mitk::Operation(opType)
    , m_PolyData(polyData)
    , m_SeedPoint(seedPoint)
    , m_MethodId(methodId)
{
}

xq_MitkSeg3DOperation::xq_MitkSeg3DOperation(mitk::OperationType opType,
                                               vtkSmartPointer<vtkPolyData> polyData)
    : xq_MitkSeg3DOperation(opType, polyData, mitk::Point3D{}, -1)
{
}

xq_MitkSeg3DOperation::xq_MitkSeg3DOperation(mitk::OperationType opType,
                                               const mitk::Point3D& seedPoint)
    : xq_MitkSeg3DOperation(opType, nullptr, seedPoint, -1)
{
}

xq_MitkSeg3DOperation::xq_MitkSeg3DOperation(mitk::OperationType opType, int methodId)
    : xq_MitkSeg3DOperation(opType, nullptr, mitk::Point3D{}, methodId)
{
}

xq_MitkSeg3DOperation::xq_MitkSeg3DOperation(mitk::OperationType opType)
    : xq_MitkSeg3DOperation(opType, nullptr, mitk::Point3D{}, -1)
{
}

vtkSmartPointer<vtkPolyData> xq_MitkSeg3DOperation::GetPolyData() const
{
    return m_PolyData;
}

mitk::Point3D xq_MitkSeg3DOperation::GetSeedPoint() const
{
    return m_SeedPoint;
}

int xq_MitkSeg3DOperation::GetMethodId() const
{
    return m_MethodId;
}
