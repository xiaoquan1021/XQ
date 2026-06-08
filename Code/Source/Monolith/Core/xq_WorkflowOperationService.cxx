#include "xq_WorkflowOperationService.h"

#include "xq_WorkflowRegistry.h"

#include <QHash>
#include <QSet>
#include <QVariantList>

namespace xq::core
{

WorkflowOperationService::WorkflowOperationService(QObject* parent)
    : QObject(parent)
{
}

QVector<WorkflowOperationDescriptor>
WorkflowOperationService::OperationsForWorkflow(
    const QString& workflowId) const
{
    return m_Operations.value(workflowId.trimmed());
}

QVariantMap WorkflowOperationService::ParameterValues(
    const QString& workflowId,
    const QString& operationId) const
{
    return m_ParameterValues.value(
        ParameterValueKey(workflowId.trimmed(), operationId.trimmed()));
}

QString WorkflowOperationService::SelectedOperationId(
    const QString& workflowId) const
{
    return m_SelectedOperationIds.value(workflowId.trimmed());
}

bool WorkflowOperationService::RegisterOperations(
    const QString& workflowId,
    const QVector<WorkflowOperationDescriptor>& operations,
    QString* message)
{
    const QString normalizedWorkflowId = workflowId.trimmed();
    if (!FindWorkflowById(normalizedWorkflowId))
    {
        SetMessage(message, QStringLiteral("Workflow was not found."));
        return false;
    }

    QVector<WorkflowOperationDescriptor> normalizedOperations;
    QSet<QString> operationIds;
    for (const auto& operation : operations)
    {
        WorkflowOperationDescriptor normalizedOperation;
        normalizedOperation.Id = operation.Id.trimmed();
        normalizedOperation.Title = operation.Title.trimmed();
        normalizedOperation.Parameters = operation.Parameters;
        if (normalizedOperation.Id.isEmpty() ||
            normalizedOperation.Title.isEmpty())
        {
            SetMessage(message,
                       QStringLiteral(
                           "Workflow operation id and title are required."));
            return false;
        }

        if (operationIds.contains(normalizedOperation.Id))
        {
            SetMessage(message,
                       QStringLiteral("Duplicate workflow operation id."));
            return false;
        }

        QSet<QString> parameterIds;
        for (auto& parameter : normalizedOperation.Parameters)
        {
            parameter.Id = parameter.Id.trimmed();
            parameter.Title = parameter.Title.trimmed();
            if (parameter.Id.isEmpty() || parameter.Title.isEmpty())
            {
                SetMessage(message,
                           QStringLiteral(
                               "Workflow operation parameter id and title are required."));
                return false;
            }

            if (parameterIds.contains(parameter.Id))
            {
                SetMessage(
                    message,
                    QStringLiteral(
                        "Duplicate workflow operation parameter id."));
                return false;
            }

            parameterIds.insert(parameter.Id);
        }

        operationIds.insert(normalizedOperation.Id);
        normalizedOperations.push_back(normalizedOperation);
    }

    const QString previousSelection =
        m_SelectedOperationIds.value(normalizedWorkflowId);
    QString nextSelection;
    if (!normalizedOperations.isEmpty())
    {
        nextSelection = normalizedOperations.front().Id;
        for (const auto& operation : normalizedOperations)
        {
            if (operation.Id == previousSelection)
            {
                nextSelection = previousSelection;
                break;
            }
        }
    }

    m_Operations.insert(normalizedWorkflowId, normalizedOperations);
    for (const auto& operation : normalizedOperations)
    {
        const QString valueKey =
            ParameterValueKey(normalizedWorkflowId, operation.Id);
        QVariantMap values = m_ParameterValues.value(valueKey);
        for (const auto& parameter : operation.Parameters)
        {
            if (!values.contains(parameter.Id))
                values.insert(parameter.Id, DefaultValueForParameter(parameter));
        }
        m_ParameterValues.insert(valueKey, values);
    }
    if (nextSelection.isEmpty())
        m_SelectedOperationIds.remove(normalizedWorkflowId);
    else
        m_SelectedOperationIds.insert(normalizedWorkflowId, nextSelection);

    SetMessage(message, QString());
    if (previousSelection != nextSelection)
        emit SelectedOperationChanged(normalizedWorkflowId, nextSelection);
    return true;
}

bool WorkflowOperationService::SelectOperation(const QString& workflowId,
                                               const QString& operationId,
                                               QString* message)
{
    const QString normalizedWorkflowId = workflowId.trimmed();
    if (!FindWorkflowById(normalizedWorkflowId))
    {
        SetMessage(message, QStringLiteral("Workflow was not found."));
        return false;
    }

    const QString normalizedOperationId = operationId.trimmed();
    if (!ContainsOperation(normalizedWorkflowId, normalizedOperationId))
    {
        SetMessage(message,
                   QStringLiteral("Workflow operation was not found."));
        return false;
    }

    if (m_SelectedOperationIds.value(normalizedWorkflowId) ==
        normalizedOperationId)
    {
        SetMessage(message, QString());
        return true;
    }

    m_SelectedOperationIds.insert(normalizedWorkflowId, normalizedOperationId);
    SetMessage(message, QString());
    emit SelectedOperationChanged(normalizedWorkflowId, normalizedOperationId);
    return true;
}

bool WorkflowOperationService::SetParameterValue(const QString& workflowId,
                                                 const QString& operationId,
                                                 const QString& parameterId,
                                                 const QVariant& value,
                                                 QString* message)
{
    const QString normalizedWorkflowId = workflowId.trimmed();
    if (!FindWorkflowById(normalizedWorkflowId))
    {
        SetMessage(message, QStringLiteral("Workflow was not found."));
        return false;
    }

    const QString normalizedOperationId = operationId.trimmed();
    if (!ContainsOperation(normalizedWorkflowId, normalizedOperationId))
    {
        SetMessage(message,
                   QStringLiteral("Workflow operation was not found."));
        return false;
    }

    const QString normalizedParameterId = parameterId.trimmed();
    if (!ContainsParameter(normalizedWorkflowId,
                           normalizedOperationId,
                           normalizedParameterId))
    {
        SetMessage(
            message,
            QStringLiteral("Workflow operation parameter was not found."));
        return false;
    }

    const QString valueKey =
        ParameterValueKey(normalizedWorkflowId, normalizedOperationId);
    QVariantMap values = m_ParameterValues.value(valueKey);
    if (values.value(normalizedParameterId) == value)
    {
        SetMessage(message, QString());
        return true;
    }

    values.insert(normalizedParameterId, value);
    m_ParameterValues.insert(valueKey, values);
    SetMessage(message, QString());
    emit ParameterValueChanged(normalizedWorkflowId,
                               normalizedOperationId,
                               normalizedParameterId,
                               value);
    return true;
}

void WorkflowOperationService::SetMessage(QString* message,
                                          const QString& value)
{
    if (message)
        *message = value;
}

bool WorkflowOperationService::ContainsOperation(
    const QString& workflowId,
    const QString& operationId) const
{
    const auto operations = OperationsForWorkflow(workflowId);
    for (const auto& operation : operations)
    {
        if (operation.Id == operationId)
            return true;
    }

    return false;
}

bool WorkflowOperationService::ContainsParameter(
    const QString& workflowId,
    const QString& operationId,
    const QString& parameterId) const
{
    const auto operations = OperationsForWorkflow(workflowId);
    for (const auto& operation : operations)
    {
        if (operation.Id != operationId)
            continue;

        for (const auto& parameter : operation.Parameters)
        {
            if (parameter.Id == parameterId)
                return true;
        }
    }

    return false;
}

QVariant WorkflowOperationService::DefaultValueForParameter(
    const WorkflowOperationParameterDescriptor& parameter)
{
    switch (parameter.Type)
    {
    case WorkflowOperationParameterValueType::NumericScalar:
        return 0.0;
    case WorkflowOperationParameterValueType::IntegerScalar:
        return 0;
    case WorkflowOperationParameterValueType::IntegerPointList:
        return QVariantList();
    }

    return {};
}

QString WorkflowOperationService::ParameterValueKey(const QString& workflowId,
                                                    const QString& operationId)
{
    return QStringLiteral("%1/%2").arg(workflowId, operationId);
}

} // namespace xq::core
