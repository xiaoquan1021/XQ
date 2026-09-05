#pragma once

#include <xqModelCommonExports.h>

#include "xq_VascularGeometry.h"

#include <mitkOperation.h>

enum XqModelOperationType
{
    OpSETMODELELEMENT = 52000,
    OpSETFACESELECTED = 52001
};

class XQMODELCOMMON_EXPORT xq_GeometryOp : public mitk::Operation
{
public:
    xq_GeometryOp(mitk::OperationType opType,
                  unsigned int timeStep,
                  xq_VascularGeometry* element = nullptr,
                  int faceId = -1);

    ~xq_GeometryOp() override = default;

    [[nodiscard]] unsigned int GetTimeStep() const;
    [[nodiscard]] xq_VascularGeometry* GetModelElement() const;
    [[nodiscard]] int GetFaceId() const;

private:
    unsigned int m_TimeStep = 0;
    xq_VascularGeometry* m_ModelElement = nullptr;
    int m_FaceId = -1;
};
