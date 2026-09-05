#include "xq_PythonApiService.h"

#include <xq_MitkGrid.h>
#include <xq_MitkMultiPhysicsJob.h>
#include <xq_MitkROMJob.h>
#include <xq_MitkSolverJob.h>
#include <xq_Model.h>
#include <xq_MultiPhysicsDomain.h>
#include <xq_MultiPhysicsEquation.h>
#include <xq_MultiPhysicsJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>
#include <xq_ROMJob.h>
#include <xq_SolverJob.h>
#include <xq_VesselCenterline.h>
#include <xq_WorkspaceManager.h>

#include <mitkBaseData.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{

xq::pipeline::Stage StageFromName(std::string_view stageName)
{
    std::string stageLower(stageName);
    std::transform(stageLower.begin(), stageLower.end(), stageLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (stageLower == "image") return xq::pipeline::Stage::Image;
    if (stageLower == "imageprocessing" || stageLower == "image_processing")
        return xq::pipeline::Stage::ImageProcessing;
    if (stageLower == "path") return xq::pipeline::Stage::Path;
    if (stageLower == "centerline") return xq::pipeline::Stage::Centerline;
    if (stageLower == "contourgroup" || stageLower == "contour_group" ||
        stageLower == "segmentation")
        return xq::pipeline::Stage::ContourGroup;
    if (stageLower == "segmentation3d" || stageLower == "segmentation_3d" ||
        stageLower == "3d_segmentation")
        return xq::pipeline::Stage::Segmentation3D;
    if (stageLower == "model") return xq::pipeline::Stage::Model;
    if (stageLower == "volumemesh" || stageLower == "volume_mesh" ||
        stageLower == "mesh")
        return xq::pipeline::Stage::VolumeMesh;
    if (stageLower == "simulationprep" || stageLower == "simulation_prep" ||
        stageLower == "simulation")
        return xq::pipeline::Stage::SimulationPrep;
    if (stageLower == "romsimulation" || stageLower == "rom_simulation" ||
        stageLower == "rom")
        return xq::pipeline::Stage::ROMSimulation;
    if (stageLower == "multiphysics" || stageLower == "multi_physics")
        return xq::pipeline::Stage::MultiPhysics;
    if (stageLower == "result")
        return xq::pipeline::Stage::Result;
    return xq::pipeline::Stage::Unknown;
}

const char* SourcePropertyForStage(xq::pipeline::Stage stage)
{
    switch (stage)
    {
    case xq::pipeline::Stage::ImageProcessing:
        return xq::pipeline::kSourceImageProperty;
    case xq::pipeline::Stage::Path:
        return xq::pipeline::kSourcePathProperty;
    case xq::pipeline::Stage::Centerline:
        return xq::pipeline::kSourceCenterlineProperty;
    case xq::pipeline::Stage::ContourGroup:
        return xq::pipeline::kSourceContourGroupsProperty;
    case xq::pipeline::Stage::Segmentation3D:
        return xq::pipeline::kSourceSegmentationProperty;
    case xq::pipeline::Stage::Model:
        return xq::pipeline::kSourceModelProperty;
    case xq::pipeline::Stage::VolumeMesh:
        return xq::pipeline::kSourceMeshProperty;
    case xq::pipeline::Stage::Result:
        return xq::pipeline::kSourceSimulationJobProperty;
    default:
        return nullptr;
    }
}

} // namespace

xq_PythonApiService::xq_PythonApiService() = default;
xq_PythonApiService::xq_PythonApiService(mitk::DataStorage* ds)
    : m_DataStorage(ds) {}
xq_PythonApiService::~xq_PythonApiService() = default;

// --- Availability ---

bool xq_PythonApiService::IsAvailable() const { return m_Available; }

std::string xq_PythonApiService::GetAvailabilityDiagnostic() const
{
    return m_AvailabilityDiag;
}

// --- Core API ---

xq_ApiResult xq_PythonApiService::Version() const
{
    xq_ApiResult r;
    r.ok = true;
    r.value = "XQ 1.0.0 (Python API skeleton, pybind11 not linked)";
    return r;
}

xq_ApiResult xq_PythonApiService::ListNodes() const
{
    xq_ApiResult r;

    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }

    auto allNodes = m_DataStorage->GetAll();
    if (!allNodes)
    {
        r.ok = true; // empty
        return r;
    }

    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto node = it->Value();
        if (!node) continue;

        r.nodes.push_back(DescribeNode(node));
    }

    r.ok = true;
    return r;
}

xq_ApiResult xq_PythonApiService::FindNode(std::string_view name) const
{
    xq_ApiResult r;

    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }

    auto allNodes = m_DataStorage->GetAll();
    if (!allNodes)
    {
        r.diagnostic = "No nodes in DataStorage.";
        return r;
    }

    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto node = it->Value();
        if (!node) continue;

        if (node->GetName() == name)
        {
            r.nodes.push_back(DescribeNode(node));
            r.ok = true;
            return r;
        }
    }

    std::ostringstream oss;
    oss << "Node '" << name << "' not found.";
    r.diagnostic = oss.str();
    return r;
}

xq_ApiResult xq_PythonApiService::ResolveUpstream(
    std::string_view nodeName, std::string_view stageName) const
{
    xq_ApiResult r;

    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }

    // Find the source node by name first.
    auto findResult = FindNode(nodeName);
    if (!findResult.ok)
    {
        r.diagnostic = "Source node not found: " + std::string(nodeName);
        return r;
    }

    auto sourceNode = FindNodePointer(nodeName);

    if (!sourceNode)
    {
        r.diagnostic = "Source node pointer is null.";
        return r;
    }

    const auto targetStage = StageFromName(stageName);
    const char* sourceProp = SourcePropertyForStage(targetStage);
    if (!sourceProp)
    {
        r.diagnostic = "Unknown upstream stage: " + std::string(stageName);
        return r;
    }

    auto upstream = xq::pipeline::ResolveUpstreamNode(
        m_DataStorage, sourceNode, sourceProp, targetStage);

    if (upstream.IsNotNull())
    {
        r.ok = true;
        r.value = upstream->GetName();
        return r;
    }

    r.diagnostic = "Could not resolve upstream node for stage '" +
                   std::string(stageName) + "'.";
    return r;
}

xq_ApiResult xq_PythonApiService::AddPath(std::string_view name,
                                          std::string_view sourceImage) const
{
    auto data = xq_VesselCenterline::New();
    return AddPipelineNode(name, "path", "path",
                           data.GetPointer(),
                           {{xq::pipeline::kSourceImageProperty, std::string(sourceImage)}});
}

xq_ApiResult xq_PythonApiService::AddSegmentation(std::string_view name,
                                                  std::string_view sourcePath,
                                                  std::string_view sourceImage) const
{
    auto data = xq_ProfileGroup::New();
    return AddPipelineNode(name, "contour_group", "contour_group",
                           data.GetPointer(),
                           {{xq::pipeline::kSourcePathProperty, std::string(sourcePath)},
                            {xq::pipeline::kSourceImageProperty, std::string(sourceImage)}});
}

xq_ApiResult xq_PythonApiService::AddModel(std::string_view name,
                                           std::string_view sourceContourGroups,
                                           std::string_view sourcePath) const
{
    auto data = xq_Model::New();
    data->SetType("PolyData");
    return AddPipelineNode(name, "model", "model",
                           data.GetPointer(),
                           {{xq::pipeline::kSourceContourGroupsProperty, std::string(sourceContourGroups)},
                            {xq::pipeline::kSourcePathProperty, std::string(sourcePath)}});
}

xq_ApiResult xq_PythonApiService::AddMesh(std::string_view name,
                                          std::string_view sourceModel) const
{
    auto data = xq_MitkGrid::New();
    return AddPipelineNode(name, "volume_mesh", "volume_mesh",
                           data.GetPointer(),
                           {{xq::pipeline::kSourceModelProperty, std::string(sourceModel)}});
}

xq_ApiResult xq_PythonApiService::AddSimulation(std::string_view name,
                                                std::string_view sourceMesh,
                                                std::string_view sourceModel) const
{
    auto data = xq_MitkSolverJob::New();
    auto job = std::make_unique<xq_SolverJob>();
    job->SetJobName(std::string(name));
    data->SetSimJob(std::move(job), 0);
    data->SetMeshName(sourceMesh);
    data->SetModelName(sourceModel);
    data->SetStatus("created");
    return AddPipelineNode(name, "simulation_prep", "simulation_prep",
                           data.GetPointer(),
                           {{xq::pipeline::kSourceMeshProperty, std::string(sourceMesh)},
                            {xq::pipeline::kSourceModelProperty, std::string(sourceModel)}});
}

xq_ApiResult xq_PythonApiService::AddROMSimulation(
    std::string_view name,
    std::string_view sourceMesh,
    std::string_view sourceModel,
    std::string_view sourcePath) const
{
    auto data = xq_MitkROMJob::New();
    auto job = std::make_unique<xq_ROMJob>();
    job->SetJobName(name);
    job->SetModelType("1D");
    job->SetProperty("mesh", sourceMesh);
    job->SetProperty("model", sourceModel);
    job->SetProperty("path", sourcePath);
    job->SetProperty("status", "created");
    job->SetProperty("diagnostic",
                     "Python-created ROM context is metadata-only; native solver execution remains disabled.");
    data->SetROMJob(std::move(job), 0);
    data->SetStatus("created");

    auto result = AddPipelineNode(name, "rom_simulation", "rom_simulation",
                                  data.GetPointer(),
                                  {{xq::pipeline::kSourceMeshProperty, std::string(sourceMesh)},
                                   {xq::pipeline::kSourceModelProperty, std::string(sourceModel)},
                                   {xq::pipeline::kSourcePathProperty, std::string(sourcePath)}});
    if (result.ok)
    {
        auto node = FindNodePointer(name);
        if (node.IsNotNull())
        {
            node->SetStringProperty("xq.rom.model_order", "1D");
            node->SetStringProperty("xq.rom.status", "created");
            node->SetStringProperty("xq.rom.capability.diagnostic",
                                    "Python bridge can create and restore ROM job context; native solver execution is disabled.");
        }
    }
    return result;
}

xq_ApiResult xq_PythonApiService::AddMultiPhysics(
    std::string_view name,
    std::string_view sourceMesh,
    std::string_view sourceModel) const
{
    auto data = xq_MitkMultiPhysicsJob::New();
    auto job = std::make_unique<xq_MultiPhysicsJob>();
    job->SetJobName(name);
    job->SetProperty("source_mesh", sourceMesh);
    job->SetProperty("source_model", sourceModel);
    job->SetProperty("status", "created");
    job->SetProperty("diagnostic",
                     "Python-created MultiPhysics context is metadata-only; native solver execution remains disabled.");

    xq_MultiPhysicsDomain domain;
    domain.name = "fluid";
    domain.type = xq_MultiPhysicsDomainType::Fluid;
    domain.material.density = 1.06;
    domain.material.viscosity = 0.04;
    domain.properties["mesh"] = std::string(sourceMesh);
    job->AddDomain(domain);

    xq_MultiPhysicsEquation equation;
    equation.name = "fluid";
    equation.type = xq_MultiPhysicsEquationType::Fluid;
    equation.domainNames.push_back(domain.name);
    equation.properties["status"] = "created";
    job->AddEquation(equation);

    data->SetJob(std::move(job), 0);
    data->SetStatus("created");

    auto result = AddPipelineNode(name, "multiphysics", "multiphysics",
                                  data.GetPointer(),
                                  {{xq::pipeline::kSourceMeshProperty, std::string(sourceMesh)},
                                   {xq::pipeline::kSourceModelProperty, std::string(sourceModel)}});
    if (result.ok)
    {
        auto node = FindNodePointer(name);
        if (node.IsNotNull())
        {
            const std::string domains = "fluid:" + std::string(sourceMesh);
            node->SetStringProperty("xq.multiphysics.domains", domains.c_str());
            node->SetStringProperty("xq.multiphysics.equations", "fluid");
            node->SetStringProperty("xq.multiphysics.status", "created");
            node->SetStringProperty("xq.multiphysics.capability.diagnostic",
                                    "Python bridge can create and restore MultiPhysics job context; native solver execution is disabled.");
        }
    }
    return result;
}

xq_ApiResult xq_PythonApiService::ReadPath(std::string_view name) const
{
    return ReadTypedNode(name, "xq_VesselCenterline", "path");
}

xq_ApiResult xq_PythonApiService::ReadSegmentation(std::string_view name) const
{
    return ReadTypedNode(name, "xq_ProfileGroup", "contour_group");
}

xq_ApiResult xq_PythonApiService::ReadModel(std::string_view name) const
{
    return ReadTypedNode(name, "xq_Model", "model");
}

xq_ApiResult xq_PythonApiService::ReadMesh(std::string_view name) const
{
    return ReadTypedNode(name, "xq_MitkGrid", "volume_mesh");
}

xq_ApiResult xq_PythonApiService::ReadSimulation(std::string_view name) const
{
    return ReadTypedNode(name, "xq_MitkSolverJob", "simulation_prep");
}

xq_ApiResult xq_PythonApiService::ReadROMSimulation(std::string_view name) const
{
    return ReadTypedNode(name, "xq_MitkROMJob", "rom_simulation");
}

xq_ApiResult xq_PythonApiService::ReadMultiPhysics(std::string_view name) const
{
    return ReadTypedNode(name, "xq_MitkMultiPhysicsJob", "multiphysics");
}

xq_ApiResult xq_PythonApiService::ReadGeometry(std::string_view modelName) const
{
    xq_ApiResult r = ReadModel(modelName);
    if (!r.ok)
        return r;

    auto node = FindNodePointer(modelName);
    auto* model = node.IsNotNull() ? dynamic_cast<xq_Model*>(node->GetData()) : nullptr;
    if (!model)
    {
        r.ok = false;
        r.diagnostic = "Node is not an xq_Model.";
        return r;
    }

    auto* geometry = model->GetModelElement(0);
    r.value = geometry ? "geometry_present" : "geometry_empty";
    return r;
}

xq_ApiResult xq_PythonApiService::ProjectOpen(std::string_view path) const
{
    xq_ApiResult r;
    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }
    if (path.empty())
    {
        r.diagnostic = "project.open path is empty.";
        return r;
    }

    mitk::DataStorage::Pointer storage(m_DataStorage);
    xq_WorkspaceManager manager;
    if (!manager.OpenProject(storage, std::string(path)))
    {
        r.diagnostic = "project.open failed for '" + std::string(path) + "'.";
        return r;
    }

    r.ok = true;
    r.value = std::string(path);
    return r;
}

xq_ApiResult xq_PythonApiService::ProjectSave(std::string_view path) const
{
    xq_ApiResult r;
    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }
    if (path.empty())
    {
        r.diagnostic = "project.save path is empty.";
        return r;
    }

    mitk::DataStorage::Pointer storage(m_DataStorage);
    xq_WorkspaceManager manager;
    if (!manager.SaveProject(storage, std::string(path)))
    {
        r.diagnostic = "project.save failed for '" + std::string(path) + "'.";
        return r;
    }

    r.ok = true;
    r.value = std::string(path);
    return r;
}

void xq_PythonApiService::SetDataStorage(mitk::DataStorage* ds)
{
    m_DataStorage = ds;
}

mitk::DataNode::Pointer
xq_PythonApiService::FindNodePointer(std::string_view name) const
{
    if (!m_DataStorage || name.empty())
        return nullptr;

    auto allNodes = m_DataStorage->GetAll();
    if (!allNodes)
        return nullptr;

    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto node = it->Value();
        if (node.IsNotNull() && node->GetName() == name)
            return node;
    }
    return nullptr;
}

xq_NodeDescriptor
xq_PythonApiService::DescribeNode(const mitk::DataNode::Pointer& node) const
{
    xq_NodeDescriptor desc;
    if (node.IsNull())
    {
        desc.type = "unknown";
        return desc;
    }

    desc.name = node->GetName();
    auto* data = node->GetData();
    desc.type = data ? data->GetNameOfClass() : "metadata_only";
    node->GetStringProperty(xq::pipeline::kStageProperty, desc.stage);
    return desc;
}

xq_ApiResult xq_PythonApiService::ReadTypedNode(
    std::string_view name,
    std::string_view expectedClass,
    std::string_view expectedStage) const
{
    xq_ApiResult r;
    auto node = FindNodePointer(name);
    if (node.IsNull())
    {
        r.diagnostic = "Node not found: " + std::string(name);
        return r;
    }

    auto desc = DescribeNode(node);
    if (desc.type != expectedClass)
    {
        r.diagnostic = "Node '" + std::string(name) + "' has type '" +
                       desc.type + "', expected '" + std::string(expectedClass) + "'.";
        return r;
    }
    if (desc.stage != expectedStage)
    {
        r.diagnostic = "Node '" + std::string(name) + "' has stage '" +
                       desc.stage + "', expected '" + std::string(expectedStage) + "'.";
        return r;
    }

    r.ok = true;
    r.nodes.push_back(std::move(desc));
    return r;
}

xq_ApiResult xq_PythonApiService::AddPipelineNode(
    std::string_view name,
    std::string_view stageName,
    std::string_view parentStageName,
    mitk::BaseData::Pointer data,
    const std::vector<std::pair<std::string, std::string>>& sources) const
{
    xq_ApiResult r;

    if (!m_DataStorage)
    {
        r.diagnostic = "DataStorage not set.";
        return r;
    }
    if (name.empty())
    {
        r.diagnostic = "Node name is empty.";
        return r;
    }
    if (FindNodePointer(name).IsNotNull())
    {
        r.diagnostic = "Node already exists: " + std::string(name);
        return r;
    }

    const auto stage = StageFromName(stageName);
    if (stage == xq::pipeline::Stage::Unknown)
    {
        r.diagnostic = "Unsupported pipeline stage: " + std::string(stageName);
        return r;
    }

    auto node = mitk::DataNode::New();
    node->SetName(std::string(name));
    if (data.IsNotNull())
        node->SetData(data);
    xq::pipeline::MarkGeneratedNode(node, stage, "python_api", "python_api");
    node->SetBoolProperty("xq.python.metadata_only", data.IsNull());

    for (const auto& [key, value] : sources)
    {
        if (!value.empty())
            node->SetStringProperty(key.c_str(), value.c_str());
    }

    auto parent = xq::pipeline::FindCategoryFolder(
        m_DataStorage, StageFromName(parentStageName));
    if (parent.IsNotNull())
        m_DataStorage->Add(node, parent);
    else
        m_DataStorage->Add(node);

    r.ok = true;
    r.nodes.push_back(DescribeNode(node));
    return r;
}
