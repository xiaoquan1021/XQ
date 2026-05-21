#include "xq_ROMSimulationView.h"

#include <xq_MitkROMJob.h>

#include <mitkDataNodeSelection.h>

#include <berryISelectionService.h>

#include <QLabel>
#include <QVBoxLayout>

const QString xq_ROMSimulationView::VIEW_ID = "org.xq.views.romsimulation";

xq_ROMSimulationView::xq_ROMSimulationView()
  : m_StatusLabel(nullptr)
  , m_CurrentROMJobNode(nullptr)
{
}

xq_ROMSimulationView::~xq_ROMSimulationView() = default;

void xq_ROMSimulationView::CreateQtPartControl(QWidget* parent)
{
  auto* layout = new QVBoxLayout(parent);

  m_StatusLabel = new QLabel(
      "ROM Simulation\n\n"
      "Reduced-Order Model (ROM) simulation workflow.\n\n"
      "Data model: xq_ROMSimulationJob (ready)\n"
      "XML export: xq_ROMSimJobXmlWriter (ready)\n"
      "Test: test_rom_job_contract (passing)\n\n"
      "GUI workflow steps (native scope):\n"
      "  1. Select model + centerline\n"
      "  2. Configure ROM mesh parameters\n"
      "  3. Set boundary conditions\n"
      "  4. Save/load ROM job metadata\n\n"
      "Runtime status: native ROM solver execution is unavailable and disabled. "
      "The view will not create fake ROM mesh, run, or result nodes.",
      parent);
  m_StatusLabel->setWordWrap(true);
  m_StatusLabel->setStyleSheet("QLabel { padding: 16px; }");
  layout->addWidget(m_StatusLabel);
  layout->addStretch();
  ClearROMState();
  BindCurrentDataManagerSelection();
}

void xq_ROMSimulationView::SetFocus()
{
  if (m_StatusLabel)
    m_StatusLabel->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_ROMSimulationView::BindCurrentDataManagerSelection()
{
  if (!GetSite() || GetSite()->GetWorkbenchWindow().IsNull())
    return;

  auto* selectionService = GetSite()->GetWorkbenchWindow()->GetSelectionService();
  if (!selectionService)
    return;

  berry::ISelection::ConstPointer selection =
      selectionService->GetSelection("org.xq.views.datamanager");
  mitk::DataNodeSelection::ConstPointer nodeSelection =
      selection.Cast<const mitk::DataNodeSelection>();
  if (nodeSelection.IsNull())
  {
    selection = selectionService->GetSelection();
    nodeSelection = selection.Cast<const mitk::DataNodeSelection>();
  }
  if (nodeSelection.IsNull())
    return;

  QList<mitk::DataNode::Pointer> nodes;
  const auto selectedNodes = nodeSelection->GetSelectedDataNodes();
  for (const auto& node : selectedNodes)
  {
    if (node.IsNotNull())
      nodes.push_back(node);
  }

  if (!nodes.empty())
    OnSelectionChanged(berry::IWorkbenchPart::Pointer(), nodes);
}

void xq_ROMSimulationView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  for (const auto& node : nodes)
  {
    if (node.IsNull())
      continue;

    auto* romJob = dynamic_cast<xq_MitkROMJob*>(node->GetData());
    if (!romJob)
      continue;

    m_CurrentROMJobNode = node;
    if (m_StatusLabel)
    {
      const auto status = romJob->GetStatus().empty()
        ? QStringLiteral("not started")
        : QString::fromStdString(romJob->GetStatus());
      QString details;
      if (auto* job = romJob->GetROMJob(0))
      {
        details += QString("Job name: %1\n").arg(QString::fromStdString(job->GetJobName()));
        details += QString("Model type: %1\n").arg(QString::fromStdString(job->GetModelType()));
        details += QString("Time step size: %1\n").arg(job->GetTimeStepSize());
        details += QString("Number of time steps: %1\n").arg(job->GetNumTimeSteps());
        details += QString("Solver tolerance: %1\n").arg(job->GetSolverTolerance());
      }

      QStringList missingSources;
      for (const auto& sourceKey : {
             "xq.source.model",
             "xq.source.mesh",
             "xq.source.path"})
      {
        std::string sourceName;
        if (!node->GetStringProperty(sourceKey, sourceName) || sourceName.empty())
          missingSources << QString::fromLatin1(sourceKey);
      }
      if (!missingSources.isEmpty())
      {
        details += QString("\nMissing source metadata: %1\n")
          .arg(missingSources.join(", "));
      }

      m_StatusLabel->setText(
        QString("ROM Simulation\n\nSelected ROM job: %1\nStatus: %2\n%3\n"
                "Double-click/open only binds this job. Native ROM solver execution is unavailable and disabled.")
          .arg(QString::fromStdString(node->GetName()))
          .arg(status)
          .arg(details));
    }
    EnableROMControls(true);
    return;
  }

  ClearROMState();
}

void xq_ROMSimulationView::ClearROMState()
{
  m_CurrentROMJobNode = nullptr;
  if (m_StatusLabel)
  {
    m_StatusLabel->setText(
      "ROM Simulation\n\n"
      "No ROM job selected.\n\n"
      "Select or double-click an xq_MitkROMJob node to bind it here. "
      "Opening this view does not export files or run a solver.");
  }
  EnableROMControls(false);
}

void xq_ROMSimulationView::EnableROMControls(bool)
{
  // Placeholder view has no action buttons yet. Keep this hook so selection
  // lifecycle behavior stays explicit without adding unauthorized UI files.
}
