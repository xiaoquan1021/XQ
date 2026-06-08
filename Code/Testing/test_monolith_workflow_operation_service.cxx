#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowOperationService.h"

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

xq::core::WorkflowOperationDescriptor Operation(const QString& id,
                                                const QString& title)
{
    xq::core::WorkflowOperationDescriptor operation;
    operation.Id = id;
    operation.Title = title;
    return operation;
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::WorkflowOperationService service;

    QString message;
    if (Expect(service.OperationsForWorkflow(QStringLiteral("image-preprocessing")).isEmpty(),
               "new workflow operation service should start empty"))
        return 1;

    QVector<xq::core::WorkflowOperationDescriptor> operations = {
        Operation(QStringLiteral("binary-threshold"),
                  QStringLiteral("Binary Threshold")),
        Operation(QStringLiteral("gaussian-smoothing"),
                  QStringLiteral("Gaussian Smoothing")),
    };
    operations[1].Parameters.push_back(
        Parameter(QStringLiteral("sigma"),
                  QStringLiteral("Sigma"),
                  xq::core::WorkflowOperationParameterValueType::NumericScalar));
    if (Expect(service.RegisterOperations(QStringLiteral("image-preprocessing"),
                                          operations,
                                          &message),
               "known workflow operation registration should succeed"))
        return 1;
    if (Expect(message.isEmpty(),
               "successful operation registration should clear message"))
        return 1;

    const auto registered =
        service.OperationsForWorkflow(QStringLiteral(" image-preprocessing "));
    if (Expect(registered.size() == 2,
               "registered workflow operations should be queryable"))
        return 1;
    if (Expect(registered.at(0).Id == QStringLiteral("binary-threshold") &&
                   registered.at(0).Title == QStringLiteral("Binary Threshold"),
               "registered workflow operations should preserve order and title"))
        return 1;
    if (Expect(registered.at(1).Parameters.size() == 1 &&
                   registered.at(1).Parameters.at(0).Id ==
                       QStringLiteral("sigma") &&
                   registered.at(1).Parameters.at(0).Title ==
                       QStringLiteral("Sigma") &&
                   registered.at(1).Parameters.at(0).Type ==
                       xq::core::WorkflowOperationParameterValueType::NumericScalar,
               "registered workflow operations should preserve parameters"))
        return 1;
    if (Expect(service.SelectedOperationId(QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("binary-threshold"),
               "first registered operation should become default selection"))
        return 1;

    int selectionChanges = 0;
    QString lastWorkflowId;
    QString lastOperationId;
    QObject::connect(&service,
                     &xq::core::WorkflowOperationService::SelectedOperationChanged,
                     [&selectionChanges,
                      &lastWorkflowId,
                      &lastOperationId](const QString& workflowId,
                                         const QString& operationId) {
                         ++selectionChanges;
                         lastWorkflowId = workflowId;
                         lastOperationId = operationId;
                     });

    if (Expect(service.SelectOperation(QStringLiteral(" image-preprocessing "),
                                       QStringLiteral(" gaussian-smoothing "),
                                       &message),
               "registered operation should be selectable"))
        return 1;
    if (Expect(service.SelectedOperationId(QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("gaussian-smoothing"),
               "selected operation id should update"))
        return 1;
    if (Expect(selectionChanges == 1 &&
                   lastWorkflowId == QStringLiteral("image-preprocessing") &&
                   lastOperationId == QStringLiteral("gaussian-smoothing"),
               "operation selection should emit normalized ids"))
        return 1;

    if (Expect(service.SelectOperation(QStringLiteral("image-preprocessing"),
                                       QStringLiteral("gaussian-smoothing"),
                                       &message),
               "re-selecting current operation should still succeed"))
        return 1;
    if (Expect(selectionChanges == 1,
               "re-selecting current operation should not emit a change"))
        return 1;

    if (Expect(!service.SelectOperation(QStringLiteral("image-preprocessing"),
                                        QStringLiteral("crop"),
                                        &message),
               "unknown operation selection should be rejected"))
        return 1;
    if (Expect(message == QStringLiteral("Workflow operation was not found."),
               "unknown operation rejection should use explicit message"))
        return 1;
    if (Expect(service.SelectedOperationId(QStringLiteral("image-preprocessing")) ==
                   QStringLiteral("gaussian-smoothing"),
               "failed operation selection should not mutate selected operation"))
        return 1;

    if (Expect(!service.RegisterOperations(QStringLiteral("missing-workflow"),
                                           operations,
                                           &message),
               "unknown workflow operation registration should be rejected"))
        return 1;
    if (Expect(message == QStringLiteral("Workflow was not found."),
               "unknown workflow registration should use explicit message"))
        return 1;

    operations.push_back(Operation(QStringLiteral("gaussian-smoothing"),
                                  QStringLiteral("Duplicate")));
    if (Expect(!service.RegisterOperations(QStringLiteral("image-preprocessing"),
                                           operations,
                                           &message),
               "duplicate operation ids should be rejected"))
        return 1;
    if (Expect(message == QStringLiteral("Duplicate workflow operation id."),
               "duplicate operation rejection should use explicit message"))
        return 1;

    operations = {
        Operation(QStringLiteral("crop"),
                  QStringLiteral("Crop")),
    };
    operations[0].Parameters.push_back(
        Parameter(QStringLiteral("origin-x"),
                  QStringLiteral("Origin X"),
                  xq::core::WorkflowOperationParameterValueType::IntegerScalar));
    operations[0].Parameters.push_back(
        Parameter(QStringLiteral("origin-x"),
                  QStringLiteral("Duplicate Origin X"),
                  xq::core::WorkflowOperationParameterValueType::IntegerScalar));
    if (Expect(!service.RegisterOperations(QStringLiteral("image-preprocessing"),
                                           operations,
                                           &message),
               "duplicate parameter ids should be rejected"))
        return 1;
    if (Expect(message == QStringLiteral("Duplicate workflow operation parameter id."),
               "duplicate parameter rejection should use explicit message"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->WorkflowOperations() != nullptr,
               "ApplicationContext should expose workflow operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowOperations()
                   ->OperationsForWorkflow(QStringLiteral("image-preprocessing"))
                   .isEmpty(),
               "default context workflow operations should start empty"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
