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
  auto isContourGroupNode = [](const mitk::DataNode::Pointer& node) {
    if (node.IsNull() || !node->GetData())
      return false;
    const std::string className = node->GetData()->GetNameOfClass();
    return xq::pipeline::HasStage(node, xq::pipeline::Stage::ContourGroup) ||
           className == "xq_ProfileGroup" ||
           className == "xq_ContourGroup";
  };

  // Check selected nodes for one explicit ContourGroup/ProfileGroup.
  for (const auto& selectedNode : selectedNodes)
  {
    if (isContourGroupNode(selectedNode))
    {
      if (contourGroupNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Create Model from Contours",
          "Multiple contour groups are selected. Select one contour group node.");
        return;
      }
      contourGroupNode = selectedNode;
    }
  }

  // Fallback only when there is exactly one ContourGroup stage node
  if (contourGroupNode.IsNull())
  {
    auto nodes = xq::pipeline::GetNodesByStage(m_DataStorage,
        xq::pipeline::Stage::ContourGroup);
    if (nodes.size() == 1)
      contourGroupNode = nodes[0];
    else if (nodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Create Model from Contours",
        "Multiple contour groups are available. Select the intended contour group node.");
      return;
    }
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
  req.sourceContourGroupNames = {contourGroupNode->GetName()};

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
