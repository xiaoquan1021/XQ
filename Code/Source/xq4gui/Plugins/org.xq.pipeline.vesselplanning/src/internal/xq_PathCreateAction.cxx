#include "xq_PathCreateAction.h"

#include <xq_PathPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <QMessageBox>

xq_PathCreateAction::xq_PathCreateAction() {}
xq_PathCreateAction::~xq_PathCreateAction() {}

void xq_PathCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_PathCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (selectedNodes.isEmpty() || m_DataStorage.IsNull())
    return;

  // Find an image node to use as source
  std::string imageName;
  auto allNodes = m_DataStorage->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    mitk::DataNode::Pointer n = it->Value();
    if (n->GetData() && std::string(n->GetData()->GetNameOfClass()) == "Image")
    {
      imageName = n->GetName();
      break;
    }
  }

  if (imageName.empty())
  {
    QMessageBox::information(nullptr, "Create Path",
      "No image found. Please import an image first.");
    return;
  }

  xq_PathPlanRequest req;
  req.pathName = "NewPath";
  req.imageNodeName = imageName;

  auto result = xq_PathPipelineService::CreatePath(m_DataStorage, req);
  if (!result.ok)
  {
    std::string msg = "Path creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Path", QString::fromStdString(msg));
  }
}
