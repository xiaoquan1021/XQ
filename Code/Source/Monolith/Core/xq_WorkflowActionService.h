#ifndef XQ_WORKFLOWACTIONSERVICE_H
#define XQ_WORKFLOWACTIONSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class TaskRunner;
class WorkflowContextService;

class WorkflowActionService : public QObject
{
    Q_OBJECT

public:
    WorkflowActionService(WorkflowContextService& workflowContext,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);

    bool RequestActiveWorkflowAction(QString* message = nullptr) const;
    bool RunActiveWorkflowAction(QString* message = nullptr);

private:
    static void SetMessage(QString* message, const QString& value);

    WorkflowContextService& m_WorkflowContext;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_WORKFLOWACTIONSERVICE_H
