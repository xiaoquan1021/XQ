#include "xq_SolverProcessHandler.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
QString ResolveExecutable(const QString& path)
{
  if (path.trimmed().isEmpty())
    return {};

  const QFileInfo info(path);
  if (info.isAbsolute() || path.contains('/'))
    return (info.exists() && info.isFile() && info.isExecutable()) ? info.absoluteFilePath() : QString();

  return QStandardPaths::findExecutable(path);
}
}

xq_SolverProcessHandler::xq_SolverProcessHandler(QObject* parent)
  : QObject(parent)
  , m_Process(nullptr)
  , m_MpiPath("mpiexec")
  , m_NumProcs(1)
  , m_UseMpi(false)
{
}

xq_SolverProcessHandler::~xq_SolverProcessHandler()
{
  StopSolver();
  delete m_Process;
}

void xq_SolverProcessHandler::SetSolverPath(const QString& path)
{
  m_SolverPath = path;
}

void xq_SolverProcessHandler::SetMpiPath(const QString& path)
{
  m_MpiPath = path;
}

void xq_SolverProcessHandler::SetNumProcessors(int numProcs)
{
  m_NumProcs = numProcs;
  m_UseMpi = (numProcs > 1);
}

bool xq_SolverProcessHandler::StartSolver(const QString& workDir)
{
  if (m_SolverPath.isEmpty())
  {
    emit solverError("Solver path is not set. Please configure it in Preferences.");
    return false;
  }

  if (IsRunning())
  {
    emit solverError("A solver process is already running.");
    return false;
  }

  const QString resolvedSolver = ResolveExecutable(m_SolverPath);
  if (resolvedSolver.isEmpty())
  {
    emit solverError("Configured solver path does not exist or is not executable.");
    return false;
  }

  QString resolvedMpi;
  if (m_UseMpi && m_NumProcs > 1)
  {
    resolvedMpi = ResolveExecutable(m_MpiPath);
    if (resolvedMpi.isEmpty())
    {
      emit solverError("Configured MPI path does not exist or is not executable.");
      return false;
    }
  }

  if (!m_Process)
  {
    m_Process = new QProcess(this);

    connect(m_Process, &QProcess::readyReadStandardOutput,
            this, &xq_SolverProcessHandler::OnReadyReadStdOut);
    connect(m_Process, &QProcess::readyReadStandardError,
            this, &xq_SolverProcessHandler::OnReadyReadStdErr);
    connect(m_Process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &xq_SolverProcessHandler::OnProcessFinished);
    connect(m_Process, &QProcess::errorOccurred,
            this, &xq_SolverProcessHandler::OnProcessError);
  }

  m_Process->setWorkingDirectory(workDir);

  QString program;
  QStringList args;

  if (m_UseMpi && m_NumProcs > 1)
  {
    program = resolvedMpi;
    args << "-np" << QString::number(m_NumProcs) << resolvedSolver;
  }
  else
  {
    program = resolvedSolver;
  }

  m_Process->start(program, args);
  if (!m_Process->waitForStarted(3000))
  {
    emit solverError("Solver process failed to start.");
    return false;
  }

  emit solverStarted();
  return true;
}

void xq_SolverProcessHandler::StopSolver()
{
  if (!m_Process || m_Process->state() == QProcess::NotRunning)
    return;

  m_Process->terminate();
  if (!m_Process->waitForFinished(3000))
    m_Process->kill();
}

bool xq_SolverProcessHandler::IsRunning() const
{
  return m_Process && m_Process->state() == QProcess::Running;
}

void xq_SolverProcessHandler::OnReadyReadStdOut()
{
  QByteArray data = m_Process->readAllStandardOutput();
  QString text = QString::fromLocal8Bit(data);
  emit outputReceived(text);

  // Parse for progress percentage (e.g. "Progress: 45%")
  static QRegularExpression progressRe("(\\d+)\\s*%");
  QRegularExpressionMatch match = progressRe.match(text);
  if (match.hasMatch())
  {
    int pct = match.captured(1).toInt();
    if (pct >= 0 && pct <= 100)
      emit progressUpdate(pct);
  }
}

void xq_SolverProcessHandler::OnReadyReadStdErr()
{
  QByteArray data = m_Process->readAllStandardError();
  QString text = QString::fromLocal8Bit(data);
  emit outputReceived(text);
}

void xq_SolverProcessHandler::OnProcessFinished(int exitCode,
                                                  QProcess::ExitStatus /*exitStatus*/)
{
  emit solverFinished(exitCode);
}

void xq_SolverProcessHandler::OnProcessError(QProcess::ProcessError error)
{
  QString errStr;
  switch (error)
  {
    case QProcess::FailedToStart:
      errStr = "Solver process failed to start. Check the solver path.";
      break;
    case QProcess::Crashed:
      errStr = "Solver process crashed.";
      break;
    case QProcess::Timedout:
      errStr = "Solver process timed out.";
      break;
    case QProcess::WriteError:
      errStr = "Write error communicating with solver process.";
      break;
    case QProcess::ReadError:
      errStr = "Read error communicating with solver process.";
      break;
    default:
      errStr = "Unknown solver process error.";
      break;
  }
  emit solverError(errStr);
}
