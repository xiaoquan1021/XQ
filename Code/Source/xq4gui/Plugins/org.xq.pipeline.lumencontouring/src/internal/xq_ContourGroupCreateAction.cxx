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
  if (selectedNodes.isEmpty() || m_DataStorage.IsNull()) return;

  // Find a path node as source
  auto pathNodes = xq::pipeline::GetNodesByStage(m_DataStorage, xq::pipeline::Stage::Path);
  if (pathNodes.empty())
  {
    QMessageBox::information(nullptr, "Create Contour Group",
      "No path found. Please create a path first.");
    return;
  }

  xq_CreateContourGroupRequest req;
  req.groupName = "NewContourGroup";
  req.pathName = pathNodes[0]->GetName();

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
