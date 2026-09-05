#include "xq_MultiPhysicsView.h"

#include <xq_MitkMultiPhysicsJob.h>

#include <mitkDataNodeSelection.h>

#include <berryISelectionService.h>

#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

namespace
{

QString GetStringProperty(const mitk::DataNode* node, const char* key)
{
  std::string value;
  if (node && node->GetStringProperty(key, value) && !value.empty())
    return QString::fromStdString(value);
  return QString();
}

QString FirstNonEmptyProperty(const std::map<std::string, std::string>& properties,
                              std::initializer_list<const char*> keys)
{
  for (const auto* key : keys)
  {
    auto it = properties.find(key);
    if (it != properties.end() && !it->second.empty())
      return QString::fromStdString(it->second);
  }
  return QString();
}

} // namespace

const QString xq_MultiPhysicsView::VIEW_ID = "org.xq.views.multiphysics";

xq_MultiPhysicsView::xq_MultiPhysicsView()
  : m_StatusLabel(nullptr)
  , m_CurrentMultiPhysicsJobNode(nullptr)
{
}

xq_MultiPhysicsView::~xq_MultiPhysicsView() = default;

void xq_MultiPhysicsView::CreateQtPartControl(QWidget* parent)
{
  auto* layout = new QVBoxLayout(parent);

  m_StatusLabel = new QLabel(
      "Multi-Physics Simulation\n\n"
      "Coupled multi-physics simulation workflow.\n\n"
      "Data model: xq_MultiPhysicsJob (ready)\n"
      "XML export: xq_MultiPhysicsXmlWriter (ready)\n"
      "Test: test_multiphysics_job_xml (passing)\n\n"
      "GUI workflow steps (native scope):\n"
      "  1. Define computational domains\n"
      "  2. Assign material properties per domain\n"
      "  3. Configure equations and solver settings\n"
      "  4. Set boundary conditions with parameters\n"
      "  5. Save/load MultiPhysics XML metadata\n\n"
      "Runtime status: native coupled solver execution is unavailable and disabled. "
      "The view will not create fake run or result nodes.",
      parent);
  m_StatusLabel->setWordWrap(true);
  m_StatusLabel->setStyleSheet("QLabel { padding: 16px; }");
  layout->addWidget(m_StatusLabel);
  layout->addStretch();
  ClearMultiPhysicsState();
  BindCurrentDataManagerSelection();
}

void xq_MultiPhysicsView::SetFocus()
{
  if (m_StatusLabel)
    m_StatusLabel->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_MultiPhysicsView::BindCurrentDataManagerSelection()
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

void xq_MultiPhysicsView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  for (const auto& node : nodes)
  {
    if (node.IsNull())
      continue;

    auto* mpJob = dynamic_cast<xq_MitkMultiPhysicsJob*>(node->GetData());
    if (!mpJob)
      continue;

    m_CurrentMultiPhysicsJobNode = node;
    if (m_StatusLabel)
    {
      const auto status = mpJob->GetStatus().empty()
        ? QStringLiteral("not started")
        : QString::fromStdString(mpJob->GetStatus());
      QString details;
      const QString sourceModel = GetStringProperty(node, "xq.source.model");
      if (!sourceModel.isEmpty())
        details += QString("Source model: %1\n").arg(sourceModel);
      const QString sourceMesh = GetStringProperty(node, "xq.source.mesh");
      if (!sourceMesh.isEmpty())
        details += QString("Source mesh: %1\n").arg(sourceMesh);

      QStringList diagnostics;
      if (auto* job = mpJob->GetJob(0))
      {
        details += QString("Job name: %1\n").arg(QString::fromStdString(job->GetJobName()));
        details += QString("Time step size: %1\n").arg(job->GetTimeStepSize());
        details += QString("Number of time steps: %1\n").arg(job->GetNumTimeSteps());
        details += QString("Domains: %1\n").arg(job->GetDomains().size());
        details += QString("Equations: %1\n").arg(job->GetEquations().size());
        details += QString("Boundary conditions: %1\n").arg(job->GetBoundaryConditions().size());

        const auto validation = job->Validate();
        if (!validation.empty())
          diagnostics << QString::fromStdString(validation);

        if (job->GetDomains().empty())
        {
          diagnostics << "No computational domains are defined.";
        }
        else
        {
          details += "\nDomain summary:\n";
          for (const auto& domain : job->GetDomains())
          {
            const QString domainMesh =
              FirstNonEmptyProperty(domain.properties,
                                    {"xq.source.mesh", "source_mesh", "mesh", "mesh_name", "meshName"});
            details += QString("  - %1 (%2)")
              .arg(QString::fromStdString(domain.name))
              .arg(QString::fromStdString(std::string(::ToString(domain.type))));
            if (!domainMesh.isEmpty())
              details += QString(", mesh: %1").arg(domainMesh);
            details += "\n";

            if (domainMesh.isEmpty() && sourceMesh.isEmpty())
            {
              diagnostics << QString("Domain '%1' has no mesh/source metadata.")
                               .arg(QString::fromStdString(domain.name));
            }
          }
        }

        if (!job->GetEquations().empty())
        {
          details += "\nEquation summary:\n";
          for (const auto& equation : job->GetEquations())
          {
            details += QString("  - %1 (%2), solver: %3, tol: %4, max iter: %5\n")
              .arg(QString::fromStdString(equation.name))
              .arg(QString::fromStdString(std::string(::ToString(equation.type))))
              .arg(QString::fromStdString(equation.solverSettings.linearSolver))
              .arg(equation.solverSettings.tolerance)
              .arg(equation.solverSettings.maxIterations);
          }
        }

        if (!job->GetBoundaryConditions().empty())
        {
          details += "\nBoundary condition summary:\n";
          for (const auto& bc : job->GetBoundaryConditions())
          {
            details += QString("  - %1 on %2 (%3), params: %4\n")
              .arg(QString::fromStdString(bc.faceName))
              .arg(QString::fromStdString(bc.domainName))
              .arg(QString::fromStdString(std::string(::ToString(bc.bcType))))
              .arg(bc.parameters.size());
          }
        }

        const auto& properties = job->GetProperties();
        if (!properties.empty())
        {
          details += "\nJob properties:\n";
          for (const auto& [key, value] : properties)
            details += QString("  - %1: %2\n")
                         .arg(QString::fromStdString(key))
                         .arg(QString::fromStdString(value));
        }
      }
      else
      {
        diagnostics << "Job slot 0 is empty.";
      }

      if (sourceModel.isEmpty())
        diagnostics << "Missing node metadata: xq.source.model.";
      if (sourceMesh.isEmpty())
        diagnostics << "Missing node metadata: xq.source.mesh.";
      if (!diagnostics.isEmpty())
      {
        details += QString("\nDiagnostics:\n  - %1\n")
          .arg(diagnostics.join("\n  - "));
      }

      m_StatusLabel->setText(
        QString("Multi-Physics Simulation\n\nSelected MultiPhysics job: %1\nStatus: %2\n%3\n"
                "Double-click/open only binds this job. Native coupled solver execution is unavailable and disabled.")
          .arg(QString::fromStdString(node->GetName()))
          .arg(status)
          .arg(details));
    }
    EnableMultiPhysicsControls(true);
    return;
  }

  ClearMultiPhysicsState();
}

void xq_MultiPhysicsView::ClearMultiPhysicsState()
{
  m_CurrentMultiPhysicsJobNode = nullptr;
  if (m_StatusLabel)
  {
    m_StatusLabel->setText(
      "Multi-Physics Simulation\n\n"
      "No MultiPhysics job selected.\n\n"
      "Select or double-click an xq_MitkMultiPhysicsJob node to bind it here. "
      "Opening this view does not export XML or run a solver.");
  }
  EnableMultiPhysicsControls(false);
}

void xq_MultiPhysicsView::EnableMultiPhysicsControls(bool)
{
  // Placeholder view has no action buttons yet. Keep this hook so selection
  // lifecycle behavior stays explicit without adding unauthorized UI files.
}
