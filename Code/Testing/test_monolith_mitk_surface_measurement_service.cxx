#include "Infrastructure/xq_MitkSurfaceMeasurementService.h"

#include <mitkDataNode.h>
#include <mitkSurface.h>

#include <vtkCubeSource.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

bool NearlyEqual(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

mitk::DataNode::Pointer MakeCuboidSurfaceNode()
{
    auto cube = vtkSmartPointer<vtkCubeSource>::New();
    cube->SetXLength(2.0);
    cube->SetYLength(3.0);
    cube->SetZLength(4.0);
    cube->Update();

    auto surface = mitk::Surface::New();
    surface->SetVtkPolyData(cube->GetOutput());

    auto node = mitk::DataNode::New();
    node->SetName("Cuboid Surface");
    node->SetData(surface);
    return node;
}

} // namespace

int main(int, char**)
{
    xq::infrastructure::MitkSurfaceMeasurementService service;

    const auto missing = service.MeasureSurface(
        nullptr,
        xq::core::SurfaceMeasurementKind::Area);
    if (Expect(!missing.Succeeded &&
                   missing.Message ==
                       QStringLiteral("Select a surface node before measuring."),
               "missing node should fail deterministically"))
        return 1;

    auto emptyNode = mitk::DataNode::New();
    emptyNode->SetName("Empty Node");
    const auto nonSurface = service.MeasureSurface(
        emptyNode,
        xq::core::SurfaceMeasurementKind::Area);
    if (Expect(!nonSurface.Succeeded &&
                   nonSurface.Message ==
                       QStringLiteral("Selected node is not a surface."),
               "non-surface node should fail deterministically"))
        return 1;

    const auto surfaceNode = MakeCuboidSurfaceNode();
    const auto area = service.MeasureSurface(
        surfaceNode,
        xq::core::SurfaceMeasurementKind::Area);
    if (Expect(area.Succeeded,
               "surface area measurement should succeed"))
        return 1;
    if (Expect(NearlyEqual(area.SurfaceArea, 52.0, 1.0e-3),
               "surface area should match a 2x3x4 cuboid"))
        return 1;
    if (Expect(area.Message.contains(QStringLiteral("Surface Area: 52.00 mm^2")),
               "surface area diagnostic should report the measured area"))
        return 1;

    const auto volume = service.MeasureSurface(
        surfaceNode,
        xq::core::SurfaceMeasurementKind::Volume);
    if (Expect(volume.Succeeded,
               "surface volume measurement should succeed"))
        return 1;
    if (Expect(NearlyEqual(volume.SurfaceArea, 52.0, 1.0e-3) &&
                   NearlyEqual(volume.Volume, 24.0, 1.0e-3),
               "surface volume should match a 2x3x4 cuboid"))
        return 1;
    if (Expect(volume.Message.contains(QStringLiteral("Volume: 24.00 mm^3")),
               "volume diagnostic should report the measured volume"))
        return 1;

    return 0;
}
