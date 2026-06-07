#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowSelectionService.h"

#include <QCoreApplication>

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

xq::core::DataImportRequest MakeImport(const QString& id,
                                       const QString& displayName,
                                       xq::core::DataWorkflowRole role)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = role;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    auto* workflowActions = context->WorkflowActions();

    if (Expect(workflowActions != nullptr,
               "ApplicationContext should expose WorkflowActionService"))
    {
        delete context;
        return 1;
    }

    QString message;
    QString errorMessage;
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowActions->RequestActiveWorkflowAction(&message),
               "image preprocessing should reject missing selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Select compatible data before running Image Preprocessing."),
               "missing data rejection should include workflow title"))
    {
        delete context;
        return 1;
    }

    const auto imageImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(imageImport.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowActions->RequestActiveWorkflowAction(&message),
               "image preprocessing should accept selected image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Image Preprocessing action requested for CTA Image."),
               "accepted image preprocessing action should include selected data"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "meshing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    if (Expect(!workflowActions->RequestActiveWorkflowAction(&message),
               "meshing should reject selected image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Select compatible data before running Meshing."),
               "incompatible data rejection should include meshing title"))
    {
        delete context;
        return 1;
    }

    const auto modelImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("model-001"),
                                           QStringLiteral("Aorta Model"),
                                           xq::core::DataWorkflowRole::Model),
                                       &errorMessage);
    if (Expect(modelImport.Succeeded, "model import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowActions->RequestActiveWorkflowAction(&message),
               "meshing should accept selected model data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Meshing action requested for Aorta Model."),
               "accepted meshing action should include selected model data"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
