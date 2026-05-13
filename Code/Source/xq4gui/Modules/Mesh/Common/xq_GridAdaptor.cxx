#include "xq_GridAdaptor.h"
#include "xq_Grid.h"

xq_GridAdaptor::xq_GridAdaptor() = default;

void xq_GridAdaptor::SetInputMesh(xq_Grid* mesh)
{
    m_InputMesh = mesh;
}

xq_Grid* xq_GridAdaptor::GetInputMesh() const
{
    return m_InputMesh;
}

void xq_GridAdaptor::SetErrorMetric(std::string_view arrayName, double targetError)
{
    m_ErrorMetricName = std::string(arrayName);
    m_TargetError = targetError;
}

const std::string& xq_GridAdaptor::GetErrorMetricName() const
{
    return m_ErrorMetricName;
}

double xq_GridAdaptor::GetTargetError() const
{
    return m_TargetError;
}

void xq_GridAdaptor::PopulateStats(xq_Grid* inputMesh,
                                   xq_Grid* outputMesh,
                                   xq_AdaptResult& result)
{
    if (inputMesh)
    {
        result.inputCellCount = inputMesh->GetNumberOfElements();
        result.inputNodeCount = inputMesh->GetNumberOfNodes();
    }
    if (outputMesh)
    {
        result.outputCellCount = outputMesh->GetNumberOfElements();
        result.outputNodeCount = outputMesh->GetNumberOfNodes();
    }
}
