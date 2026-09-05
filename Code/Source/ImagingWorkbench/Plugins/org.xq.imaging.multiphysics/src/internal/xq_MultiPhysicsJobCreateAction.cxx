#include "xq_MultiPhysicsJobCreateAction.h"

#include <xq_MitkMultiPhysicsJob.h>
#include <xq_MultiPhysicsJob.h>
#include <xq_PipelineDataUtils.h>

#include <QMessageBox>

xq_MultiPhysicsJobCreateAction::xq_MultiPhysicsJobCreateAction() = default;
xq_MultiPhysicsJobCreateAction::~xq_MultiPhysicsJobCreateAction() = default;

void xq_MultiPhysicsJobCreateAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_MultiPhysicsJobCreateAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (m_DataStorage.IsNull())
    return;

  mitk::DataNode::Pointer meshNode;
  for (const auto& node : selectedNodes)
  {
    if (node.IsNull() || !node->GetData())
      continue;

    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::VolumeMesh) ||
        std::string(node->GetData()->GetNameOfClass()) == "xq_MitkGrid")
    {
      meshNode = node;
      break;
    }
  }

  if (meshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create MultiPhysics Job",
      "No supported domain mesh is selected. Select a volume mesh node first.");
    return;
  }

  auto modelNode = xq::pipeline::ResolveUpstreamNode(
    m_DataStorage, meshNode.GetPointer(),
    xq::pipeline::kSourceModelProperty,
    xq::pipeline::Stage::Model);
  if (modelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Create MultiPhysics Job",
      "The selected mesh does not have a unique upstream model. Recreate or repair mesh metadata before creating a MultiPhysics job.");
    return;
  }

  const std::string jobName = meshNode->GetName() + "_multiphysics";
  auto job = std::make_unique<xq_MultiPhysicsJob>();
  job->SetJobName(jobName);
  job->SetTimeStepSize(0.001);
  job->SetNumTimeSteps(100);
  job->SetProperty("status", "created");
  job->SetProperty("process_count", "1");
  job->SetProperty("solver_paths", "not_configured");
  job->SetProperty("source_model", modelNode->GetName());
  job->SetProperty("source_mesh", meshNode->GetName());
  job->SetProperty("diagnostic",
    "MultiPhysics job context is created and restorable. Native solver execution is unavailable and disabled.");

  xq_MultiPhysicsDomain domain;
  domain.name = "fluid";
  domain.type = xq_MultiPhysicsDomainType::Fluid;
  domain.material.density = 1.06;
  domain.material.viscosity = 0.04;
  domain.properties["mesh"] = meshNode->GetName();
  domain.properties["mesh_format"] = "xq_MitkGrid";
  job->AddDomain(domain);

  xq_MultiPhysicsEquation equation;
  equation.name = "fluid";
  equation.type = xq_MultiPhysicsEquationType::Fluid;
  equation.domainNames.push_back(domain.name);
  equation.properties["status"] = "created";
  job->AddEquation(equation);

  auto mpData = xq_MitkMultiPhysicsJob::New();
  mpData->SetJob(std::move(job), 0);
  mpData->SetStatus("created");

  auto mpNode = mitk::DataNode::New();
  mpNode->SetData(mpData);
  mpNode->SetName(jobName);
  xq::pipeline::MarkNode(mpNode, xq::pipeline::Stage::MultiPhysics);
  xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kAlgorithmProperty, "multiphysics_job");
  xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kSourceModelProperty, modelNode->GetName());
  xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kSourceMeshProperty, meshNode->GetName());
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.domains", "fluid:" + meshNode->GetName());
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.equations", "fluid");
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.bc", "not_configured");
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.params", "time_step_size=0.001;num_time_steps=100");
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.solver_paths", "not_configured");
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.status", "created");
  mpNode->SetIntProperty("xq.multiphysics.process_count", 1);
  xq::pipeline::SetStringProperty(mpNode, "xq.multiphysics.capability.diagnostic",
    "Domain/equation job context and XML roundtrip are available; native solver execution remains disabled.");

  auto mpFolder = xq::pipeline::FindCategoryFolder(
    m_DataStorage, xq::pipeline::Stage::MultiPhysics, meshNode.GetPointer());
  if (mpFolder.IsNotNull())
    m_DataStorage->Add(mpNode, mpFolder);
  else
    m_DataStorage->Add(mpNode, meshNode);
}
