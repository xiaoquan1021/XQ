#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_SegmentationWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

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

QComboBox* FindSelector(xq::presentation::MainWindow& window,
                        const QString& workflowId)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqWorkflowOperationSelector_%1").arg(workflowId));
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window,
                              const QString& workflowId)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_%1").arg(workflowId));
}

QDoubleSpinBox* FindNumericParameter(xq::presentation::MainWindow& window,
                                     const QString& parameterId)
{
    return window.findChild<QDoubleSpinBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
}

QSpinBox* FindIntegerParameter(xq::presentation::MainWindow& window,
                               const QString& parameterId)
{
    return window.findChild<QSpinBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
}

xq::core::DataImportRequest MakeImageImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("seg-image");
    request.SourcePath = QStringLiteral("C:/studies/seg-image");
    request.DisplayName = QStringLiteral("Seg CTA");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions(),
        context->WorkflowOperations());
    xq::infrastructure::RegisterDynamicSegmentationWorkflowActionHandler(
        *context,
        nullptr);
    xq::presentation::MainWindow window(*context);

    auto* segmentation2dSelector =
        FindSelector(window, QStringLiteral("segmentation-2d"));
    if (Expect(segmentation2dSelector != nullptr,
               "2D segmentation page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(segmentation2dSelector->count() == 3,
               "2D segmentation selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(segmentation2dSelector->itemData(0).toString() ==
                       QStringLiteral("threshold-contour") &&
                   segmentation2dSelector->itemText(0) ==
                       QStringLiteral("Threshold Contour"),
               "2D segmentation selector should preserve operation order"))
    {
        delete context;
        return 1;
    }
    auto* segmentation2dButton =
        FindActionButton(window, QStringLiteral("segmentation-2d"));
    if (Expect(segmentation2dButton != nullptr &&
                   segmentation2dButton->text() ==
                       QStringLiteral("Run Threshold Contour"),
               "2D segmentation action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("threshold-lower")) !=
                       nullptr &&
                   FindNumericParameter(window,
                                        QStringLiteral("threshold-upper")) !=
                       nullptr,
               "2D threshold contour should expose threshold controls"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("segmentation-2d")) ==
                   QStringLiteral("threshold-contour"),
               "2D segmentation should keep threshold contour selected by default"))
    {
        delete context;
        return 1;
    }
    if (Expect(segmentation2dButton->text() ==
                   QStringLiteral("Run Threshold Contour"),
               "2D segmentation action should keep threshold contour action text"))
    {
        delete context;
        return 1;
    }

    segmentation2dSelector->setCurrentIndex(
        segmentation2dSelector->findData(QStringLiteral("loft-profiles")));
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("segmentation-2d")) ==
                   QStringLiteral("loft-profiles"),
               "2D segmentation selector should update Core state"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindIntegerParameter(window,
                                    QStringLiteral("sample-count")) != nullptr,
               "2D loft profiles should expose sample count control"))
    {
        delete context;
        return 1;
    }

    if (Expect(segmentation2dButton->text() ==
                   QStringLiteral("Run Loft Profiles"),
               "2D segmentation action should update to loft profiles"))
    {
        delete context;
        return 1;
    }
    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(), &errorMessage);
    if (Expect(importResult.Succeeded,
               "segmentation image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("segmentation-2d")),
               "2D segmentation workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(segmentation2dButton->isEnabled(),
               "compatible image should enable 2D segmentation action"))
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
    segmentation2dButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run 2D Segmentation failed: Active contour/profile segmentation node is required for loft profiles.")),
               "2D segmentation action should report native loft validation"))
    {
        delete context;
        return 1;
    }

    auto* segmentation3dSelector =
        FindSelector(window, QStringLiteral("segmentation-3d"));
    if (Expect(segmentation3dSelector != nullptr,
               "3D segmentation page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(segmentation3dSelector->count() == 3,
               "3D segmentation selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(segmentation3dSelector->itemData(0).toString() ==
                       QStringLiteral("threshold-region") &&
                   segmentation3dSelector->itemText(0) ==
                       QStringLiteral("Threshold Region"),
               "3D segmentation selector should preserve operation order"))
    {
        delete context;
        return 1;
    }

    segmentation3dSelector->setCurrentIndex(
        segmentation3dSelector->findData(QStringLiteral("threshold-region")));
    app.processEvents();
    auto* segmentation3dButton =
        FindActionButton(window, QStringLiteral("segmentation-3d"));
    if (Expect(segmentation3dButton != nullptr &&
                   segmentation3dButton->text() ==
                       QStringLiteral("Run Threshold Region"),
               "3D segmentation action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("threshold-lower")) !=
                       nullptr &&
                   FindNumericParameter(window,
                                        QStringLiteral("threshold-upper")) !=
                       nullptr,
               "3D threshold region should expose threshold controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("segmentation-3d")),
               "3D segmentation workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(segmentation3dButton->isEnabled(),
               "compatible image should enable 3D segmentation action"))
    {
        delete context;
        return 1;
    }
    segmentation3dButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run 3D Segmentation failed: Active image node is required for 3D segmentation.")),
               "3D segmentation action should report infrastructure validation"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
