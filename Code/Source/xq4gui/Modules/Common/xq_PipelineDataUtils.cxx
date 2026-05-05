#include "xq_PipelineDataUtils.h"

#include <mitkDataStorage.h>

#include <sstream>

namespace xq::pipeline {

std::string_view ToStageName(Stage stage)
{
    switch (stage)
    {
    case Stage::Path:
        return "path";
    case Stage::ContourGroup:
        return "contour_group";
    case Stage::Model:
        return "model";
    case Stage::VolumeMesh:
        return "volume_mesh";
    case Stage::SimulationPrep:
        return "simulation_prep";
    case Stage::Unknown:
    default:
        return "unknown";
    }
}

void MarkNode(const mitk::DataNode::Pointer& node, Stage stage)
{
    if (node.IsNull())
        return;

    node->SetStringProperty(kStageProperty, std::string(ToStageName(stage)).c_str());
    node->SetStringProperty(kVersionProperty, "1");
}

bool HasStage(const mitk::DataNode::Pointer& node, Stage stage)
{
    return HasStage(node.GetPointer(), stage);
}

bool HasStage(const mitk::DataNode* node, Stage stage)
{
    return GetStringProperty(node, kStageProperty) == ToStageName(stage);
}

std::string GetStringProperty(const mitk::DataNode* node, const char* key)
{
    if (!node || !key)
        return {};

    std::string value;
    node->GetStringProperty(key, value);
    return value;
}

void SetStringProperty(
    const mitk::DataNode::Pointer& node, const char* key, std::string_view value)
{
    if (node.IsNull() || key == nullptr)
        return;

    node->SetStringProperty(key, std::string(value).c_str());
}

bool IsPathNode(const mitk::DataNode::Pointer& node)
{
    return IsPathNode(node.GetPointer());
}

bool IsPathNode(const mitk::DataNode* node)
{
    if (!node)
        return false;

    bool isPathProperty = false;
    node->GetBoolProperty("xq.pathplanning.path", isPathProperty);
    const auto* data = node->GetData();
    const bool isPathType =
        data != nullptr && std::string_view(data->GetNameOfClass()) == "xq_VesselCenterline";
    return isPathType || isPathProperty || HasStage(node, Stage::Path);
}

mitk::DataNode::Pointer FindNodeByNameAndStage(
    mitk::DataStorage* dataStorage, std::string_view name, Stage stage)
{
    if (!dataStorage || name.empty())
        return nullptr;

    const auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNull())
            continue;

        if (node->GetName() == name && HasStage(node, stage))
            return node;
    }

    return nullptr;
}

std::vector<mitk::DataNode::Pointer> GetNodesByStage(
    mitk::DataStorage* dataStorage, Stage stage)
{
    std::vector<mitk::DataNode::Pointer> result;
    if (!dataStorage)
        return result;

    const auto allNodes = dataStorage->GetAll();
    result.reserve(allNodes->size());
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNotNull() && HasStage(node, stage))
            result.push_back(node);
    }

    return result;
}

mitk::DataNode::Pointer ResolveUpstreamNode(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode* downstreamNode,
    const char* sourceProperty,
    Stage upstreamStage)
{
    if (!dataStorage || !downstreamNode || !sourceProperty)
        return nullptr;

    const auto name = GetStringProperty(downstreamNode, sourceProperty);
    if (name.empty())
        return nullptr;

    return FindNodeByNameAndStage(dataStorage, name, upstreamStage);
}

mitk::DataNode::Pointer FindCategoryFolder(
    mitk::DataStorage* dataStorage,
    Stage stage,
    const mitk::DataNode* referenceNode)
{
    if (!dataStorage)
        return nullptr;

    // Map pipeline stage to the concrete folder class name produced by
    // xq_PathFolder/xq_SegmentationFolder/etc. in ProjectManagement.
    std::string_view folderClass;
    switch (stage)
    {
    case Stage::Path:          folderClass = "xq_PathFolder"; break;
    case Stage::ContourGroup:  folderClass = "xq_SegmentationFolder"; break;
    case Stage::Model:         folderClass = "xq_ModelFolder"; break;
    case Stage::VolumeMesh:    folderClass = "xq_GridFolder"; break;
    case Stage::SimulationPrep:folderClass = "xq_SimulationFolder"; break;
    default: return nullptr;
    }

    // Quick check: if the DataStorage is empty or has no folder nodes,
    // there is no category folder to find — return null early.
    // This call is placed before the referenceNode walk to ensure the
    // DataStorage is in a valid, populated state first.
    const auto allNodes = dataStorage->GetAll();
    if (!allNodes || allNodes->Size() == 0)
        return nullptr;

    // If a reference node is provided, prefer the folder that shares its
    // nearest project ancestor (the project node typically sets the
    // string property "project.name").
    std::string referenceProject;
    if (referenceNode)
    {
        // Walk up parents to find a project node.
        // Uses GetElement(0) instead of iterator dereference (*Begin())
        // to avoid a GCC -O3 optimization bug that emits "mov 0x0,%reg"
        // (absolute null deref, SIGSEGV) when inlining the iterator chain.
        auto sources = dataStorage->GetSources(referenceNode);
        unsigned int depth = 0;
        while (sources && sources->Size() > 0 && depth < 100)
        {
            const auto parent = sources->GetElement(0);
            if (parent.IsNull())
                break;
            const auto name = GetStringProperty(
                parent.GetPointer(), "project.name");
            if (!name.empty())
            {
                referenceProject = name;
                break;
            }
            sources = dataStorage->GetSources(parent.GetPointer());
            ++depth;
        }
    }

    mitk::DataNode::Pointer fallback = nullptr;
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNull())
            continue;
        const auto* data = node->GetData();
        if (!data)
            continue;
        if (std::string_view(data->GetNameOfClass()) != folderClass)
            continue;

        if (referenceProject.empty())
        {
            // No project context — first match wins.
            return node;
        }

        // Check whether this folder belongs to the same project.
        auto parents = dataStorage->GetSources(node);
        if (parents && parents->Size() > 0)
        {
            const auto projectParent = parents->GetElement(0);
            if (projectParent.IsNotNull())
            {
                const auto projectName = GetStringProperty(
                    projectParent.GetPointer(), "project.name");
                if (projectName == referenceProject)
                    return node;
            }
        }
        if (fallback.IsNull())
            fallback = node;
    }
    return fallback;
}

std::vector<std::string> SplitSourceList(std::string_view csv)
{
    std::vector<std::string> result;
    std::string token;
    for (char c : csv)
    {
        if (c == ';')
        {
            if (!token.empty())
            {
                result.push_back(token);
                token.clear();
            }
        }
        else
        {
            token.push_back(c);
        }
    }
    if (!token.empty())
        result.push_back(token);
    return result;
}

std::string JoinSourceList(const std::vector<std::string>& names)
{
    std::ostringstream oss;
    for (size_t i = 0; i < names.size(); ++i)
    {
        if (i != 0)
            oss << ';';
        oss << names[i];
    }
    return oss.str();
}

} // namespace xq::pipeline
