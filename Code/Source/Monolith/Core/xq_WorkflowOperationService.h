#ifndef XQ_WORKFLOWOPERATIONSERVICE_H
#define XQ_WORKFLOWOPERATIONSERVICE_H

#include <QHash>
#include <QObject>
#include <QString>
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

class WorkflowOperationService : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowOperationService(QObject* parent = nullptr);

    QVector<WorkflowOperationDescriptor> OperationsForWorkflow(
        const QString& workflowId) const;
    QString SelectedOperationId(const QString& workflowId) const;

    bool RegisterOperations(
        const QString& workflowId,
        const QVector<WorkflowOperationDescriptor>& operations,
        QString* message = nullptr);
    bool SelectOperation(const QString& workflowId,
                         const QString& operationId,
                         QString* message = nullptr);

signals:
    void SelectedOperationChanged(const QString& workflowId,
                                  const QString& operationId);

private:
    static void SetMessage(QString* message, const QString& value);

    bool ContainsOperation(const QString& workflowId,
                           const QString& operationId) const;

    QHash<QString, QVector<WorkflowOperationDescriptor>> m_Operations;
    QHash<QString, QString> m_SelectedOperationIds;
};

} // namespace xq::core

#endif // XQ_WORKFLOWOPERATIONSERVICE_H
