#ifndef XQ_WORKFLOWSELECTIONSERVICE_H
#define XQ_WORKFLOWSELECTIONSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class WorkflowSelectionService : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowSelectionService(QObject* parent = nullptr);

    QString SelectedWorkflowId() const;

public slots:
    bool SelectWorkflow(const QString& workflowId);

signals:
    void WorkflowChanged(const QString& workflowId);

private:
    QString m_SelectedWorkflowId;
};

} // namespace xq::core

#endif // XQ_WORKFLOWSELECTIONSERVICE_H
