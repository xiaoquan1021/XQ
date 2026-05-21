#pragma once

#include <xqMeshCommonExports.h>

#include <mitkOperation.h>

#include <cstdint>

class xq_Grid;

class XQMESHCOMMON_EXPORT xq_MitkGridOperation : public mitk::Operation
{
public:
    static constexpr mitk::OperationType OpSETMESH      = 53000;
    static constexpr mitk::OperationType OpADAPTMESH    = 53001;
    static constexpr mitk::OperationType OpGENERATEMESH = 53002;

    xq_MitkGridOperation(mitk::OperationType opType, unsigned int timeStep, xq_Grid* mesh = nullptr);
    ~xq_MitkGridOperation() override = default;

    [[nodiscard]] unsigned int GetTimeStep() const;
    [[nodiscard]] xq_Grid* GetMesh() const;

private:
    unsigned int m_TimeStep = 0;
    xq_Grid* m_Mesh = nullptr;
};
