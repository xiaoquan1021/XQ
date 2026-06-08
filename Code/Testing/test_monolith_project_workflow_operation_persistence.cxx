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

xq::core::WorkflowOperationParameterOption Option(const QString& id,
                                                  const QString& title)
{
    xq::core::WorkflowOperationParameterOption option;
    option.Id = id;
    option.Title = title;
    return option;
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

QVector<xq::core::WorkflowOperationDescriptor> RichImageOperations()
{
    return {
        Operation(QStringLiteral("connected-threshold"),
                  QStringLiteral("Connected Threshold"),
                  {Parameter(QStringLiteral("seeds"),
                             QStringLiteral("Seeds"),
                             xq::core::WorkflowOperationParameterValueType::
                                 IntegerPointList)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> FlowOperations()
{
    auto solverProfile =
        Parameter(QStringLiteral("solver-profile"),
                  QStringLiteral("Solver Profile"),
                  xq::core::WorkflowOperationParameterValueType::Option);
    solverProfile.Options = {
        Option(QStringLiteral("steady"), QStringLiteral("Steady")),
        Option(QStringLiteral("pulsatile"), QStringLiteral("Pulsatile")),
        Option(QStringLiteral("transient"), QStringLiteral("Transient")),
    };

    return {
        Operation(QStringLiteral("configure-cfd-job"),
                  QStringLiteral("Configure CFD Job"),
                  {solverProfile}),
    };
}

bool RegisterTestOperations(xq::core::WorkflowOperationService& operations,
                            QString* errorMessage)
{
    return operations.RegisterOperations(QStringLiteral("image-preprocessing"),
                                         TestOperations(),
                                         errorMessage);
}

bool RegisterRichOperations(xq::core::WorkflowOperationService& operations,
                            QString* errorMessage)
{
    return operations.RegisterOperations(QStringLiteral("image-preprocessing"),
                                         RichImageOperations(),
                                         errorMessage) &&
           operations.RegisterOperations(QStringLiteral("flow-simulation"),
                                         FlowOperations(),
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

QVariantList Point(int x, int y, int z)
{
    QVariantList point;
    point.push_back(x);
    point.push_back(y);
    point.push_back(z);
    return point;
}

QVariantList SeedPoints()
{
    QVariantList points;
    points.push_back(Point(1, 2, 3));
    points.push_back(Point(4, 5, 6));
    return points;
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

bool WriteInvalidOptionFixture(const QString& projectPath)
{
    QJsonObject parameterValues;
    parameterValues.insert(QStringLiteral("solver-profile"),
                           QStringLiteral("missing-profile"));

    QJsonObject operationState;
    operationState.insert(QStringLiteral("workflowId"),
                          QStringLiteral("flow-simulation"));
    operationState.insert(QStringLiteral("selectedOperationId"),
                          QStringLiteral("configure-cfd-job"));
    operationState.insert(QStringLiteral("operationId"),
                          QStringLiteral("configure-cfd-job"));
    operationState.insert(QStringLiteral("parameters"), parameterValues);

    QJsonArray workflowOperations;
    workflowOperations.append(operationState);

    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"),
                         QStringLiteral("InvalidOptionState"));
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

    const QString richProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("RichOperationState.xqproj"));
    xq::core::ProjectService richProject;
    if (Expect(richProject.CreateProject(QStringLiteral("RichOperationState"),
                                         richProjectPath,
                                         &errorMessage),
               "rich operation-state project should be created"))
        return 1;

    xq::core::WorkflowOperationService richOperations;
    if (Expect(RegisterRichOperations(richOperations, &errorMessage),
               "rich workflow operations should register before save"))
        return 1;
    if (Expect(richOperations.SetParameterValue(
                   QStringLiteral("flow-simulation"),
                   QStringLiteral("configure-cfd-job"),
                   QStringLiteral("solver-profile"),
                   QStringLiteral("transient"),
                   &errorMessage),
               "option parameter should be edited before save"))
        return 1;
    if (Expect(richOperations.SetParameterValue(
                   QStringLiteral("image-preprocessing"),
                   QStringLiteral("connected-threshold"),
                   QStringLiteral("seeds"),
                   SeedPoints(),
                   &errorMessage),
               "point-list parameter should be edited before save"))
        return 1;
    if (Expect(richProject.SaveProject(catalog,
                                       hierarchy,
                                       richOperations,
                                       &errorMessage),
               "SaveProject should persist rich workflow operation state"))
        return 1;

    QFile richSavedFile(richProjectPath);
    if (Expect(richSavedFile.open(QIODevice::ReadOnly),
               "saved rich operation-state project should be readable"))
        return 1;
    const QJsonArray richSavedOperations =
        QJsonDocument::fromJson(richSavedFile.readAll())
            .object()
            .value(QStringLiteral("project"))
            .toObject()
            .value(QStringLiteral("workflowOperations"))
            .toArray();
    bool sawOptionJson = false;
    bool sawPointListJson = false;
    for (const auto& operationItem : richSavedOperations)
    {
        const QJsonObject operationState = operationItem.toObject();
        const QJsonArray operationValues =
            operationState.value(QStringLiteral("operations")).toArray();
        for (const auto& operationValue : operationValues)
        {
            const QJsonObject operationObject = operationValue.toObject();
            const QJsonObject parameters =
                operationObject.value(QStringLiteral("parameters")).toObject();
            if (parameters.value(QStringLiteral("solver-profile")).toString() ==
                QStringLiteral("transient"))
            {
                sawOptionJson = true;
            }
            if (parameters.value(QStringLiteral("seeds")).isArray() &&
                parameters.value(QStringLiteral("seeds"))
                        .toArray()
                        .size() == 2)
            {
                sawPointListJson = true;
            }
        }
    }
    if (Expect(sawOptionJson,
               "rich operation JSON should persist option parameter ids"))
        return 1;
    if (Expect(sawPointListJson,
               "rich operation JSON should persist point-list parameter arrays"))
        return 1;

    xq::core::ProjectService richOpenedProject;
    xq::core::WorkflowOperationService richOpenedOperations;
    if (Expect(RegisterRichOperations(richOpenedOperations, &errorMessage),
               "rich workflow operations should register before open"))
        return 1;
    if (Expect(richOpenedProject.OpenProject(richProjectPath,
                                             openedCatalog,
                                             openedHierarchy,
                                             richOpenedOperations,
                                             &errorMessage),
               "OpenProject should restore rich workflow operation state"))
        return 1;
    if (Expect(richOpenedOperations
                   .ParameterValues(QStringLiteral("flow-simulation"),
                                    QStringLiteral("configure-cfd-job"))
                   .value(QStringLiteral("solver-profile"))
                   .toString() == QStringLiteral("transient"),
               "OpenProject should restore option parameter values"))
        return 1;
    const QVariantList restoredSeeds =
        richOpenedOperations
            .ParameterValues(QStringLiteral("image-preprocessing"),
                             QStringLiteral("connected-threshold"))
            .value(QStringLiteral("seeds"))
            .toList();
    if (Expect(restoredSeeds.size() == 2 &&
                   restoredSeeds.at(0).toList().at(0).toInt() == 1 &&
                   restoredSeeds.at(1).toList().at(2).toInt() == 6,
               "OpenProject should restore point-list parameter values"))
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

    const QString invalidOptionPath =
        QDir(tempDir.path()).filePath(QStringLiteral("InvalidOption.xqproj"));
    if (Expect(WriteInvalidOptionFixture(invalidOptionPath),
               "invalid option workflow operation fixture should be written"))
        return 1;
    if (Expect(richOpenedOperations.SetParameterValue(
                   QStringLiteral("flow-simulation"),
                   QStringLiteral("configure-cfd-job"),
                   QStringLiteral("solver-profile"),
                   QStringLiteral("pulsatile"),
                   &errorMessage),
               "existing option state should be set before failed open"))
        return 1;
    errorMessage.clear();
    if (Expect(!richOpenedProject.OpenProject(invalidOptionPath,
                                              openedCatalog,
                                              openedHierarchy,
                                              richOpenedOperations,
                                              &errorMessage),
               "OpenProject should reject unknown option parameter values"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("option")),
               "unknown option rejection should report useful error"))
        return 1;
    if (Expect(richOpenedOperations
                   .ParameterValues(QStringLiteral("flow-simulation"),
                                    QStringLiteral("configure-cfd-job"))
                   .value(QStringLiteral("solver-profile"))
                   .toString() == QStringLiteral("pulsatile"),
               "failed option-state open should not mutate option values"))
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
