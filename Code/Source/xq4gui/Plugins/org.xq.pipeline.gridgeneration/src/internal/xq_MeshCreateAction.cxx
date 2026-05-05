#include "xq_MeshCreateAction.h"
#include <xq_MeshPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <QMessageBox>

xq_MeshCreateAction::xq_MeshCreateAction() {}
xq_MeshCreateAction::~xq_MeshCreateAction() {}

void xq_MeshCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_MeshCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (selectedNodes.isEmpty() || m_DataStorage.IsNull()) return;

  // The selected node could be a folder or a model node.
  // If it's a model node, use it directly; otherwise find a model.
  mitk::DataNode::Pointer modelNode;
  auto* node = selectedNodes[0].GetPointer();
  if (node && node->GetData() &&
      std::string(node->GetData()->GetNameOfClass()) == "xq_Model")
  {
    modelNode = selectedNodes[0];
  }
  else
  {
    auto modelNodes = xq::pipeline::GetNodesByStage(m_DataStorage, xq::pipeline::Stage::Model);
    if (!modelNodes.empty())
      modelNode = modelNodes[0];
  }

  if (modelNode.IsNull())
  {
    QMessageBox::information(nullptr, "Create Mesh",
      "No model found. Please create a model first.");
    return;
  }

  xq_MeshGenerationRequest req;
  req.globalEdgeSize = 0.5;

  auto result = xq_MeshPipelineService::CreateVolumeMesh(
      m_DataStorage, modelNode, req);
  if (!result.ok)
  {
    std::string msg = "Mesh creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Mesh", QString::fromStdString(msg));
  }
}
