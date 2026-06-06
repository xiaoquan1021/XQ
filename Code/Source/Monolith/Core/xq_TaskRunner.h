#ifndef XQ_TASKRUNNER_H
#define XQ_TASKRUNNER_H

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace xq::core
{

struct TaskRecord
{
    QString Name;
    bool Succeeded = false;
    QString Message;
};

class TaskRunner : public QObject
{
    Q_OBJECT

public:
    using TaskWork = std::function<bool(QString* message)>;

    explicit TaskRunner(QObject* parent = nullptr);

    QVector<TaskRecord> History() const;
    bool RunBlocking(const QString& taskName,
                     const TaskWork& work,
                     QString* errorMessage = nullptr);

signals:
    void TaskStarted(const QString& taskName);
    void TaskFinished(const QString& taskName,
                      bool succeeded,
                      const QString& message);

private:
    static void SetError(QString* errorMessage, const QString& message);

    QVector<TaskRecord> m_History;
};

} // namespace xq::core

#endif // XQ_TASKRUNNER_H
