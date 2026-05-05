#pragma once

#include <xqMeshCommonExports.h>

#include <string>
#include <string_view>

class xq_Grid;

class XQMESHCOMMON_EXPORT xq_GridAdaptor
{
public:
    virtual ~xq_GridAdaptor() = default;

    virtual bool Adapt() = 0;

    virtual void SetInputMesh(xq_Grid* mesh);
    [[nodiscard]] xq_Grid* GetInputMesh() const;

    virtual void SetErrorMetric(std::string_view arrayName, double targetError);
    [[nodiscard]] const std::string& GetErrorMetricName() const;
    [[nodiscard]] double GetTargetError() const;

protected:
    xq_GridAdaptor();

    xq_Grid* m_InputMesh = nullptr;
    std::string m_ErrorMetricName;
    double m_TargetError = 0.01;
};
