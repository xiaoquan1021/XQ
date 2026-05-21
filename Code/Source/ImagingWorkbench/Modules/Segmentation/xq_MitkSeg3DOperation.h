// XQ MitkSeg3DOperation: delegating-constructor pattern for multi-payload operations
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkOperation.h>
#include <mitkPoint.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

enum xq_MitkSeg3DOperationType
{
    OpSETSEG3DRESULT  = 300,
    OpADDSEEDPOINT    = 301,
    OpCLEARSEEDPOINTS = 302,
    OpSETMETHOD       = 303
};

class XQMODULESEGMENTATION_EXPORT xq_MitkSeg3DOperation : public mitk::Operation
{
public:
    xq_MitkSeg3DOperation(mitk::OperationType opType,
                           vtkSmartPointer<vtkPolyData> polyData,
                           const mitk::Point3D& seedPoint,
                           int methodId);

    xq_MitkSeg3DOperation(mitk::OperationType opType,
                           vtkSmartPointer<vtkPolyData> polyData);

    xq_MitkSeg3DOperation(mitk::OperationType opType,
                           const mitk::Point3D& seedPoint);

    xq_MitkSeg3DOperation(mitk::OperationType opType, int methodId);

    xq_MitkSeg3DOperation(mitk::OperationType opType);

    ~xq_MitkSeg3DOperation() override = default;

    vtkSmartPointer<vtkPolyData> GetPolyData() const;
    mitk::Point3D GetSeedPoint() const;
    int GetMethodId() const;

private:
    vtkSmartPointer<vtkPolyData> m_PolyData = nullptr;
    mitk::Point3D m_SeedPoint;
    int m_MethodId = -1;
};
