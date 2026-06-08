#include "xq_WorkflowOperationService.h"

#include "xq_WorkflowRegistry.h"

#include <QHash>
#include <QSet>
#include <QStringList>
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

QVector<WorkflowOperationState> WorkflowOperationService::State() const
{
    QVector<WorkflowOperationState> states;
    QStringList workflowIds = m_Operations.keys();
    workflowIds.sort();
    for (const auto& workflowId : workflowIds)
    {
        WorkflowOperationState state;
        state.WorkflowId = workflowId;
        state.SelectedOperationId = m_SelectedOperationIds.value(workflowId);
        for (const auto& operation : m_Operations.value(workflowId))
        {
            const QString valueKey = ParameterValueKey(workflowId,
                                                       operation.Id);
            state.ParameterValuesByOperationId.insert(
                operation.Id,
                m_ParameterValues.value(valueKey));
        }
        states.push_back(state);
    }

    return states;
}

bool WorkflowOperationService::ApplyState(
    const QVector<WorkflowOperationState>& states,
    QString* message)
{
    for (const auto& state : states)
    {
        const QString workflowId = state.WorkflowId.trimmed();
        if (!state.SelectedOperationId.trimmed().isEmpty() &&
            !SelectOperation(workflowId,
                             state.SelectedOperationId,
                             message))
        {
            return false;
        }

        QStringList operationIds = state.ParameterValuesByOperationId.keys();
        operationIds.sort();
        for (const auto& operationId : operationIds)
        {
            const QVariantMap values =
                state.ParameterValuesByOperationId.value(operationId);
            QStringList parameterIds = values.keys();
            parameterIds.sort();
            for (const auto& parameterId : parameterIds)
            {
                if (!SetParameterValue(workflowId,
                                       operationId,
                                       parameterId,
                                       values.value(parameterId),
                                       message))
                {
                    return false;
                }
            }
        }
    }

    SetMessage(message, QString());
    return true;
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

            if (parameter.Type ==
                WorkflowOperationParameterValueType::Option)
            {
                QSet<QString> optionIds;
                for (auto& option : parameter.Options)
                {
                    option.Id = option.Id.trimmed();
                    option.Title = option.Title.trimmed();
                    if (option.Id.isEmpty() || option.Title.isEmpty())
                    {
                        SetMessage(
                            message,
                            QStringLiteral(
                                "Workflow operation parameter option id and title are required."));
                        return false;
                    }

                    if (optionIds.contains(option.Id))
                    {
                        SetMessage(
                            message,
                            QStringLiteral(
                                "Duplicate workflow operation parameter option id."));
                        return false;
                    }

                    optionIds.insert(option.Id);
                }

                if (optionIds.isEmpty())
                {
                    SetMessage(
                        message,
                        QStringLiteral(
                            "Workflow operation option parameters require options."));
                    return false;
                }
            }
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

void WorkflowOperationService::ReplaceStateWith(
    const WorkflowOperationService& other)
{
    const auto previousSelections = m_SelectedOperationIds;
    const auto previousParameterValues = m_ParameterValues;

    m_SelectedOperationIds = other.m_SelectedOperationIds;
    m_ParameterValues = other.m_ParameterValues;

    for (const auto& workflowId : m_Operations.keys())
    {
        for (const auto& operation : m_Operations.value(workflowId))
        {
            const QString valueKey = ParameterValueKey(workflowId,
                                                       operation.Id);
            if (!m_ParameterValues.contains(valueKey))
            {
                QVariantMap values;
                for (const auto& parameter : operation.Parameters)
                    values.insert(parameter.Id,
                                  DefaultValueForParameter(parameter));
                m_ParameterValues.insert(valueKey, values);
            }
        }

        if (!m_SelectedOperationIds.contains(workflowId) &&
            !m_Operations.value(workflowId).isEmpty())
        {
            m_SelectedOperationIds.insert(workflowId,
                                          m_Operations.value(workflowId)
                                              .front()
                                              .Id);
        }
    }

    for (auto it = m_SelectedOperationIds.cbegin();
         it != m_SelectedOperationIds.cend();
         ++it)
    {
        if (previousSelections.value(it.key()) != it.value())
            emit SelectedOperationChanged(it.key(), it.value());
    }

    for (auto it = m_ParameterValues.cbegin();
         it != m_ParameterValues.cend();
         ++it)
    {
        const QVariantMap previousValues =
            previousParameterValues.value(it.key());
        const QVariantMap nextValues = it.value();
        QStringList parameterIds = nextValues.keys();
        parameterIds.sort();
        const QStringList keyParts = it.key().split(QStringLiteral("/"));
        if (keyParts.size() != 2)
            continue;

        for (const auto& parameterId : parameterIds)
        {
            if (previousValues.value(parameterId) ==
                nextValues.value(parameterId))
            {
                continue;
            }

            emit ParameterValueChanged(keyParts.at(0),
                                       keyParts.at(1),
                                       parameterId,
                                       nextValues.value(parameterId));
        }
    }
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

    if (!ContainsOptionValue(normalizedWorkflowId,
                             normalizedOperationId,
                             normalizedParameterId,
                             value.toString()))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Workflow operation parameter option was not found."));
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

bool WorkflowOperationService::ContainsOptionValue(
    const QString& workflowId,
    const QString& operationId,
    const QString& parameterId,
    const QString& value) const
{
    const auto operations = OperationsForWorkflow(workflowId);
    for (const auto& operation : operations)
    {
        if (operation.Id != operationId)
            continue;

        for (const auto& parameter : operation.Parameters)
        {
            if (parameter.Id != parameterId)
                continue;
            if (parameter.Type != WorkflowOperationParameterValueType::Option)
                return true;

            for (const auto& option : parameter.Options)
            {
                if (option.Id == value.trimmed())
                    return true;
            }
            return false;
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
    case WorkflowOperationParameterValueType::Option:
        if (!parameter.Options.isEmpty())
            return parameter.Options.front().Id;
        return QString();
    }

    return {};
}

QString WorkflowOperationService::ParameterValueKey(const QString& workflowId,
                                                    const QString& operationId)
{
    return QStringLiteral("%1/%2").arg(workflowId, operationId);
}

} // namespace xq::core
