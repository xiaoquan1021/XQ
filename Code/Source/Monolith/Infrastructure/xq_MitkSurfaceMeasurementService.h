#ifndef XQ_INFRASTRUCTURE_MITKSURFACEMEASUREMENTSERVICE_H
#define XQ_INFRASTRUCTURE_MITKSURFACEMEASUREMENTSERVICE_H

#include "Core/xq_MeasurementService.h"

namespace xq::infrastructure
{

class MitkSurfaceMeasurementService : public xq::core::MeasurementService
{
public:
    xq::core::MeasurementResult MeasureSurface(
        mitk::DataNode::Pointer node,
        xq::core::SurfaceMeasurementKind kind) override;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITKSURFACEMEASUREMENTSERVICE_H
