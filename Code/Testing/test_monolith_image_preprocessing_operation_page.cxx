#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

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

xq::core::DataImportRequest MakeImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

QComboBox* FindOperationSelector(xq::presentation::MainWindow& window)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqImagePreprocessingOperationSelector"));
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_image-preprocessing"));
}

QWidget* FindParameterPanel(xq::presentation::MainWindow& window)
{
    return window.findChild<QWidget*>(
        QStringLiteral("xqImagePreprocessingParameterPanel"));
}

QDoubleSpinBox* FindNumericParameter(xq::presentation::MainWindow& window,
                                     const QString& parameterId)
{
    return window.findChild<QDoubleSpinBox*>(
        QStringLiteral("xqImagePreprocessingParameter_%1").arg(parameterId));
}

QSpinBox* FindIntegerParameter(xq::presentation::MainWindow& window,
                               const QString& parameterId)
{
    return window.findChild<QSpinBox*>(
        QStringLiteral("xqImagePreprocessingParameter_%1").arg(parameterId));
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions(),
        context->WorkflowOperations());
    xq::presentation::MainWindow window(*context);

    auto* operationSelector = FindOperationSelector(window);
    if (Expect(operationSelector != nullptr,
               "image preprocessing page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    auto* actionButton = FindActionButton(window);
    if (Expect(actionButton != nullptr,
               "image preprocessing page should expose a primary action"))
    {
        delete context;
        return 1;
    }
    auto* parameterPanel = FindParameterPanel(window);
    if (Expect(parameterPanel != nullptr,
               "image preprocessing page should expose a parameter panel"))
    {
        delete context;
        return 1;
    }

    if (Expect(operationSelector->count() == 6,
               "image preprocessing operation selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(operationSelector->itemData(0).toString() ==
                   QStringLiteral("binary-threshold") &&
                   operationSelector->itemText(0) ==
                       QStringLiteral("Binary Threshold"),
               "image preprocessing operation selector should preserve operation ids and titles"))
    {
        delete context;
        return 1;
    }
    if (Expect(operationSelector->currentData().toString() ==
                   QStringLiteral("binary-threshold"),
               "image preprocessing operation selector should default to first operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(actionButton->text() == QStringLiteral("Run Binary Threshold"),
               "image preprocessing action text should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window, QStringLiteral("lower")) != nullptr &&
                   FindNumericParameter(window, QStringLiteral("upper")) != nullptr &&
                   FindNumericParameter(window, QStringLiteral("inside-value")) != nullptr &&
                   FindNumericParameter(window, QStringLiteral("outside-value")) != nullptr,
               "binary threshold should expose numeric parameter controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window, QStringLiteral("sigma")) == nullptr,
               "inactive operation parameters should not be visible"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("binary-threshold"),
               "operation selector default should synchronize to core selection"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImport(), &errorMessage);
    if (Expect(importResult.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(actionButton->isEnabled(),
               "compatible image should enable operation action"))
    {
        delete context;
        return 1;
    }

    operationSelector->setCurrentIndex(
        operationSelector->findData(QStringLiteral("gaussian-smoothing")));
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("gaussian-smoothing"),
               "changing operation selector should update core selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(actionButton->text() == QStringLiteral("Run Gaussian Smoothing"),
               "changing operation selector should update action text"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window, QStringLiteral("sigma")) != nullptr,
               "Gaussian smoothing should expose sigma parameter"))
    {
        delete context;
        return 1;
    }
    auto* sigmaEditor = FindNumericParameter(window, QStringLiteral("sigma"));
    sigmaEditor->setValue(1.25);
    app.processEvents();
    if (Expect(context->WorkflowOperations()
                   ->ParameterValues(QStringLiteral("image-preprocessing"),
                                     QStringLiteral("gaussian-smoothing"))
                   .value(QStringLiteral("sigma"))
                   .toDouble() == 1.25,
               "editing sigma should update Core operation parameter state"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window, QStringLiteral("lower")) == nullptr,
               "switching operations should remove previous parameter controls"))
    {
        delete context;
        return 1;
    }

    operationSelector->setCurrentIndex(
        operationSelector->findData(QStringLiteral("crop")));
    app.processEvents();
    if (Expect(FindIntegerParameter(window, QStringLiteral("origin-x")) != nullptr &&
                   FindIntegerParameter(window, QStringLiteral("size-z")) != nullptr,
               "crop should expose integer parameter controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window, QStringLiteral("sigma")) == nullptr,
               "switching to crop should remove Gaussian parameter controls"))
    {
        delete context;
        return 1;
    }

    operationSelector->setCurrentIndex(
        operationSelector->findData(QStringLiteral("gaussian-smoothing")));
    app.processEvents();

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });
    actionButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Image Preprocessing succeeded: Gaussian Smoothing preprocessing operation accepted CTA Image.")),
               "operation action should run the selected image preprocessing operation"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
