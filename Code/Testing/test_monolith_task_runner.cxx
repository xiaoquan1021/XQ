#include "Core/xq_ApplicationContext.h"
#include "Core/xq_TaskRunner.h"

#include <QCoreApplication>
#include <QObject>
#include <QString>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::TaskRunner runner;
    if (Expect(runner.History().isEmpty(),
               "new TaskRunner should start with empty history"))
        return 1;

    int startedSignals = 0;
    int finishedSignals = 0;
    QString lastStartedName;
    QString lastFinishedName;
    bool lastFinishedSuccess = false;
    QString lastFinishedMessage;

    QObject::connect(&runner,
                     &xq::core::TaskRunner::TaskStarted,
                     [&startedSignals,
                      &lastStartedName](const QString& taskName) {
                         ++startedSignals;
                         lastStartedName = taskName;
                     });
    QObject::connect(&runner,
                     &xq::core::TaskRunner::TaskFinished,
                     [&finishedSignals,
                      &lastFinishedName,
                      &lastFinishedSuccess,
                      &lastFinishedMessage](const QString& taskName,
                                            bool succeeded,
                                            const QString& message) {
                         ++finishedSignals;
                         lastFinishedName = taskName;
                         lastFinishedSuccess = succeeded;
                         lastFinishedMessage = message;
                     });

    QString errorMessage;
    const bool success = runner.RunBlocking(
        QStringLiteral("Import DICOM"),
        [](QString* message) {
            if (message)
                *message = QStringLiteral("Imported 12 images");
            return true;
        },
        &errorMessage);

    if (Expect(success, "successful task should return true"))
        return 1;
    if (Expect(startedSignals == 1 && finishedSignals == 1,
               "successful task should emit started and finished once"))
        return 1;
    if (Expect(lastStartedName == QStringLiteral("Import DICOM"),
               "TaskStarted should emit the normalized task name"))
        return 1;
    if (Expect(lastFinishedSuccess,
               "TaskFinished should report successful completion"))
        return 1;
    if (Expect(lastFinishedMessage == QStringLiteral("Imported 12 images"),
               "TaskFinished should report the task message"))
        return 1;
    if (Expect(runner.History().size() == 1,
               "successful task should be recorded in history"))
        return 1;
    if (Expect(runner.History().at(0).Name == QStringLiteral("Import DICOM"),
               "task history should preserve the task name"))
        return 1;
    if (Expect(runner.History().at(0).Succeeded,
               "task history should preserve success state"))
        return 1;

    const bool failure = runner.RunBlocking(
        QStringLiteral("Segment Vessel"),
        [](QString* message) {
            if (message)
                *message = QStringLiteral("No image selected");
            return false;
        },
        &errorMessage);

    if (Expect(!failure, "failed task should return false"))
        return 1;
    if (Expect(finishedSignals == 2,
               "failed task should still emit finished"))
        return 1;
    if (Expect(lastFinishedName == QStringLiteral("Segment Vessel"),
               "failed task should emit the task name"))
        return 1;
    if (Expect(!lastFinishedSuccess,
               "failed task should emit failed state"))
        return 1;
    if (Expect(lastFinishedMessage == QStringLiteral("No image selected"),
               "failed task should emit the failure message"))
        return 1;
    if (Expect(runner.History().size() == 2,
               "failed task should be recorded in history"))
        return 1;

    bool invoked = false;
    errorMessage.clear();
    const bool emptyName = runner.RunBlocking(
        QStringLiteral("   "),
        [&invoked](QString*) {
            invoked = true;
            return true;
        },
        &errorMessage);

    if (Expect(!emptyName, "empty task name should fail"))
        return 1;
    if (Expect(!invoked, "empty task name should not invoke work"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("name")),
               "empty task name should produce a useful error"))
        return 1;
    if (Expect(runner.History().size() == 2,
               "empty task name should not be recorded in history"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->Tasks() != nullptr,
               "ApplicationContext should expose TaskRunner"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().isEmpty(),
               "ApplicationContext TaskRunner should start with empty history"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
