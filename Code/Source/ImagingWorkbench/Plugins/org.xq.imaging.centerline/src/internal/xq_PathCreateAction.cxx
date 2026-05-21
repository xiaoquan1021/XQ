#include "xq_PathCreateAction.h"

#include <xq_PathPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <mitkImage.h>

#include <QMessageBox>

#include <vector>

xq_PathCreateAction::xq_PathCreateAction() {}
xq_PathCreateAction::~xq_PathCreateAction() {}

void xq_PathCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_PathCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull())
    return;

  mitk::DataNode::Pointer imageNode;
  for (const auto& node : selectedNodes)
  {
    if (node.IsNotNull() && dynamic_cast<mitk::Image*>(node->GetData()))
    {
      if (imageNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Create Path",
          "Multiple images are selected. Select one image node.");
        return;
      }
      imageNode = node;
    }
  }

  if (imageNode.IsNull())
  {
    std::vector<mitk::DataNode::Pointer> imageNodes;
    auto allNodes = m_DataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      mitk::DataNode::Pointer n = it->Value();
      if (n.IsNotNull() && dynamic_cast<mitk::Image*>(n->GetData()))
        imageNodes.push_back(n);
    }

    if (imageNodes.empty())
    {
      QMessageBox::information(nullptr, "Create Path",
        "No image found. Please import an image first.");
      return;
    }
    if (imageNodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Create Path",
        "Multiple images are available. Select the intended image node before creating a path.");
      return;
    }
    imageNode = imageNodes.front();
  }

  xq_PathPlanRequest req;
  req.pathName = "NewPath";
  req.imageNodeName = imageNode->GetName();

  auto result = xq_PathPipelineService::CreatePath(m_DataStorage, req);
  if (!result.ok)
  {
    std::string msg = "Path creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Path", QString::fromStdString(msg));
  }
}
