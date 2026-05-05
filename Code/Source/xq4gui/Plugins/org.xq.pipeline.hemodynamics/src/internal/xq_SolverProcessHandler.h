#ifndef XQ_SOLVERPROCESSHANDLER_H
#define XQ_SOLVERPROCESSHANDLER_H

#include <QObject>
#include <QProcess>

class xq_SolverProcessHandler : public QObject
{
  Q_OBJECT

public:
  explicit xq_SolverProcessHandler(QObject* parent = nullptr);
  ~xq_SolverProcessHandler() override;

  void SetSolverPath(const QString& path);
  void SetMpiPath(const QString& path);
  void SetNumProcessors(int numProcs);

  void StartSolver(const QString& workDir);
  void StopSolver();
  bool IsRunning() const;

signals:
  void solverStarted();
  void solverFinished(int exitCode);
  void solverError(const QString& errorString);
  void progressUpdate(int percent);
  void outputReceived(const QString& text);

private slots:
  void OnReadyReadStdOut();
  void OnReadyReadStdErr();
  void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void OnProcessError(QProcess::ProcessError error);

private:
  QProcess* m_Process;
  QString m_SolverPath;
  QString m_MpiPath;
  int m_NumProcs;
  bool m_UseMpi;
};

#endif // XQ_SOLVERPROCESSHANDLER_H
