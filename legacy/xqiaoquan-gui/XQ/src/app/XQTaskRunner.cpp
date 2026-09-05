#include "app/XQTaskRunner.h"

#include <utility>

namespace xq {

XQTaskRunner::XQTaskRunner(QObject* parent)
    : QObject(parent)
{
    workerContext_.moveToThread(&thread_);
    thread_.setObjectName(QStringLiteral("xqTaskRunnerWorker"));
    thread_.start();
}

XQTaskRunner::~XQTaskRunner()
{
    thread_.quit();
    thread_.wait();
}

bool XQTaskRunner::busy() const
{
    return busy_;
}

bool XQTaskRunner::run(const QString& label,
                       std::function<std::shared_ptr<void>()> work,
                       std::function<void(std::shared_ptr<void>)> commit)
{
    if (busy_ || !work) {
        return false;
    }
    busy_ = true;
    emit taskStarted(label);

    // Double queued hop: GUI -> worker (run work) -> GUI (commit + finish).
    // Functor-based invokeMethod needs no qRegisterMetaType (Qt 6).
    QMetaObject::invokeMethod(
        &workerContext_,
        [this, label, work = std::move(work), commit = std::move(commit)]() {
            std::shared_ptr<void> result = work();
            QMetaObject::invokeMethod(
                this,
                [this, label, commit, result]() {
                    if (commit) {
                        commit(result);
                    }
                    busy_ = false;
                    emit taskFinished(label);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    return true;
}

} // namespace xq
