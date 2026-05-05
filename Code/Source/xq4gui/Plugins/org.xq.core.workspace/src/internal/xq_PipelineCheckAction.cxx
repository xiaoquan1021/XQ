#include "xq_PipelineCheckAction.h"

#include <xq_PipelineConsistency.h>

#include <QMessageBox>
#include <QString>

xq_PipelineCheckAction::xq_PipelineCheckAction()
{
}

xq_PipelineCheckAction::~xq_PipelineCheckAction()
{
}

void xq_PipelineCheckAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_PipelineCheckAction::Run(const QList<mitk::DataNode::Pointer>& /*selectedNodes*/)
{
  if (m_DataStorage.IsNull())
  {
    QMessageBox::warning(nullptr, "Pipeline Check",
                         "No DataStorage available.");
    return;
  }

  std::vector<xq_PipelineIssue> issues = CheckDataStorage(m_DataStorage);

  if (issues.empty())
  {
    QMessageBox::information(nullptr, "Pipeline Consistency",
                             "No pipeline consistency problems found.");
    return;
  }

  // Group issues by severity
  QStringList errorLines;
  QStringList warningLines;
  QStringList infoLines;

  for (const auto& issue : issues)
  {
    QString line;
    if (issue.nodeName.empty())
    {
      line = QString::fromStdString(issue.message);
    }
    else
    {
      line = QString("[%1] %2")
                 .arg(QString::fromStdString(issue.nodeName))
                 .arg(QString::fromStdString(issue.message));
    }

    if (!issue.suggestedFix.empty())
    {
      line += QString("\n       Fix: %1").arg(QString::fromStdString(issue.suggestedFix));
    }

    switch (issue.severity)
    {
      case xq_PipelineIssue::Severity::Error:
        errorLines << line;
        break;
      case xq_PipelineIssue::Severity::Warning:
        warningLines << line;
        break;
      case xq_PipelineIssue::Severity::Info:
        infoLines << line;
        break;
    }
  }

  // Build the message box text
  QString text;
  if (!errorLines.isEmpty())
  {
    text += QString("ERRORS (%1):\n").arg(errorLines.size());
    for (const auto& line : errorLines)
    {
      text += "  • " + line + "\n";
    }
    text += "\n";
  }
  if (!warningLines.isEmpty())
  {
    text += QString("WARNINGS (%1):\n").arg(warningLines.size());
    for (const auto& line : warningLines)
    {
      text += "  • " + line + "\n";
    }
    text += "\n";
  }
  if (!infoLines.isEmpty())
  {
    text += QString("INFO (%1):\n").arg(infoLines.size());
    for (const auto& line : infoLines)
    {
      text += "  • " + line + "\n";
    }
  }

  // Choose icon based on worst severity
  QMessageBox::Icon icon = QMessageBox::Information;
  if (!errorLines.isEmpty())
  {
    icon = QMessageBox::Critical;
  }
  else if (!warningLines.isEmpty())
  {
    icon = QMessageBox::Warning;
  }

  QMessageBox msgBox(icon, "Pipeline Consistency Check", text, QMessageBox::Ok);
  msgBox.exec();
}
