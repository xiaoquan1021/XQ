#include "xq_MitkSurfaceMeasurementService.h"

#include <xq_VtkUtils.h>

#include <mitkSurface.h>

namespace xq::infrastructure
{

namespace
{

xq::core::MeasurementResult Failure(const QString& message)
{
    xq::core::MeasurementResult result;
    result.Message = message;
    return result;
}

} // namespace

xq::core::MeasurementResult MitkSurfaceMeasurementService::MeasureSurface(
    mitk::DataNode::Pointer node,
    xq::core::SurfaceMeasurementKind kind)
{
    if (node.IsNull())
        return Failure(QStringLiteral("Select a surface node before measuring."));

    auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
    if (!surface)
        return Failure(QStringLiteral("Selected node is not a surface."));

    auto* polyData = surface->GetVtkPolyData();
    if (!polyData || polyData->GetNumberOfCells() == 0)
        return Failure(QStringLiteral("Selected surface has no polygon data."));

    xq::core::MeasurementResult result;
    result.Succeeded = true;
    result.SurfaceArea = xq_VtkUtils::MeasureSurfaceArea(polyData);
    if (kind == xq::core::SurfaceMeasurementKind::Area)
    {
        result.Message = QStringLiteral("Surface Area: %1 mm^2")
                             .arg(result.SurfaceArea, 0, 'f', 2);
        return result;
    }

    result.Volume = xq_VtkUtils::MeasureEnclosedVolume(polyData);
    const double ratio =
        result.SurfaceArea > 1.0e-12 ? result.Volume / result.SurfaceArea
                                     : 0.0;
    result.Message =
        QStringLiteral("Surface Area: %1 mm^2\nVolume: %2 mm^3\n"
                       "Volume-to-Surface Ratio: %3 mm")
            .arg(result.SurfaceArea, 0, 'f', 2)
            .arg(result.Volume, 0, 'f', 2)
            .arg(ratio, 0, 'f', 2);
    return result;
}

} // namespace xq::infrastructure
