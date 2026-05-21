#include "xq_ContourGroupCreateAction.h"
#include <xq_SegmentationPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <xq_SegmentationUtils.h>
#include <QMessageBox>

xq_ContourGroupCreateAction::xq_ContourGroupCreateAction() {}
xq_ContourGroupCreateAction::~xq_ContourGroupCreateAction() {}

void xq_ContourGroupCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{ m_DataStorage = dataStorage; }

void xq_ContourGroupCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull()) return;

  mitk::DataNode::Pointer pathNode;
  for (const auto& node : selectedNodes)
  {
    if (node.IsNotNull() && xq::pipeline::IsPathNode(node))
    {
      if (pathNode.IsNotNull())
      {
        QMessageBox::warning(nullptr, "Create Contour Group",
          "Multiple paths are selected. Select one path node.");
        return;
      }
      pathNode = node;
    }
  }

  if (pathNode.IsNull())
  {
    auto pathNodes = xq::pipeline::GetNodesByStage(m_DataStorage, xq::pipeline::Stage::Path);
    if (pathNodes.empty())
    {
      QMessageBox::information(nullptr, "Create Contour Group",
        "No path found. Please create a path first.");
      return;
    }
    if (pathNodes.size() > 1)
    {
      QMessageBox::warning(nullptr, "Create Contour Group",
        "Multiple paths are available. Select the intended path node before creating a contour group.");
      return;
    }
    pathNode = pathNodes.front();
  }

  xq_CreateContourGroupRequest req;
  req.groupName = "NewContourGroup";
  req.pathName = pathNode->GetName();

  auto result = xq_SegmentationPipelineService::CreateContourGroup(m_DataStorage, req);
  if (!result.ok)
  {
    std::string msg = "Contour group creation failed.";
    for (const auto& d : result.diagnostics)
      msg += "\n" + d.message;
    QMessageBox::warning(nullptr, "Create Contour Group", QString::fromStdString(msg));
  }
  else if (result.node.IsNotNull())
  {
    auto report = xq_SegmentationUtils::BuildReadinessReport(result.profileGroup);
    result.node->SetBoolProperty("xq.contour.ready", report.loftReady && report.modelingReady);
    result.node->SetIntProperty("xq.contour.profile_count", report.profileCount);
    result.node->SetIntProperty("xq.contour.missing_count", report.missingCount);
    result.node->SetIntProperty("xq.contour.warning_count", static_cast<int>(report.warnings.size()));
    result.node->SetIntProperty("xq.contour.error_count", static_cast<int>(report.errors.size()));
  }
}
