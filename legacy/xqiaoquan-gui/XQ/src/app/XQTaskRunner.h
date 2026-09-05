#ifndef XQ_APP_XQ_TASK_RUNNER_H
#define XQ_APP_XQ_TASK_RUNNER_H

#include <QObject>
#include <QString>
#include <QThread>

#include <functional>
#include <memory>

namespace xq {

// One resident worker thread for the app shell's heavy jobs (segmentation,
// meshing, solving, loading). Single-task discipline: while busy() a second
// run() is refused (returns false), which is the whole-app concurrency model --
// the busy matrix in XQMainWindow disables every input that could start work
// or mutate the scene.
//
// work() runs on the worker thread. HARD RULES for work closures: no scene
// writes, no widget access, no rendering, no QSettings -- pure computation over
// inputs the closure owns (copied values / shared_ptr keepalives). commit()
// runs back on the GUI thread and is the only place results touch the scene
// (via the command stack) or the UI.
class XQTaskRunner : public QObject {
    Q_OBJECT
public:
    explicit XQTaskRunner(QObject* parent = nullptr); // starts the worker thread
    ~XQTaskRunner() override;                          // quit() + wait()

    XQTaskRunner(const XQTaskRunner&) = delete;
    XQTaskRunner& operator=(const XQTaskRunner&) = delete;

    bool busy() const;

    // Queues work on the worker thread; commit(result) is delivered back to
    // this object's (GUI) thread after work returns. Returns false without
    // running anything while busy.
    bool run(const QString& label,
             std::function<std::shared_ptr<void>()> work,
             std::function<void(std::shared_ptr<void>)> commit);

signals:
    void taskStarted(const QString& label);
    void taskFinished(const QString& label);

private:
    QThread thread_;
    QObject workerContext_; // lives on thread_; the anchor invokeMethod targets
    bool busy_ = false;     // GUI-thread-only state (run/finish both run there)
};

} // namespace xq

#endif // XQ_APP_XQ_TASK_RUNNER_H
