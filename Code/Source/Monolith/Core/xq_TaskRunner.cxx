#include "xq_TaskRunner.h"

namespace xq::core
{

TaskRunner::TaskRunner(QObject* parent)
    : QObject(parent)
{
}

QVector<TaskRecord> TaskRunner::History() const
{
    return m_History;
}

bool TaskRunner::RunBlocking(const QString& taskName,
                             const TaskWork& work,
                             QString* errorMessage)
{
    const QString normalizedName = taskName.trimmed();
    if (normalizedName.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Task name is required."));
        return false;
    }

    if (!work)
    {
        SetError(errorMessage, QStringLiteral("Task work is required."));
        return false;
    }

    emit TaskStarted(normalizedName);

    QString message;
    const bool succeeded = work(&message);

    TaskRecord record;
    record.Name = normalizedName;
    record.Succeeded = succeeded;
    record.Message = message;
    m_History.append(record);

    SetError(errorMessage, message);
    emit TaskFinished(record.Name, record.Succeeded, record.Message);

    return succeeded;
}

void TaskRunner::SetError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
