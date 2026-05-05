#include "xq_SolverProcessHandler.h"

#include <QRegularExpression>

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

void xq_SolverProcessHandler::StartSolver(const QString& workDir)
{
  if (m_SolverPath.isEmpty())
  {
    emit solverError("Solver path is not set. Please configure it in Preferences.");
    return;
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
    program = m_MpiPath;
    args << "-np" << QString::number(m_NumProcs) << m_SolverPath;
  }
  else
  {
    program = m_SolverPath;
  }

  m_Process->start(program, args);
  emit solverStarted();
}

void xq_SolverProcessHandler::StopSolver()
{
  if (!m_Process || !m_Process->isOpen())
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
