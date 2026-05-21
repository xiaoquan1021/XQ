#include "xq_ResultImportAction.h"
#include <xq_ResultImport.h>
#include <xq_PipelineDataUtils.h>
#include <xq_MitkSolverJob.h>
#include <xq_SimulationFolder.h>
#include <QMessageBox>
#include <QFileDialog>

xq_ResultImportAction::xq_ResultImportAction() {}
xq_ResultImportAction::~xq_ResultImportAction() {}

void xq_ResultImportAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_ResultImportAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull()) return;

  QString selectedFilePath = QFileDialog::getOpenFileName(nullptr,
      "Import Result File", QString(),
      "VTK Result Files (*.vtu *.vtp);;All Files (*)");
  if (selectedFilePath.isEmpty()) return;

  xq_ResultImportEntry entry;
  entry.filePath = selectedFilePath.toStdString();
  entry.nodeName = "result";
  entry.simulationName = "";

  // Bind simulation name from the selected node.
  // If the user invoked "Import Result" on a SolverJob node, tag the
  // imported result with that job's name so the source simulation is
  // recorded.  If invoked on a SimulationFolder, leave it empty.
  for (const auto& node : selectedNodes)
  {
      if (node.IsNotNull() && node->GetData())
      {
          if (dynamic_cast<xq_MitkSolverJob*>(node->GetData()))
          {
              entry.simulationName = node->GetName();
              break;
          }
          if (dynamic_cast<xq_SimulationFolder*>(node->GetData()))
          {
              entry.simulationName = "";
              break;
          }
      }
  }

  auto result = xq_ResultImport::Import(m_DataStorage, entry);
  if (!result.ok)
  {
    std::string msg = "Result import failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d;
    QMessageBox::warning(nullptr, "Import Result File",
                         QString::fromStdString(msg));
    return;
  }

  std::string info = "Imported result with " +
      std::to_string(result.fieldNames.size()) + " field(s):";
  for (const auto& f : result.fieldNames)
    info += "\n" + f;
  QMessageBox::information(nullptr, "Import Result File",
                           QString::fromStdString(info));
}
