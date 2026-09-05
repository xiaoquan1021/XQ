#include "xq_ROMJobCreateAction.h"

#include <xq_MitkROMJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>

#include <QMessageBox>

namespace
{
mitk::DataNode::Pointer FirstSelectedStage(
  const QList<mitk::DataNode::Pointer>& selectedNodes,
  xq::pipeline::Stage stage)
{
  for (const auto& node : selectedNodes)
  {
    if (node.IsNull() || !node->GetData())
      continue;

    const std::string className = node->GetData()->GetNameOfClass();
    const bool legacyMatch =
      (stage == xq::pipeline::Stage::Model && className == "xq_Model") ||
      (stage == xq::pipeline::Stage::VolumeMesh && className == "xq_MitkGrid");
    if (xq::pipeline::HasStage(node, stage) || legacyMatch)
      return node;
  }
  return nullptr;
}

std::string SingleSourcePathName(mitk::DataNode* modelNode)
{
  const auto pathList = xq::pipeline::GetStringProperty(
    modelNode, xq::pipeline::kSourcePathProperty);
  const auto pathNames = xq::pipeline::SplitSourceList(pathList);
  return pathNames.size() == 1 ? pathNames.front() : std::string();
}
}

xq_ROMJobCreateAction::xq_ROMJobCreateAction() = default;
xq_ROMJobCreateAction::~xq_ROMJobCreateAction() = default;

void xq_ROMJobCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_ROMJobCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull())
    return;

  auto meshNode = FirstSelectedStage(selectedNodes, xq::pipeline::Stage::VolumeMesh);
  auto modelNode = FirstSelectedStage(selectedNodes, xq::pipeline::Stage::Model);

  if (meshNode.IsNull() && modelNode.IsNotNull())
  {
    const auto meshNodes = xq::pipeline::GetNodesByStage(
      m_DataStorage, xq::pipeline::Stage::VolumeMesh);
    mitk::DataNode::Pointer matchedMesh;
    for (const auto& candidate : meshNodes)
    {
      const auto sourceModel = xq::pipeline::GetStringProperty(
        candidate.GetPointer(), xq::pipeline::kSourceModelProperty);
      if (sourceModel == modelNode->GetName())
      {
        if (matchedMesh.IsNotNull())
        {
          QMessageBox::warning(nullptr, "Create ROM Simulation Job",
            "Multiple meshes reference the selected model. Select the intended mesh node instead.");
          return;
        }
        matchedMesh = candidate;
      }
    }
    meshNode = matchedMesh;
  }

  if (meshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create ROM Simulation Job",
      "No volume mesh is selected or uniquely resolvable.");
    return;
  }

  if (modelNode.IsNull())
  {
    modelNode = xq::pipeline::ResolveUpstreamNode(
      m_DataStorage, meshNode.GetPointer(),
      xq::pipeline::kSourceModelProperty,
      xq::pipeline::Stage::Model);
  }

  if (modelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create ROM Simulation Job",
      "No upstream model is attached to the selected mesh.");
    return;
  }

  const auto pathName = SingleSourcePathName(modelNode.GetPointer());
  if (pathName.empty())
  {
    QMessageBox::warning(nullptr, "Create ROM Simulation Job",
      "The source path for this model is missing or ambiguous. ROM job creation requires one explicit path/centerline.");
    return;
  }

  auto pathNode = xq::pipeline::FindNodeByNameAndStage(
    m_DataStorage, pathName, xq::pipeline::Stage::Path);
  if (pathNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create ROM Simulation Job",
      "The source path named by the model metadata is not present in Data Manager.");
    return;
  }

  auto romData = xq_MitkROMJob::New();
  auto romJob = std::make_unique<xq_ROMJob>();
  const std::string jobName = meshNode->GetName() + "_rom";
  romJob->SetJobName(jobName);
  romJob->SetModelType("1D");
  romJob->SetProperty("model", modelNode->GetName());
  romJob->SetProperty("mesh", meshNode->GetName());
  romJob->SetProperty("path", pathNode->GetName());
  romJob->SetProperty("status", "created");
  romJob->SetProperty("diagnostic",
    "ROM job context is created and restorable. Native 1D mesh generation, solver execution, and result conversion are unavailable and disabled.");
  romData->SetROMJob(std::move(romJob), 0);
  romData->SetStatus("created");

  auto romNode = mitk::DataNode::New();
  romNode->SetData(romData);
  romNode->SetName(jobName);
  xq::pipeline::MarkNode(romNode, xq::pipeline::Stage::ROMSimulation);
  xq::pipeline::SetStringProperty(romNode, xq::pipeline::kAlgorithmProperty, "rom_job");
  xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourceModelProperty, modelNode->GetName());
  xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourceMeshProperty, meshNode->GetName());
  xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourcePathProperty, pathNode->GetName());
  xq::pipeline::SetStringProperty(romNode, "xq.rom.model_order", "1D");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.status", "created");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.bc", "not_configured");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.solver_params", "not_configured");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.run_params", "not_configured");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.result_conversion", "not_configured");
  xq::pipeline::SetStringProperty(romNode, "xq.rom.capability.diagnostic",
    "ROM job creation and restore are available; native 1D mesh generation, solver execution, and result conversion remain disabled.");

  auto romFolder = xq::pipeline::FindCategoryFolder(
    m_DataStorage, xq::pipeline::Stage::ROMSimulation, meshNode.GetPointer());
  if (romFolder.IsNotNull())
    m_DataStorage->Add(romNode, romFolder);
  else
    m_DataStorage->Add(romNode, meshNode);
}
