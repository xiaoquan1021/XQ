#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Presentation/xq_MainWindow.h"
#include "xq_MonolithApplication.h"

#include <QAction>
#include <QApplication>

#include <iostream>
#include <memory>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

class CancelPathProvider : public xq::core::FileImportPathProvider
{
public:
    mutable int Invocations = 0;

    QString ChooseFilePath() const override
    {
        ++Invocations;
        return {};
    }
};

xq::core::DataImportRequest MakeImageImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001.nii");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto context = std::unique_ptr<xq::core::ApplicationContext>(
        xq::core::ApplicationContext::CreateDefault());
    CancelPathProvider provider;
    auto configuredWindow =
        xq::CreateConfiguredMainWindow(*context, &provider);
    auto* window = configuredWindow->Window.get();

    if (Expect(configuredWindow->RenderRefresh != nullptr,
               "configured monolith window should own a render refresh service"))
        return 1;

    auto* importAction =
        window->findChild<QAction*>(QStringLiteral("xqImportDataAction"));
    if (Expect(importAction != nullptr,
               "configured monolith window should expose import action"))
        return 1;
    if (Expect(importAction->isEnabled(),
               "configured monolith import action should be enabled"))
        return 1;

    int notConfiguredDiagnostics = 0;
    QObject::connect(context.get(),
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&notConfiguredDiagnostics](const QString& message) {
                         if (message ==
                             QStringLiteral("No data import command is configured."))
                         {
                             ++notConfiguredDiagnostics;
                         }
                     });

    importAction->trigger();
    app.processEvents();

    if (Expect(provider.Invocations == 1,
               "configured monolith import action should invoke provider"))
        return 1;
    if (Expect(notConfiguredDiagnostics == 0,
               "configured monolith import action should have an import command"))
        return 1;
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "cancelled real file dialog path should not mutate catalog"))
        return 1;

    auto preprocessingContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *preprocessingContext->WorkflowActions(),
        preprocessingContext->WorkflowOperations());
    CancelPathProvider preprocessingProvider;
    auto preprocessingWindow =
        xq::CreateConfiguredMainWindow(*preprocessingContext,
                                       &preprocessingProvider);

    if (Expect(preprocessingContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "configured preprocessing workflow should be selectable"))
        return 1;

    QString message;
    const auto importResult =
        preprocessingContext->DataImports()->Import(MakeImageImport(),
                                                    &message);
    if (Expect(importResult.Succeeded,
               "configured preprocessing image import should succeed"))
        return 1;

    if (Expect(!preprocessingContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured preprocessing action should require a MITK source node"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active image node is required for image preprocessing."),
               "configured preprocessing action should use infrastructure diagnostic"))
        return 1;
    const auto history = preprocessingContext->Tasks()->History();
    if (Expect(!history.empty() &&
                   !history.back().Succeeded &&
                   history.back().Message == message,
               "configured preprocessing action failure should be recorded"))
        return 1;

    auto pathContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *pathContext->WorkflowActions(),
        pathContext->WorkflowOperations());
    CancelPathProvider pathProvider;
    auto pathWindow =
        xq::CreateConfiguredMainWindow(*pathContext, &pathProvider);

    if (Expect(pathContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("path")),
               "configured path workflow should be selectable"))
        return 1;

    const auto pathImportResult =
        pathContext->DataImports()->Import(MakeImageImport(), &message);
    if (Expect(pathImportResult.Succeeded,
               "configured path image import should succeed"))
        return 1;

    if (Expect(!pathContext->WorkflowActions()->RunActiveWorkflowAction(
                   &message),
               "configured path action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Path creation requires at least two seed points."),
               "configured path action should require seed points"))
        return 1;

    return 0;
}
