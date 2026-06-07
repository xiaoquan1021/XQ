#ifndef XQ_WORKFLOWACTIONSERVICE_H
#define XQ_WORKFLOWACTIONSERVICE_H

#include "xq_WorkflowContextService.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace xq::core
{

class TaskRunner;

class WorkflowActionService : public QObject
{
    Q_OBJECT

public:
    using WorkflowActionHandler =
        std::function<bool(const WorkflowContextSnapshot& snapshot,
                           QString* message)>;

    WorkflowActionService(WorkflowContextService& workflowContext,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);

    bool RegisterHandler(const QString& workflowId,
                         WorkflowActionHandler handler,
                         QString* message = nullptr);
    bool HasHandler(const QString& workflowId) const;
    bool RequestActiveWorkflowAction(QString* message = nullptr) const;
    bool RunActiveWorkflowAction(QString* message = nullptr);

private:
    static void SetMessage(QString* message, const QString& value);

    WorkflowContextService& m_WorkflowContext;
    TaskRunner& m_TaskRunner;
    QHash<QString, WorkflowActionHandler> m_Handlers;
};

} // namespace xq::core

#endif // XQ_WORKFLOWACTIONSERVICE_H
