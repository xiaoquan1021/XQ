#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>

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

QComboBox* FindSelector(xq::presentation::MainWindow& window)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqWorkflowOperationSelector_path"));
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_path"));
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
    request.RequestedId = QStringLiteral("path-image");
    request.SourcePath = QStringLiteral("C:/studies/path-image");
    request.DisplayName = QStringLiteral("Path CTA");
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
    xq::presentation::MainWindow window(*context);

    auto* selector = FindSelector(window);
    if (Expect(selector != nullptr,
               "Path page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(selector->count() == 3,
               "Path selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(selector->itemData(0).toString() ==
                       QStringLiteral("create-centerline") &&
                   selector->itemText(0) ==
                       QStringLiteral("Create Centerline"),
               "Path selector should preserve operation order"))
    {
        delete context;
        return 1;
    }
    auto* actionButton = FindActionButton(window);
    if (Expect(actionButton != nullptr &&
                   actionButton->text() ==
                       QStringLiteral("Run Create Centerline"),
               "Path action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindIntegerParameter(window,
                                    QStringLiteral("control-point-count")) !=
                       nullptr,
               "Create Centerline should expose control point count"))
    {
        delete context;
        return 1;
    }

    auto* controlPointCount =
        FindIntegerParameter(window, QStringLiteral("control-point-count"));
    if (Expect(controlPointCount != nullptr,
               "Create Centerline control point count editor should be visible"))
    {
        delete context;
        return 1;
    }
    controlPointCount->setValue(7);
    app.processEvents();

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
    {
        delete context;
        return 1;
    }
    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("PathOperationUi.xqproj"));
    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(
                   QStringLiteral("PathOperationUi"),
                   projectPath,
                   &errorMessage),
               "Path operation UI project should be created"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "Path operation UI project should save parameter state"))
    {
        delete context;
        return 1;
    }
    controlPointCount->setValue(2);
    app.processEvents();
    if (Expect(context->ProjectSession()->Open(projectPath, &errorMessage),
               "Path operation UI project should reopen with persisted parameter state"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    controlPointCount =
        FindIntegerParameter(window, QStringLiteral("control-point-count"));
    if (Expect(controlPointCount != nullptr && controlPointCount->value() == 7,
               "project open should refresh unchanged Path operation parameters"))
    {
        delete context;
        return 1;
    }

    selector->setCurrentIndex(
        selector->findData(QStringLiteral("smooth-path")));
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("path")) ==
                   QStringLiteral("smooth-path"),
               "Path selector should update Core state"))
    {
        delete context;
        return 1;
    }
    if (Expect(actionButton->text() == QStringLiteral("Run Smooth Path"),
               "Path action should update after selector change"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("smoothing-factor")) !=
                       nullptr &&
                   FindIntegerParameter(window,
                                        QStringLiteral("iteration-count")) !=
                       nullptr,
               "Smooth Path should expose smoothing parameters"))
    {
        delete context;
        return 1;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(), &errorMessage);
    if (Expect(importResult.Succeeded,
               "Path image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("path")),
               "Path workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(actionButton->isEnabled(),
               "compatible image should enable Path action"))
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
    actionButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Path succeeded: Smooth Path path operation accepted Path CTA.")),
               "Path action should report selected operation"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
