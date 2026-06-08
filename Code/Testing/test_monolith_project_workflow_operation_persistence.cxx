#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

xq::core::WorkflowOperationParameterDescriptor Parameter(
    const QString& id,
    const QString& title,
    xq::core::WorkflowOperationParameterValueType type)
{
    xq::core::WorkflowOperationParameterDescriptor parameter;
    parameter.Id = id;
    parameter.Title = title;
    parameter.Type = type;
    parameter.Required = true;
    return parameter;
}

xq::core::WorkflowOperationDescriptor Operation(
    const QString& id,
    const QString& title,
    const QVector<xq::core::WorkflowOperationParameterDescriptor>& parameters)
{
    xq::core::WorkflowOperationDescriptor operation;
    operation.Id = id;
    operation.Title = title;
    operation.Parameters = parameters;
    return operation;
}

QVector<xq::core::WorkflowOperationDescriptor> TestOperations()
{
    return {
        Operation(QStringLiteral("binary-threshold"),
                  QStringLiteral("Binary Threshold"),
                  {Parameter(QStringLiteral("lower"),
                             QStringLiteral("Lower"),
                             xq::core::WorkflowOperationParameterValueType::
                                 NumericScalar)}),
        Operation(QStringLiteral("gaussian-smoothing"),
                  QStringLiteral("Gaussian Smoothing"),
                  {Parameter(QStringLiteral("sigma"),
                             QStringLiteral("Sigma"),
                             xq::core::WorkflowOperationParameterValueType::
                                 NumericScalar)}),
    };
}

bool RegisterTestOperations(xq::core::WorkflowOperationService& operations,
                            QString* errorMessage)
{
    return operations.RegisterOperations(QStringLiteral("image-preprocessing"),
                                         TestOperations(),
                                         errorMessage);
}

bool SelectGaussian(xq::core::WorkflowOperationService& operations,
                    double sigma,
                    QString* errorMessage)
{
    if (!operations.SelectOperation(QStringLiteral("image-preprocessing"),
                                    QStringLiteral("gaussian-smoothing"),
                                    errorMessage))
        return false;

    return operations.SetParameterValue(QStringLiteral("image-preprocessing"),
                                        QStringLiteral("gaussian-smoothing"),
                                        QStringLiteral("sigma"),
                                        sigma,
                                        errorMessage);
}

bool WriteInvalidParameterFixture(const QString& projectPath)
{
    QJsonObject parameterValues;
    parameterValues.insert(QStringLiteral("unknown-parameter"), 2.0);

    QJsonObject operationState;
    operationState.insert(QStringLiteral("workflowId"),
                          QStringLiteral("image-preprocessing"));
    operationState.insert(QStringLiteral("selectedOperationId"),
                          QStringLiteral("gaussian-smoothing"));
    operationState.insert(QStringLiteral("operationId"),
                          QStringLiteral("gaussian-smoothing"));
    operationState.insert(QStringLiteral("parameters"), parameterValues);

    QJsonArray workflowOperations;
    workflowOperations.append(operationState);

    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"),
                         QStringLiteral("InvalidOperationState"));
    projectObject.insert(QStringLiteral("workspaceDirectory"),
                         QStringLiteral("workspace"));
    projectObject.insert(QStringLiteral("workflowOperations"),
                         workflowOperations);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("2.0"));
    root.insert(QStringLiteral("project"), projectObject);

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    QString errorMessage;
    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("OperationState.xqproj"));

    xq::core::ProjectService project;
    if (Expect(project.CreateProject(QStringLiteral("OperationState"),
                                     projectPath,
                                     &errorMessage),
               "operation-state project should be created"))
        return 1;

    xq::core::DataCatalogService catalog;
    xq::core::DataHierarchyService hierarchy;
    xq::core::WorkflowOperationService operations;
    if (Expect(RegisterTestOperations(operations, &errorMessage),
               "workflow operations should register before save"))
        return 1;
    if (Expect(SelectGaussian(operations, 1.75, &errorMessage),
               "workflow operation parameter should be edited before save"))
        return 1;
    if (Expect(project.SaveProject(catalog,
                                   hierarchy,
                                   operations,
                                   &errorMessage),
               "SaveProject should persist workflow operation state"))
        return 1;

    QFile savedFile(projectPath);
    if (Expect(savedFile.open(QIODevice::ReadOnly),
               "saved operation-state project should be readable"))
        return 1;
    const QJsonObject savedProject =
        QJsonDocument::fromJson(savedFile.readAll())
            .object()
            .value(QStringLiteral("project"))
            .toObject();
    const QJsonArray savedOperations =
        savedProject.value(QStringLiteral("workflowOperations")).toArray();
    if (Expect(savedOperations.size() == 1,
               "project JSON should persist one workflow operation state"))
        return 1;
    const QJsonObject savedOperation = savedOperations.at(0).toObject();
    if (Expect(savedOperation.value(QStringLiteral("workflowId")).toString() ==
                   QStringLiteral("image-preprocessing"),
               "workflow operation JSON should include workflow id"))
        return 1;
    if (Expect(savedOperation
                   .value(QStringLiteral("selectedOperationId"))
                   .toString() == QStringLiteral("gaussian-smoothing"),
               "workflow operation JSON should include selected operation"))
        return 1;
    if (Expect(savedOperation.value(QStringLiteral("parameters"))
                   .toObject()
                   .value(QStringLiteral("sigma"))
                   .toDouble() == 1.75,
               "workflow operation JSON should include edited parameter values"))
        return 1;

    xq::core::ProjectService openedProject;
    xq::core::DataCatalogService openedCatalog;
    xq::core::DataHierarchyService openedHierarchy;
    xq::core::WorkflowOperationService openedOperations;
    if (Expect(RegisterTestOperations(openedOperations, &errorMessage),
               "workflow operations should register before open"))
        return 1;
    if (Expect(openedProject.OpenProject(projectPath,
                                         openedCatalog,
                                         openedHierarchy,
                                         openedOperations,
                                         &errorMessage),
               "OpenProject should restore workflow operation state"))
        return 1;
    if (Expect(openedOperations.SelectedOperationId(
                   QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("gaussian-smoothing"),
               "OpenProject should restore selected workflow operation"))
        return 1;
    if (Expect(openedOperations
                   .ParameterValues(QStringLiteral("image-preprocessing"),
                                    QStringLiteral("gaussian-smoothing"))
                   .value(QStringLiteral("sigma"))
                   .toDouble() == 1.75,
               "OpenProject should restore workflow operation parameters"))
        return 1;

    const QString invalidPath =
        QDir(tempDir.path()).filePath(QStringLiteral("InvalidOperation.xqproj"));
    if (Expect(WriteInvalidParameterFixture(invalidPath),
               "invalid workflow operation fixture should be written"))
        return 1;

    if (Expect(SelectGaussian(openedOperations, 3.5, &errorMessage),
               "existing operation state should be set before failed open"))
        return 1;
    errorMessage.clear();
    if (Expect(!openedProject.OpenProject(invalidPath,
                                          openedCatalog,
                                          openedHierarchy,
                                          openedOperations,
                                          &errorMessage),
               "OpenProject should reject unknown workflow operation parameters"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("operation parameter")),
               "unknown workflow operation parameter should report useful error"))
        return 1;
    if (Expect(openedProject.CurrentProject() != nullptr &&
                   openedProject.CurrentProject()->Name ==
                       QStringLiteral("OperationState"),
               "failed operation-state open should not replace project metadata"))
        return 1;
    if (Expect(openedOperations
                   .ParameterValues(QStringLiteral("image-preprocessing"),
                                    QStringLiteral("gaussian-smoothing"))
                   .value(QStringLiteral("sigma"))
                   .toDouble() == 3.5,
               "failed operation-state open should not mutate operation values"))
        return 1;

    const QString sessionPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SessionOperation.xqproj"));
    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->Projects()->CreateProject(QStringLiteral("Session"),
                                                  sessionPath,
                                                  &errorMessage),
               "session operation-state project should be created"))
    {
        delete context;
        return 1;
    }
    if (Expect(RegisterTestOperations(*context->WorkflowOperations(),
                                      &errorMessage),
               "session workflow operations should register before save"))
    {
        delete context;
        return 1;
    }
    if (Expect(SelectGaussian(*context->WorkflowOperations(),
                              2.25,
                              &errorMessage),
               "session workflow operation state should be edited before save"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "ProjectSession save should persist operation state"))
    {
        delete context;
        return 1;
    }
    delete context;

    auto* reopenedContext = xq::core::ApplicationContext::CreateDefault();
    if (Expect(RegisterTestOperations(*reopenedContext->WorkflowOperations(),
                                      &errorMessage),
               "reopened context should register operations before open"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->ProjectSession()->Open(sessionPath,
                                                       &errorMessage),
               "ProjectSession open should restore operation state"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->WorkflowOperations()
                   ->SelectedOperationId(
                       QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("gaussian-smoothing"),
               "ProjectSession open should restore selected operation"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->WorkflowOperations()
                   ->ParameterValues(QStringLiteral("image-preprocessing"),
                                     QStringLiteral("gaussian-smoothing"))
                   .value(QStringLiteral("sigma"))
                   .toDouble() == 2.25,
               "ProjectSession open should restore operation parameter values"))
    {
        delete reopenedContext;
        return 1;
    }

    delete reopenedContext;
    return 0;
}
