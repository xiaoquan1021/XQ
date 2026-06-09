#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>
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

QGroupBox* FindWorkbenchGroup(xq::presentation::MainWindow& window,
                              const QString& objectName)
{
    return window.findChild<QGroupBox*>(objectName);
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

QLineEdit* FindPointListParameter(xq::presentation::MainWindow& window,
                                  const QString& parameterId)
{
    return window.findChild<QLineEdit*>(
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
    xq::infrastructure::RegisterDynamicImagePreprocessingWorkflowActionHandler(
        *context);
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
    if (Expect(window.findChild<QLabel*>(QStringLiteral(
                   "xqImagePreprocessingContextLabel")) != nullptr,
               "image preprocessing page should restore original context status label"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingInputGroup")) !=
                   nullptr,
               "image preprocessing page should restore original Input group"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.findChild<QComboBox*>(QStringLiteral(
                   "xqImagePreprocessingImageComboBox")) != nullptr,
               "image preprocessing page should restore original image selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.findChild<QPushButton*>(QStringLiteral(
                   "xqImagePreprocessingRefreshButton")) != nullptr,
               "image preprocessing page should restore original image refresh button"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingOperationGroup")) !=
                   nullptr,
               "image preprocessing page should restore original Operation group"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingThresholdGroup")) !=
                   nullptr,
               "image preprocessing page should restore Threshold Parameters group"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingSeedGroup")) !=
                   nullptr,
               "image preprocessing page should restore Seed group"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingCropGroup")) !=
                   nullptr,
               "image preprocessing page should restore Crop Region group"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindWorkbenchGroup(window,
                                  QStringLiteral("xqImagePreprocessingResampleGroup")) !=
                   nullptr,
               "image preprocessing page should restore Resample / Surface group"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.findChild<QTextEdit*>(QStringLiteral(
                   "xqImagePreprocessingDiagnosticsText")) != nullptr,
               "image preprocessing page should restore original diagnostics text area"))
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
        operationSelector->findData(QStringLiteral("connected-threshold")));
    app.processEvents();
    auto* seedsEditor =
        FindPointListParameter(window, QStringLiteral("seeds"));
    if (Expect(seedsEditor != nullptr,
               "connected threshold should expose an editable seeds parameter"))
    {
        delete context;
        return 1;
    }
    seedsEditor->setText(QStringLiteral("1,2,3; 4,5,6"));
    app.processEvents();
    seedsEditor->editingFinished();
    app.processEvents();
    const QVariantList seedPoints =
        context->WorkflowOperations()
            ->ParameterValues(QStringLiteral("image-preprocessing"),
                              QStringLiteral("connected-threshold"))
            .value(QStringLiteral("seeds"))
            .toList();
    if (Expect(seedPoints.size() == 2 &&
                   seedPoints.at(0).toList().size() == 3 &&
                   seedPoints.at(0).toList().at(0).toInt() == 1 &&
                   seedPoints.at(1).toList().at(2).toInt() == 6,
               "editing seeds should update Core point-list state"))
    {
        delete context;
        return 1;
    }
    QStringList pointListDiagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&pointListDiagnostics](const QString& message) {
                         pointListDiagnostics.append(message);
                     });
    seedsEditor->setText(QStringLiteral("bad seed"));
    app.processEvents();
    seedsEditor->editingFinished();
    app.processEvents();
    const QVariantList unchangedSeedPoints =
        context->WorkflowOperations()
            ->ParameterValues(QStringLiteral("image-preprocessing"),
                              QStringLiteral("connected-threshold"))
            .value(QStringLiteral("seeds"))
            .toList();
    if (Expect(unchangedSeedPoints.size() == 2 &&
                   unchangedSeedPoints.at(1).toList().at(2).toInt() == 6,
               "invalid seeds text should not replace the last valid state"))
    {
        delete context;
        return 1;
    }
    if (Expect(pointListDiagnostics.contains(QStringLiteral(
                   "Point list entries must use x,y,z format.")),
               "invalid seeds text should post a point-list diagnostic"))
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
                   "Run Image Preprocessing failed: Active image node is required for image preprocessing.")),
               "operation action should report infrastructure validation"))
    {
        delete context;
        return 1;
    }

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
    {
        delete context;
        return 1;
    }
    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("OperationUi.xqproj"));
    if (Expect(context->Projects()->CreateProject(QStringLiteral("OperationUi"),
                                                  projectPath,
                                                  &errorMessage),
               "operation UI project should be created"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "operation UI project should save selected operation state"))
    {
        delete context;
        return 1;
    }

    operationSelector->setCurrentIndex(
        operationSelector->findData(QStringLiteral("crop")));
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("crop"),
               "operation selector should switch away before project reopen"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->ProjectSession()->Open(projectPath, &errorMessage),
               "operation UI project should reopen with persisted operation state"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(operationSelector->currentData().toString() ==
                   QStringLiteral("gaussian-smoothing"),
               "project open should refresh operation selector from persisted state"))
    {
        delete context;
        return 1;
    }
    if (Expect(actionButton->text() == QStringLiteral("Run Gaussian Smoothing"),
               "project open should refresh action text from persisted operation"))
    {
        delete context;
        return 1;
    }
    sigmaEditor = FindNumericParameter(window, QStringLiteral("sigma"));
    if (Expect(sigmaEditor != nullptr && sigmaEditor->value() == 1.25,
               "project open should rebuild parameter panel with persisted values"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
