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
  if (!selectedNodes.isEmpty())
  {
    auto* node = selectedNodes[0].GetPointer();
    if (node && node->GetData() &&
        std::string(node->GetData()->GetNameOfClass()) == "xq_MitkSolverJob")
    {
      simPrepNode = selectedNodes[0];
    }
  }

  // Fallback: find first SimulationPrep stage node
  if (simPrepNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::SimulationPrep);
    if (!nodes.empty())
      simPrepNode = nodes[0];
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

  // Set status and export directory on the node
  xq::pipeline::SetStringProperty(simPrepNode, "xq.sim.status", "exported");
  xq::pipeline::SetStringProperty(simPrepNode, "xq.sim.export_dir", req.outputDir);

  std::string info = "Export complete.\nFiles written:";
  for (const auto& f : result.filesWritten)
    info += "\n- " + f;
  QMessageBox::information(nullptr, "Export Solver Files",
                           QString::fromStdString(info));
}
