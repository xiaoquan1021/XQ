#include "xq_PythonApiService.h"

#include <xq_PipelineDataUtils.h>

#include <mitkBaseData.h>

#include <algorithm>
#include <sstream>

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

        xq_NodeDescriptor desc;
        desc.name = node->GetName();

        auto* data = node->GetData();
        desc.type = data ? data->GetNameOfClass() : "unknown";

        std::string stageStr;
        if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Path))
            stageStr = "Path";
        else if (xq::pipeline::HasStage(node, xq::pipeline::Stage::ContourGroup))
            stageStr = "ContourGroup";
        else if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Model))
            stageStr = "Model";
        else if (xq::pipeline::HasStage(node, xq::pipeline::Stage::VolumeMesh))
            stageStr = "VolumeMesh";
        else if (xq::pipeline::HasStage(node, xq::pipeline::Stage::SimulationPrep))
            stageStr = "SimulationPrep";
        else
        {
            // Check string-based stage property
            node->GetStringProperty("xq.pipeline.stage", stageStr);
        }
        desc.stage = stageStr;

        r.nodes.push_back(desc);
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
            xq_NodeDescriptor desc;
            desc.name = node->GetName();
            auto* data = node->GetData();
            desc.type = data ? data->GetNameOfClass() : "unknown";

            std::string stageStr;
            node->GetStringProperty("xq.pipeline.stage", stageStr);
            desc.stage = stageStr;

            r.nodes.push_back(desc);
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

    // Walk the DataStorage to locate the actual node.
    auto allNodes = m_DataStorage->GetAll();
    mitk::DataNode::Pointer sourceNode;
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        if (it->Value() && it->Value()->GetName() == nodeName)
        {
            sourceNode = it->Value();
            break;
        }
    }

    if (!sourceNode)
    {
        r.diagnostic = "Source node pointer is null.";
        return r;
    }

    // Map stage name to enum.
    xq::pipeline::Stage targetStage = xq::pipeline::Stage::Unknown;
    std::string stageLower(stageName);
    std::transform(stageLower.begin(), stageLower.end(), stageLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (stageLower == "path") targetStage = xq::pipeline::Stage::Path;
    else if (stageLower == "contourgroup" || stageLower == "contour_group")
        targetStage = xq::pipeline::Stage::ContourGroup;
    else if (stageLower == "model") targetStage = xq::pipeline::Stage::Model;
    else if (stageLower == "volumemesh" || stageLower == "volume_mesh")
        targetStage = xq::pipeline::Stage::VolumeMesh;
    else if (stageLower == "simulationprep" || stageLower == "simulation_prep")
        targetStage = xq::pipeline::Stage::SimulationPrep;

    // Determine the source property to follow.
    std::string sourceProp;
    if (targetStage == xq::pipeline::Stage::Model ||
        targetStage == xq::pipeline::Stage::VolumeMesh)
        sourceProp = xq::pipeline::kSourceModelProperty;
    else if (targetStage == xq::pipeline::Stage::VolumeMesh)
        sourceProp = xq::pipeline::kSourceModelProperty;
    else if (targetStage == xq::pipeline::Stage::SimulationPrep)
        sourceProp = xq::pipeline::kSourceMeshProperty;
    else
        sourceProp = "xq.source.path"; // generic fallback

    auto upstream = xq::pipeline::ResolveUpstreamNode(
        m_DataStorage, sourceNode, sourceProp.c_str(), targetStage);

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

xq_ApiResult xq_PythonApiService::ProjectOpen(std::string_view /*path*/) const
{
    xq_ApiResult r;
    r.diagnostic = "project.open not implemented in Python API skeleton.";
    return r;
}

xq_ApiResult xq_PythonApiService::ProjectSave(std::string_view /*path*/) const
{
    xq_ApiResult r;
    r.diagnostic = "project.save not implemented in Python API skeleton.";
    return r;
}

void xq_PythonApiService::SetDataStorage(mitk::DataStorage* ds)
{
    m_DataStorage = ds;
}
