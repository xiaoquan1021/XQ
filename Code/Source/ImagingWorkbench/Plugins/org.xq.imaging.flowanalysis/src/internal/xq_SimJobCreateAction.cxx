#include "xq_SimJobCreateAction.h"
#include <xq_SimulationPrepPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <QMessageBox>

xq_SimJobCreateAction::xq_SimJobCreateAction() {}
xq_SimJobCreateAction::~xq_SimJobCreateAction() {}

void xq_SimJobCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_SimJobCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull()) return;

  mitk::DataNode::Pointer meshNode;

  // Check selected nodes for one explicit mesh (xq_MitkGrid)
  for (const auto& selectedNode : selectedNodes)
  {
    auto* node = selectedNode.GetPointer();
    if (node && node->GetData() &&
        (xq::pipeline::HasStage(node, xq::pipeline::Stage::VolumeMesh) ||
         std::string(node->GetData()->GetNameOfClass()) == "xq_MitkGrid"))
    {
      if (meshNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Create Simulation Job",
          "Multiple meshes are selected. Select one mesh node.");
        return;
      }
      meshNode = selectedNode;
    }
  }

  // Fallback only when there is exactly one VolumeMesh stage node
  if (meshNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::VolumeMesh);
    if (nodes.size() == 1)
      meshNode = nodes[0];
    else if (nodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Create Simulation Job",
        "Multiple meshes are available. Select the intended mesh node.");
      return;
    }
  }

  if (meshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create Simulation Job",
      "No mesh found. Please create a mesh first.");
    return;
  }

  // Resolve upstream model
  auto modelNode = xq::pipeline::ResolveUpstreamNode(
      m_DataStorage, meshNode.GetPointer(),
      xq::pipeline::kSourceModelProperty,
      xq::pipeline::Stage::Model);

  if (modelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create Simulation Job",
      "No upstream model found for this mesh.");
    return;
  }

  xq_SimulationPrepRequest req;
  req.jobName = meshNode->GetName() + "_sim";
  req.deformableWall = false;

  auto result = xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
      m_DataStorage, modelNode, meshNode, req);
  if (!result.ok)
  {
    std::string msg = "Simulation job creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Simulation Job",
                         QString::fromStdString(msg));
  }
}
