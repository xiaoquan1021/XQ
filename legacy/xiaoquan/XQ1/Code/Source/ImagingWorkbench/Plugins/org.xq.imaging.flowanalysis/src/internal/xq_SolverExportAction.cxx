#include "xq_SolverExportAction.h"
#include <xq_SimulationPrepPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <QMessageBox>
#include <QFileDialog>

xq_SolverExportAction::xq_SolverExportAction() {}
xq_SolverExportAction::~xq_SolverExportAction() {}

void xq_SolverExportAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_SolverExportAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull()) return;

  mitk::DataNode::Pointer simPrepNode;

  // Check if the selected node is a solver job (xq_MitkSolverJob)
  for (const auto& selectedNode : selectedNodes)
  {
    auto* node = selectedNode.GetPointer();
    if (node && node->GetData() &&
        (xq::pipeline::HasStage(node, xq::pipeline::Stage::SimulationPrep) ||
         std::string(node->GetData()->GetNameOfClass()) == "xq_MitkSolverJob"))
    {
      if (simPrepNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Export Solver Files",
          "Multiple simulation jobs are selected. Select one simulation job node.");
        return;
      }
      simPrepNode = selectedNode;
    }
  }

  // Fallback only when there is exactly one SimulationPrep node.
  if (simPrepNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::SimulationPrep);
    if (nodes.size() == 1)
      simPrepNode = nodes[0];
    else if (nodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Export Solver Files",
        "Multiple simulation jobs are available. Select the intended simulation job node.");
      return;
    }
  }

  if (simPrepNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Export Solver Files",
      "No simulation job found. Please create a simulation job first.");
    return;
  }

  QString exportDir = QFileDialog::getExistingDirectory(nullptr,
      "Select Export Directory", QString());
  if (exportDir.isEmpty()) return;

  xq_SimulationExportRequest req;
  req.outputDir = exportDir.toStdString();

  auto result = xq_SimulationPrepPipelineService::ExportForSolver(
      m_DataStorage, simPrepNode, req);
  if (!result.ok)
  {
    std::string msg = "Solver export failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Export Solver Files",
                         QString::fromStdString(msg));
    return;
  }

  QString warnings;
  for (const auto& d : result.diagnostics)
  {
    if (d.severity == xq::pipeline::Severity::Warning)
      warnings += "\n- " + QString::fromStdString(d.message);
  }

  // Keep the status aligned with ExportForSolver; do not label placeholder
  // waveform/outflow exports as cleanly runnable.
  xq::pipeline::SetStringProperty(
      simPrepNode, "xq.sim.status",
      warnings.isEmpty() ? "exported" : "exported_with_warnings");
  xq::pipeline::SetStringProperty(simPrepNode, "xq.sim.export_dir", req.outputDir);

  std::string info = "Export complete.\nFiles written:";
  for (const auto& f : result.filesWritten)
    info += "\n- " + f;
  if (!warnings.isEmpty())
  {
    info += "\n\nWarnings:";
    info += warnings.toStdString();
    info += "\n\nDo not run these files until the warnings are resolved.";
  }
  QMessageBox::information(nullptr, "Export Solver Files",
                           QString::fromStdString(info));
}
