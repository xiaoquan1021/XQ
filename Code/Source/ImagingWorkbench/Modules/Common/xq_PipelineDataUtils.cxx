#include "xq_PipelineDataUtils.h"

#include <mitkDataStorage.h>
#include <mitkImage.h>

#include <QDateTime>

#include <sstream>

namespace xq::pipeline {

namespace
{

bool IsImageNode(const mitk::DataNode::Pointer& node)
{
    if (node.IsNull() || !node->GetData())
        return false;

    return dynamic_cast<mitk::Image*>(node->GetData()) != nullptr ||
           std::string_view(node->GetData()->GetNameOfClass()) == "Image";
}

std::string_view DataClassName(const mitk::DataNode::Pointer& node)
{
    if (node.IsNull() || !node->GetData())
        return {};

    return node->GetData()->GetNameOfClass();
}

bool MatchesLegacyDataClass(const mitk::DataNode::Pointer& node, Stage stage)
{
    const auto className = DataClassName(node);
    std::string xqType;
    std::string resultFieldName;
    std::string resultFilePath;
    const bool isResultNode =
        (node.IsNotNull() && node->GetStringProperty("xq.type", xqType) && xqType == "result") ||
        (node.IsNotNull() && node->GetStringProperty("xq.result.field_name", resultFieldName)) ||
        (node.IsNotNull() && node->GetStringProperty("xq.result.file_path", resultFilePath));

    switch (stage)
    {
    case Stage::Image:
    case Stage::ImageProcessing:
        return IsImageNode(node);
    case Stage::Path:
        return IsPathNode(node);
    case Stage::ContourGroup:
        return className == "xq_ProfileGroup" || className == "xq_ContourGroup";
    case Stage::Segmentation3D:
        return className == "xq_MitkSeg3D" ||
               className.find("LabelSetImage") != std::string_view::npos;
    case Stage::Model:
        return className == "xq_Model";
    case Stage::VolumeMesh:
        return !isResultNode && className == "xq_MitkGrid";
    case Stage::SimulationPrep:
        return className == "xq_MitkSolverJob";
    case Stage::ROMSimulation:
        return className == "xq_MitkROMJob" || className == "xq_MitkROMSimJob";
    case Stage::MultiPhysics:
        return className == "xq_MitkMultiPhysicsJob" || className == "xq_MitkMPJob";
    case Stage::Result:
        return isResultNode;
    default:
        return false;
    }
}

bool MatchesExpectedUpstream(const mitk::DataNode::Pointer& node, Stage stage)
{
    if (node.IsNull())
        return false;

    if (stage == Stage::Unknown)
        return true;

    return HasStage(node, stage) || MatchesLegacyDataClass(node, stage);
}

bool MatchesExpectedSource(
    const mitk::DataNode::Pointer& node,
    const char* sourceProperty,
    Stage stage)
{
    if (node.IsNull())
        return false;

    if (stage == Stage::Unknown &&
        sourceProperty != nullptr &&
        std::string_view(sourceProperty) == kSourceImageProperty)
        return IsImageNode(node);

    return MatchesExpectedUpstream(node, stage);
}

mitk::DataNode::Pointer FindNodeByName(
    mitk::DataStorage* dataStorage, std::string_view name)
{
    if (!dataStorage || name.empty())
        return nullptr;

    const auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNotNull() && node->GetName() == name)
            return node;
    }

    return nullptr;
}

mitk::DataNode::Pointer UniqueDirectSource(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode* downstreamNode,
    const char* sourceProperty,
    Stage upstreamStage)
{
    auto sources = dataStorage->GetSources(downstreamNode);
    if (!sources || sources->Size() == 0)
        return nullptr;

    mitk::DataNode::Pointer match = nullptr;
    for (auto it = sources->Begin(); it != sources->End(); ++it)
    {
        const auto& source = it->Value();
        if (!MatchesExpectedSource(source, sourceProperty, upstreamStage))
            continue;

        if (match.IsNotNull())
            return nullptr;
        match = source;
    }

    return match;
}

mitk::DataNode::Pointer UniqueGlobalCandidate(
    mitk::DataStorage* dataStorage,
    const char* sourceProperty,
    Stage upstreamStage)
{
    const auto allNodes = dataStorage->GetAll();
    mitk::DataNode::Pointer match = nullptr;
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (!MatchesExpectedSource(node, sourceProperty, upstreamStage))
            continue;

        if (match.IsNotNull())
            return nullptr;
        match = node;
    }

    return match;
}

} // namespace

std::string_view ToStageName(Stage stage)
{
    switch (stage)
    {
    case Stage::Image:
        return "image";
    case Stage::ImageProcessing:
        return "image_processing";
    case Stage::Centerline:
        return "centerline";
    case Stage::Path:
        return "path";
    case Stage::ContourGroup:
        return "contour_group";
    case Stage::Segmentation3D:
        return "segmentation_3d";
    case Stage::Model:
        return "model";
    case Stage::VolumeMesh:
        return "volume_mesh";
    case Stage::SimulationPrep:
        return "simulation_prep";
    case Stage::ROMSimulation:
        return "rom_simulation";
    case Stage::MultiPhysics:
        return "multiphysics";
    case Stage::Result:
        return "result";
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

void MarkGeneratedNode(
    const mitk::DataNode::Pointer& node,
    Stage stage,
    std::string_view algorithm,
    std::string_view createdByTool,
    std::string_view algorithmVersion)
{
    if (node.IsNull())
        return;

    MarkNode(node, stage);
    SetStringProperty(node, kAlgorithmProperty, algorithm);
    SetStringProperty(node, kAlgorithmVersionProperty, algorithmVersion);
    SetStringProperty(node, kCreatedByToolProperty, createdByTool);
    SetStringProperty(
        node,
        kCreatedAtProperty,
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString());
    node->SetBoolProperty(kValidProperty, true);
    SetStringProperty(node, kDiagnosticProperty, "");
    SetStringProperty(node, kLimitationsProperty, "");
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

        if (node->GetName() == name && MatchesExpectedUpstream(node, stage))
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
        if (node.IsNotNull() &&
            (HasStage(node, stage) || MatchesLegacyDataClass(node, stage)))
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
    if (!name.empty())
    {
        const auto sourceNames = SplitSourceList(name);
        const auto& sourceName = sourceNames.empty() ? name : sourceNames.front();

        auto resolved = FindNodeByNameAndStage(dataStorage, sourceName, upstreamStage);
        if (resolved.IsNotNull())
            return resolved;

        if (upstreamStage == Stage::Unknown)
            return FindNodeByName(dataStorage, sourceName);

        // If metadata is present but stale, do not guess a different global
        // node. A direct DataStorage source edge is still safe because it is an
        // explicit relation between these two nodes.
        return UniqueDirectSource(
            dataStorage, downstreamNode, sourceProperty, upstreamStage);
    }

    auto directSource = UniqueDirectSource(
        dataStorage, downstreamNode, sourceProperty, upstreamStage);
    if (directSource.IsNotNull())
        return directSource;

    return UniqueGlobalCandidate(dataStorage, sourceProperty, upstreamStage);
}

std::vector<mitk::DataNode::Pointer> ResolveUpstreamNodes(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode* downstreamNode,
    const char* sourceProperty,
    Stage upstreamStage)
{
    std::vector<mitk::DataNode::Pointer> result;
    if (!dataStorage || !downstreamNode || !sourceProperty)
        return result;

    const auto names = SplitSourceList(GetStringProperty(downstreamNode, sourceProperty));
    if (!names.empty())
    {
        result.reserve(names.size());
        for (const auto& name : names)
        {
            auto node = FindNodeByNameAndStage(dataStorage, name, upstreamStage);
            if (node.IsNull() && upstreamStage == Stage::Unknown)
                node = FindNodeByName(dataStorage, name);
            if (node.IsNotNull())
                result.push_back(node);
        }
        return result;
    }

    auto direct = UniqueDirectSource(
        dataStorage, downstreamNode, sourceProperty, upstreamStage);
    if (direct.IsNotNull())
        result.push_back(direct);
    return result;
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
    case Stage::Image:         folderClass = "xq_ImageFolder"; break;
    case Stage::ImageProcessing: folderClass = "xq_ImageFolder"; break;
    case Stage::Path:          folderClass = "xq_PathFolder"; break;
    case Stage::ContourGroup:  folderClass = "xq_SegmentationFolder"; break;
    case Stage::Segmentation3D:folderClass = "xq_SegmentationFolder"; break;
    case Stage::Model:         folderClass = "xq_ModelFolder"; break;
    case Stage::VolumeMesh:    folderClass = "xq_GridFolder"; break;
    case Stage::SimulationPrep:folderClass = "xq_SimulationFolder"; break;
    case Stage::ROMSimulation: folderClass = "xq_ROMSimulationFolder"; break;
    case Stage::MultiPhysics:  folderClass = "xq_MultiPhysicsFolder"; break;
    case Stage::Result:        folderClass = "xq_SimulationFolder"; break;
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

QString ResolveToolViewIdForNode(const mitk::DataNode* node)
{
    if (!node)
        return QString();

    bool isPath = false;
    if (node->GetBoolProperty("xq.pathplanning.path", isPath) && isPath)
        return QStringLiteral("org.xq.views.pathplanning");

    std::string stage;
    if (node->GetStringProperty("xq.pipeline.stage", stage))
    {
        if (stage == "image" || stage == "image_processing" || stage == "imageprocessing")
            return QStringLiteral("org.xq.views.imageprocessing");
        if (stage == "path")
            return QStringLiteral("org.xq.views.pathplanning");
        if (stage == "contour_group")
            return QStringLiteral("org.xq.views.segmentation");
        if (stage == "segmentation_3d" || stage == "segmentation3d" ||
            stage == "3d_segmentation" || stage == "mitk_segmentation")
            return QStringLiteral("org.xq.views.mitksegmentation");
        if (stage == "model")
            return QStringLiteral("org.xq.views.modeling");
        if (stage == "volume_mesh")
            return QStringLiteral("org.xq.views.meshing");
        if (stage == "simulation_prep")
            return QStringLiteral("org.xq.views.simulation");
        if (stage == "rom_simulation" || stage == "romsimulation")
            return QStringLiteral("org.xq.views.romsimulation");
        if (stage == "multiphysics")
            return QStringLiteral("org.xq.views.multiphysics");
        if (stage == "result")
            return QStringLiteral("org.xq.views.simulation");
    }

    auto* data = node->GetData();
    if (!data)
        return QString();

    const std::string className = data->GetNameOfClass();
    if (className == "xq_VesselCenterline")
        return QStringLiteral("org.xq.views.pathplanning");
    if (className == "xq_ProfileGroup" || className == "xq_ContourGroup")
        return QStringLiteral("org.xq.views.segmentation");
    if (className == "xq_MitkSeg3D" ||
        className.find("LabelSetImage") != std::string::npos)
        return QStringLiteral("org.xq.views.mitksegmentation");
    if (className == "xq_Model")
        return QStringLiteral("org.xq.views.modeling");
    if (className == "Surface")
    {
        std::string xqType;
        if (node->GetStringProperty("xq.type", xqType) && xqType == "model")
            return QStringLiteral("org.xq.views.modeling");
    }
    if (className == "xq_MitkGrid")
        return QStringLiteral("org.xq.views.meshing");
    if (className == "xq_MitkSolverJob")
        return QStringLiteral("org.xq.views.simulation");
    if (className == "xq_MitkROMJob" || className == "xq_MitkROMSimJob")
        return QStringLiteral("org.xq.views.romsimulation");
    if (className == "xq_MitkMultiPhysicsJob" || className == "xq_MitkMPJob")
        return QStringLiteral("org.xq.views.multiphysics");
    if (dynamic_cast<mitk::Image*>(data))
        return QStringLiteral("org.xq.views.imageprocessing");

    return QString();
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
