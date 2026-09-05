#include <app/XQTaskRunner.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <memory>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

// Runs the event loop until taskFinished fires (or a 30s watchdog trips).
bool waitForFinished(xq::XQTaskRunner* runner)
{
    QEventLoop loop;
    bool finished = false;
    QObject::connect(runner, &xq::XQTaskRunner::taskFinished, &loop,
                     [&loop, &finished](const QString&) {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit); // watchdog
    loop.exec();
    return finished;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QThread* guiThread = QThread::currentThread();

    xq::XQTaskRunner runner;

    // 1) work runs off the GUI thread; commit runs on it; busy sequences.
    {
        QThread* workThread = nullptr;
        QThread* commitThread = nullptr;
        bool busyDuringWork = false;
        int order = 0;
        int workOrder = 0;
        int commitOrder = 0;

        CHECK(!runner.busy());
        const bool started = runner.run(
            QStringLiteral("job-1"),
            [&workThread, &order, &workOrder]() -> std::shared_ptr<void> {
                workThread = QThread::currentThread();
                workOrder = ++order;
                return std::make_shared<int>(42);
            },
            [&commitThread, &order, &commitOrder, &busyDuringWork,
             &runner](std::shared_ptr<void> result) {
                commitThread = QThread::currentThread();
                commitOrder = ++order;
                busyDuringWork = runner.busy(); // still busy inside commit
                const int value = *std::static_pointer_cast<int>(result);
                if (value != 42) {
                    std::exit(2);
                }
            });
        CHECK(started);
        CHECK(runner.busy());

        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(!runner.busy());
        CHECK(workThread != nullptr && workThread != guiThread);
        CHECK(commitThread == guiThread);
        CHECK(workOrder == 1 && commitOrder == 2);
        CHECK(busyDuringWork);
    }

    // 2) second run while busy is refused and does not run its work.
    {
        bool secondRan = false;
        const bool first = runner.run(
            QStringLiteral("job-2"),
            []() -> std::shared_ptr<void> {
                QThread::msleep(200); // hold busy long enough to test refusal
                return nullptr;
            },
            [](std::shared_ptr<void>) {});
        CHECK(first);
        const bool second = runner.run(
            QStringLiteral("job-3"),
            [&secondRan]() -> std::shared_ptr<void> {
                secondRan = true;
                return nullptr;
            },
            [](std::shared_ptr<void>) {});
        CHECK(!second);

        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(!secondRan);
        CHECK(!runner.busy());
    }

    // 3) null-result work is fine; taskStarted fires with the label.
    {
        QString startedLabel;
        QObject::connect(&runner, &xq::XQTaskRunner::taskStarted, &runner,
                         [&startedLabel](const QString& label) {
            startedLabel = label;
        });
        const bool started = runner.run(
            QStringLiteral("job-4"),
            []() -> std::shared_ptr<void> { return nullptr; },
            [](std::shared_ptr<void> result) {
                if (result != nullptr) {
                    std::exit(3);
                }
            });
        CHECK(started);
        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(startedLabel == QStringLiteral("job-4"));
    }

    std::printf("OK: task runner threading + busy discipline\n");
    return 0;
}
