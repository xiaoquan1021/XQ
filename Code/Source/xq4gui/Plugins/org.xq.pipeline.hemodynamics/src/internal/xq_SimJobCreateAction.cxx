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

  // Check if the selected node is a mesh (xq_MitkGrid)
  if (!selectedNodes.isEmpty())
  {
    auto* node = selectedNodes[0].GetPointer();
    if (node && node->GetData() &&
        std::string(node->GetData()->GetNameOfClass()) == "xq_MitkGrid")
    {
      meshNode = selectedNodes[0];
    }
  }

  // Fallback: find first VolumeMesh stage node
  if (meshNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::VolumeMesh);
    if (!nodes.empty())
      meshNode = nodes[0];
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
