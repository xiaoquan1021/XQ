#pragma once

#include <xqMeshCommonExports.h>

#include <string>
#include <string_view>

class xq_Grid;

struct XQMESHCOMMON_EXPORT xq_AdaptResult
{
    bool ok = false;
    std::string diagnostic;
    int iterationCount = 0;
    int inputCellCount = 0;
    int inputNodeCount = 0;
    int outputCellCount = 0;
    int outputNodeCount = 0;
    double peakError = 0.0;
};

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

    static void PopulateStats(xq_Grid* inputMesh,
                              xq_Grid* outputMesh,
                              xq_AdaptResult& result);

protected:
    xq_GridAdaptor();

    xq_Grid* m_InputMesh = nullptr;
    std::string m_ErrorMetricName;
    double m_TargetError = 0.01;
};
