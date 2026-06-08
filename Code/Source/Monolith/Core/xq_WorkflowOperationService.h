#ifndef XQ_WORKFLOWOPERATIONSERVICE_H
#define XQ_WORKFLOWOPERATIONSERVICE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

namespace xq::core
{

enum class WorkflowOperationParameterValueType
{
    NumericScalar,
    IntegerScalar,
    IntegerPointList
};

struct WorkflowOperationParameterDescriptor
{
    QString Id;
    QString Title;
    WorkflowOperationParameterValueType Type =
        WorkflowOperationParameterValueType::NumericScalar;
    bool Required = true;
};

struct WorkflowOperationDescriptor
{
    QString Id;
    QString Title;
    QVector<WorkflowOperationParameterDescriptor> Parameters;
};

struct WorkflowOperationState
{
    QString WorkflowId;
    QString SelectedOperationId;
    QHash<QString, QVariantMap> ParameterValuesByOperationId;
};

class WorkflowOperationService : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowOperationService(QObject* parent = nullptr);

    QVector<WorkflowOperationDescriptor> OperationsForWorkflow(
        const QString& workflowId) const;
    QVariantMap ParameterValues(const QString& workflowId,
                                const QString& operationId) const;
    QString SelectedOperationId(const QString& workflowId) const;
    QVector<WorkflowOperationState> State() const;

    bool ApplyState(const QVector<WorkflowOperationState>& states,
                    QString* message = nullptr);
    bool RegisterOperations(
        const QString& workflowId,
        const QVector<WorkflowOperationDescriptor>& operations,
        QString* message = nullptr);
    void ReplaceStateWith(const WorkflowOperationService& other);
    bool SelectOperation(const QString& workflowId,
                         const QString& operationId,
                         QString* message = nullptr);
    bool SetParameterValue(const QString& workflowId,
                           const QString& operationId,
                           const QString& parameterId,
                           const QVariant& value,
                           QString* message = nullptr);

signals:
    void SelectedOperationChanged(const QString& workflowId,
                                  const QString& operationId);
    void ParameterValueChanged(const QString& workflowId,
                               const QString& operationId,
                               const QString& parameterId,
                               const QVariant& value);

private:
    static void SetMessage(QString* message, const QString& value);

    bool ContainsOperation(const QString& workflowId,
                           const QString& operationId) const;
    bool ContainsParameter(const QString& workflowId,
                           const QString& operationId,
                           const QString& parameterId) const;
    static QVariant DefaultValueForParameter(
        const WorkflowOperationParameterDescriptor& parameter);
    static QString ParameterValueKey(const QString& workflowId,
                                     const QString& operationId);

    QHash<QString, QVector<WorkflowOperationDescriptor>> m_Operations;
    QHash<QString, QVariantMap> m_ParameterValues;
    QHash<QString, QString> m_SelectedOperationIds;
};

} // namespace xq::core

#endif // XQ_WORKFLOWOPERATIONSERVICE_H
