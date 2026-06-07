#ifndef XQ_WORKFLOWACTIONSERVICE_H
#define XQ_WORKFLOWACTIONSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class WorkflowContextService;

class WorkflowActionService : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowActionService(WorkflowContextService& workflowContext,
                                   QObject* parent = nullptr);

    bool RequestActiveWorkflowAction(QString* message = nullptr) const;

private:
    static void SetMessage(QString* message, const QString& value);

    WorkflowContextService& m_WorkflowContext;
};

} // namespace xq::core

#endif // XQ_WORKFLOWACTIONSERVICE_H
