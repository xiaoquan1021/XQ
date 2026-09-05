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
  if (m_DataStorage.IsNull()) return;

  // The selection could include a folder or one model node.
  mitk::DataNode::Pointer modelNode;
  for (const auto& selectedNode : selectedNodes)
  {
    auto* node = selectedNode.GetPointer();
    if (node && node->GetData() &&
        (xq::pipeline::HasStage(node, xq::pipeline::Stage::Model) ||
         std::string(node->GetData()->GetNameOfClass()) == "xq_Model"))
    {
      if (modelNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Create Mesh",
          "Multiple models are selected. Select one model node.");
        return;
      }
      modelNode = selectedNode;
    }
  }

  if (modelNode.IsNull())
  {
    auto modelNodes = xq::pipeline::GetNodesByStage(m_DataStorage, xq::pipeline::Stage::Model);
    if (modelNodes.size() == 1)
      modelNode = modelNodes[0];
    else if (modelNodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Create Mesh",
        "Multiple models are available. Select the intended model node.");
      return;
    }
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
