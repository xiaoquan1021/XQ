#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_MeasurementService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>

#include <mitkDataNode.h>
#include <mitkSurface.h>

#include <vtkCubeSource.h>
#include <vtkSmartPointer.h>

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

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

xq::core::DataImportRequest MakeModelImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("model-surface-001");
    request.SourcePath = QStringLiteral("C:/studies/model.vtp");
    request.DisplayName = QStringLiteral("Aorta Surface");
    request.Modality = QStringLiteral("Surface");
    request.WorkflowRole = xq::core::DataWorkflowRole::Model;
    return request;
}

mitk::DataNode::Pointer MakeSurfaceNode()
{
    auto cube = vtkSmartPointer<vtkCubeSource>::New();
    cube->Update();

    auto surface = mitk::Surface::New();
    surface->SetVtkPolyData(cube->GetOutput());

    auto node = mitk::DataNode::New();
    node->SetName("Aorta Surface");
    node->SetData(surface);
    return node;
}

class FakeMeasurementService : public xq::core::MeasurementService
{
public:
    xq::core::MeasurementResult MeasureSurface(
        mitk::DataNode::Pointer node,
        xq::core::SurfaceMeasurementKind kind) override
    {
        LastNode = node;
        LastKind = kind;
        ++Requests;

        xq::core::MeasurementResult result;
        result.Succeeded = true;
        if (kind == xq::core::SurfaceMeasurementKind::Area)
        {
            result.SurfaceArea = 12.5;
            result.Message = QStringLiteral("Surface Area: 12.50 mm^2");
        }
        else
        {
            result.SurfaceArea = 12.5;
            result.Volume = 8.0;
            result.Message = QStringLiteral("Volume: 8.00 mm^3");
        }
        return result;
    }

    int Requests = 0;
    mitk::DataNode::Pointer LastNode;
    xq::core::SurfaceMeasurementKind LastKind =
        xq::core::SurfaceMeasurementKind::Area;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    FakeMeasurementService service;
    window.SetMeasurementService(&service);
    window.show();
    app.processEvents();

    auto* distanceAction =
        FindAction(window, QStringLiteral("xqMeasureDistanceAction"));
    auto* angleAction =
        FindAction(window, QStringLiteral("xqMeasureAngleAction"));
    auto* areaAction =
        FindAction(window, QStringLiteral("xqMeasureAreaAction"));
    auto* volumeAction =
        FindAction(window, QStringLiteral("xqMeasureVolumeAction"));
    if (Expect(distanceAction && angleAction && areaAction && volumeAction,
               "measurement actions should exist"))
    {
        delete context;
        return 1;
    }
    if (Expect(!distanceAction->isEnabled() && !angleAction->isEnabled(),
               "interactive distance and angle measurement should stay disabled in v1"))
    {
        delete context;
        return 1;
    }
    if (Expect(!areaAction->isEnabled() && !volumeAction->isEnabled(),
               "surface measurements should be disabled without selected surface data"))
    {
        delete context;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });

    distanceAction->trigger();
    angleAction->trigger();
    app.processEvents();
    if (Expect(diagnostics.isEmpty(),
               "disabled distance and angle actions should be quiet"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeModelImport(), &errorMessage);
    if (Expect(importResult.Succeeded,
               "model metadata import should succeed"))
    {
        delete context;
        return 1;
    }

    auto surfaceNode = MakeSurfaceNode();
    if (Expect(context->DataNodes()->BindNode(importResult.EntryId,
                                              surfaceNode,
                                              &errorMessage),
               "test should bind surface data to the imported model"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectCatalogEntry(importResult.EntryId,
                                                            &errorMessage),
               "test should select imported model data"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(areaAction->isEnabled() && volumeAction->isEnabled(),
               "surface measurements should enable for selected surface data"))
    {
        delete context;
        return 1;
    }

    areaAction->trigger();
    app.processEvents();
    if (Expect(service.Requests == 1 &&
                   service.LastNode == surfaceNode &&
                   service.LastKind == xq::core::SurfaceMeasurementKind::Area,
               "surface area action should call the measurement service"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.contains(QStringLiteral("Surface Area: 12.50 mm^2")),
               "surface area action should post the measurement result"))
    {
        delete context;
        return 1;
    }

    volumeAction->trigger();
    app.processEvents();
    if (Expect(service.Requests == 2 &&
                   service.LastNode == surfaceNode &&
                   service.LastKind ==
                       xq::core::SurfaceMeasurementKind::Volume,
               "surface volume action should call the measurement service"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.contains(QStringLiteral("Volume: 8.00 mm^3")),
               "surface volume action should post the measurement result"))
    {
        delete context;
        return 1;
    }
    if (Expect(!diagnostics.contains(QStringLiteral(
                   "Measurement tools are not available in Windows monolith v1.")),
               "migrated surface measurement actions should not post unavailable diagnostics"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
