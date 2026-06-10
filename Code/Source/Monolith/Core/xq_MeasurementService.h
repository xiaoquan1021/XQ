#ifndef XQ_CORE_MEASUREMENTSERVICE_H
#define XQ_CORE_MEASUREMENTSERVICE_H

#include <QString>

#include <mitkDataNode.h>

namespace xq::core
{

enum class SurfaceMeasurementKind
{
    Area,
    Volume
};

struct MeasurementResult
{
    bool Succeeded = false;
    double SurfaceArea = 0.0;
    double Volume = 0.0;
    QString Message;
};

class MeasurementService
{
public:
    virtual ~MeasurementService() = default;

    virtual MeasurementResult MeasureSurface(
        mitk::DataNode::Pointer node,
        SurfaceMeasurementKind kind) = 0;
};

} // namespace xq::core

#endif // XQ_CORE_MEASUREMENTSERVICE_H
