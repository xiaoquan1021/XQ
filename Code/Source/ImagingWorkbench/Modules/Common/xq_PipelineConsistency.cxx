#include "xq_PipelineConsistency.h"
#include "xq_PipelineDataUtils.h"

namespace
{

std::string GetProperty(const mitk::DataNode* node, const std::string& key)
{
    if (!node) return {};
    std::string val;
    node->GetStringProperty(key.c_str(), val);
    return val;
}

mitk::DataNode::Pointer FindNodeByName(mitk::DataStorage* ds, const std::string& name)
{
    if (!ds || name.empty()) return nullptr;
    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        if (it->Value()->GetName() == name)
            return it->Value();
    }
    return nullptr;
}

void CheckUpstreamSource(mitk::DataStorage* ds,
                         const mitk::DataNode* node,
                         const std::string& sourceProp,
                         xq::pipeline::Stage expectedStage,
                         const std::string& sourceLabel,
                         std::vector<xq_PipelineIssue>& issues)
{
    auto sourceNames = xq::pipeline::SplitSourceList(GetProperty(node, sourceProp));
    if (sourceNames.empty())
    {
        if (expectedStage != xq::pipeline::Stage::Unknown)
        {
            const auto candidates = xq::pipeline::GetNodesByStage(ds, expectedStage);
            if (candidates.size() > 1)
            {
                issues.push_back({xq_PipelineIssue::Severity::Error,
                                  node->GetName(),
                                  "Ambiguous " + sourceLabel +
                                      " source: metadata is missing and multiple upstream nodes exist.",
                                  "Set the " + sourceProp +
                                      " property before opening or running this tool."});
                return;
            }
        }

        issues.push_back({xq_PipelineIssue::Severity::Warning,
                          node->GetName(),
                          "Missing source reference: " + sourceLabel,
                          "Set the " + sourceProp + " property."});
        return;
    }

    for (const auto& srcName : sourceNames)
    {
        auto srcNode = FindNodeByName(ds, srcName);
        if (!srcNode)
        {
            issues.push_back({xq_PipelineIssue::Severity::Error,
                              node->GetName(),
                              "Source node not found: " + srcName,
                              "Recreate upstream node '" + srcName + "' or repoint source."});
            continue;
        }
        if (expectedStage != xq::pipeline::Stage::Unknown &&
            !xq::pipeline::HasStage(srcNode, expectedStage))
        {
            issues.push_back({xq_PipelineIssue::Severity::Warning,
                              node->GetName(),
                              "Source node '" + srcName + "' does not have the expected pipeline stage.",
                              "Re-mark the source node with the correct pipeline stage."});
        }
    }
}

std::string GetNodeStage(const mitk::DataNode* node)
{
    if (!node) return {};
    std::string stage;
    node->GetStringProperty(xq::pipeline::kStageProperty, stage);
    return stage;
}

bool IsStageNode(const mitk::DataNode* node)
{
    return !GetNodeStage(node).empty();
}

// Map a pipeline stage to the expected parent-folder data class name
// (as produced by ProjectManagement folder types).
std::string_view ExpectedFolderClass(xq::pipeline::Stage stage)
{
    switch (stage)
    {
    case xq::pipeline::Stage::Image:
    case xq::pipeline::Stage::ImageProcessing: return "xq_ImageFolder";
    case xq::pipeline::Stage::Path:            return "xq_PathFolder";
    case xq::pipeline::Stage::ContourGroup:    return "xq_SegmentationFolder";
    case xq::pipeline::Stage::Segmentation3D:  return "xq_SegmentationFolder";
    case xq::pipeline::Stage::Model:           return "xq_ModelFolder";
    case xq::pipeline::Stage::VolumeMesh:      return "xq_GridFolder";
    case xq::pipeline::Stage::SimulationPrep:  return "xq_SimulationFolder";
    case xq::pipeline::Stage::Result:          return "xq_SimulationFolder";
    default: return {};
    }
}

// Check whether a pipeline node lives under the expected folder type.
// Uses the parent data's class name, which covers the specific folder types
// produced by ProjectManagement (xq_PathFolder, xq_SegmentationFolder, etc.).
void CheckStageFolderPlacement(mitk::DataStorage* ds,
                               const mitk::DataNode* node,
                               xq::pipeline::Stage stage,
                               std::vector<xq_PipelineIssue>& issues)
{
    auto expectedClass = ExpectedFolderClass(stage);
    if (expectedClass.empty()) return;

    auto sources = ds->GetSources(node);
    if (!sources || sources->empty()) return;

    auto parent = (*sources->Begin()).Value();
    const auto* parentData = parent ? parent->GetData() : nullptr;
    if (!parentData) return;

    std::string parentClass = parentData->GetNameOfClass();
    if (parentClass == expectedClass) return;

    // Accept any folder whose class name ends with the expected class
    // (handles subclassed or renamed folder types).
    if (parentClass.size() >= expectedClass.size() &&
        parentClass.compare(parentClass.size() - expectedClass.size(),
                            expectedClass.size(), expectedClass) == 0)
        return;

    issues.push_back({xq_PipelineIssue::Severity::Warning,
                      node->GetName(),
                      "Pipeline node is not under the expected folder (found: " +
                          parentClass + ", expected: " + std::string(expectedClass) + ")",
                      "Move the node to the correct category folder."});
}

} // namespace

std::vector<xq_PipelineIssue> CheckDataStorage(mitk::DataStorage* ds)
{
    std::vector<xq_PipelineIssue> issues;

    if (!ds)
    {
        issues.push_back({xq_PipelineIssue::Severity::Error, "",
                          "DataStorage is null.", ""});
        return issues;
    }

    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto nodeIssues = CheckNode(ds, it->Value());
        issues.insert(issues.end(), nodeIssues.begin(), nodeIssues.end());
    }

    return issues;
}

std::vector<xq_PipelineIssue> CheckNode(
    mitk::DataStorage* ds,
    const mitk::DataNode* node)
{
    std::vector<xq_PipelineIssue> issues;

    if (!node || !ds)
        return issues;

    // Skip non-pipeline nodes (folders, etc.)
    if (!IsStageNode(node))
        return issues;

    // Check imported image / preprocessing stages.
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Image))
    {
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::Image, issues);
        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "Image" && className != "LabelSetImage")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "Image node has unexpected data type: " + className +
                                      " (expected Image).",
                                  "Re-import the image or convert it to a MITK image."});
            }
        }
    }

    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::ImageProcessing))
    {
        CheckUpstreamSource(ds, node, "xq.source.image",
                            xq::pipeline::Stage::Unknown, "source image", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::ImageProcessing, issues);

        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "Image" && className != "Surface" &&
                className != "LabelSetImage")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "ImageProcessing node has unexpected data type: " + className +
                                      " (expected Image or Surface).",
                                  "Re-run the operation so the result is stored as a supported MITK type."});
            }
        }
    }

    // Check Path stage
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Path))
    {
        CheckUpstreamSource(ds, node, "xq.source.image",
                            xq::pipeline::Stage::Unknown, "source image", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::Path, issues);
    }

    // Check ContourGroup stage
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::ContourGroup))
    {
        CheckUpstreamSource(ds, node, "xq.source.path",
                            xq::pipeline::Stage::Path, "source path", issues);
        CheckUpstreamSource(ds, node, "xq.source.image",
                            xq::pipeline::Stage::Unknown, "source image", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::ContourGroup, issues);

        // Contour group content check: warn if no profiles are present
        int profileCount = 0;
        node->GetIntProperty("xq.contour.profile_count", profileCount);
        if (profileCount == 0)
        {
            issues.push_back({xq_PipelineIssue::Severity::Warning,
                              node->GetName(),
                              "Contour group has no profiles.",
                              "Run contour extraction to populate the contour group."});
        }
    }

    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Segmentation3D))
    {
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceImageProperty,
                            xq::pipeline::Stage::Unknown, "source image", issues);
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceSegmentationProperty,
                            xq::pipeline::Stage::Unknown, "source segmentation", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::Segmentation3D, issues);

        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "xq_MitkSeg3D" &&
                className.find("LabelSetImage") == std::string::npos)
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "Segmentation3D node has unexpected data type: " + className +
                                      " (expected xq_MitkSeg3D or LabelSetImage).",
                                  "Re-create the segmentation in the 3D segmentation tool."});
            }
        }
    }

    // Check Model stage
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Model))
    {
        CheckUpstreamSource(ds, node, "xq.source.contour_groups",
                            xq::pipeline::Stage::ContourGroup,
                            "source contour groups", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::Model, issues);

        // Data type vs stage mismatch
        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className == "Surface")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "Model node uses legacy mitk::Surface type — face metadata may be lost.",
                                  "Re-create the model using the XQ pipeline to get xq_Model type."});
            }
            else if (className != "xq_Model")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "Model node has unexpected data type: " + className +
                                      " (expected xq_Model).",
                                  "Re-create the model or convert it to xq_Model."});
            }
        }

        // QA property check
        bool qaOk = true;
        if (node->GetBoolProperty("xq.model.qa.ok", qaOk) && !qaOk)
        {
            issues.push_back({xq_PipelineIssue::Severity::Error,
                              node->GetName(),
                              "Model QA check failed.",
                              "Review and fix the model geometry issues before meshing."});
        }
    }

    // Check VolumeMesh stage
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::VolumeMesh))
    {
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceModelProperty,
                            xq::pipeline::Stage::Model, "source model", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::VolumeMesh, issues);

        // Data type vs stage mismatch
        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "xq_MitkGrid")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "VolumeMesh node has unexpected data type: " + className +
                                      " (expected xq_MitkGrid).",
                                  "Re-create the mesh or convert it to xq_MitkGrid."});
            }
        }

        // QA property check
        bool qaOk = true;
        if (node->GetBoolProperty("xq.mesh.qa.ok", qaOk) && !qaOk)
        {
            issues.push_back({xq_PipelineIssue::Severity::Error,
                              node->GetName(),
                              "Mesh QA check failed.",
                              "Review and fix mesh quality issues before simulation."});
        }
    }

    // Check SimulationPrep stage
    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::SimulationPrep))
    {
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceModelProperty,
                            xq::pipeline::Stage::Model, "source model", issues);
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceMeshProperty,
                            xq::pipeline::Stage::VolumeMesh, "source mesh", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::SimulationPrep, issues);

        // Data type vs stage mismatch
        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "xq_MitkSolverJob")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "SimulationPrep node has unexpected data type: " + className +
                                      " (expected xq_MitkSolverJob).",
                                  "Re-create the simulation prep or convert it to xq_MitkSolverJob."});
            }
        }
    }

    if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Result))
    {
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceSimulationJobProperty,
                            xq::pipeline::Stage::SimulationPrep, "source simulation job", issues);
        CheckUpstreamSource(ds, node, xq::pipeline::kSourceSolverCaseProperty,
                            xq::pipeline::Stage::Unknown, "source solver case", issues);
        CheckStageFolderPlacement(ds, node, xq::pipeline::Stage::Result, issues);

        auto* data = node->GetData();
        if (data)
        {
            std::string className = data->GetNameOfClass();
            if (className != "Surface" &&
                className != "xq_MitkGrid" &&
                className != "Image")
            {
                issues.push_back({xq_PipelineIssue::Severity::Warning,
                                  node->GetName(),
                                  "Result node has unexpected data type: " + className +
                                      " (expected Surface, xq_MitkGrid, or Image).",
                                  "Import the result again using a supported result file type."});
            }
        }
    }

    return issues;
}
