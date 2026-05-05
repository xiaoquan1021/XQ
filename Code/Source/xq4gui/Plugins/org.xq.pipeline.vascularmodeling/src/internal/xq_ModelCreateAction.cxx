#include "xq_ModelCreateAction.h"
#include <xq_ModelPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <QMessageBox>

xq_ModelCreateAction::xq_ModelCreateAction() {}
xq_ModelCreateAction::~xq_ModelCreateAction() {}

void xq_ModelCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_ModelCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull()) return;

  mitk::DataNode::Pointer contourGroupNode;

  // Check if the selected node is a ContourGroup (xq_ProfileGroup)
  if (!selectedNodes.isEmpty())
  {
    auto* node = selectedNodes[0].GetPointer();
    if (node && node->GetData() &&
        std::string(node->GetData()->GetNameOfClass()) == "xq_ProfileGroup")
    {
      contourGroupNode = selectedNodes[0];
    }
  }

  // Fallback: find first ContourGroup stage node
  if (contourGroupNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::ContourGroup);
    if (!nodes.empty())
      contourGroupNode = nodes[0];
  }

  if (contourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create Model from Contours",
      "No contour group found. Please create a contour group first.");
    return;
  }

  xq_CreateModelRequest req;
  req.modelName = contourGroupNode->GetName() + "_model";
  req.modelType = "PolyData";
  req.numSampling = 48;

  std::string pathName;
  if (contourGroupNode->GetStringProperty(
        xq::pipeline::kSourcePathProperty, pathName))
  {
    req.pathFilter = pathName;
  }

  auto result = xq_ModelPipelineService::CreateModel(m_DataStorage, req);
  if (!result.ok)
  {
    std::string msg = "Model creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Model from Contours",
                         QString::fromStdString(msg));
  }
}
