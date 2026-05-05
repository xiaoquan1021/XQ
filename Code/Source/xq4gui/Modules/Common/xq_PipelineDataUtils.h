#pragma once

#include <xqModuleCommonExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>
#include <string_view>
#include <vector>

namespace xq::pipeline {

enum class Stage
{
    Unknown,
    Path,
    ContourGroup,
    Model,
    VolumeMesh,
    SimulationPrep
};

enum class Severity
{
    Warning,
    Error
};

struct XQMODULECOMMON_EXPORT Diagnostic
{
    Severity severity = Severity::Error;
    std::string message;
};

struct XQMODULECOMMON_EXPORT OperationStatus
{
    bool ok = false;
    std::vector<Diagnostic> diagnostics;
};

XQMODULECOMMON_EXPORT std::string_view ToStageName(Stage stage);
XQMODULECOMMON_EXPORT void MarkNode(const mitk::DataNode::Pointer& node, Stage stage);
XQMODULECOMMON_EXPORT bool HasStage(const mitk::DataNode::Pointer& node, Stage stage);
XQMODULECOMMON_EXPORT bool HasStage(const mitk::DataNode* node, Stage stage);
XQMODULECOMMON_EXPORT std::string GetStringProperty(
    const mitk::DataNode* node, const char* key);
XQMODULECOMMON_EXPORT void SetStringProperty(
    const mitk::DataNode::Pointer& node, const char* key, std::string_view value);
XQMODULECOMMON_EXPORT bool IsPathNode(const mitk::DataNode::Pointer& node);
XQMODULECOMMON_EXPORT bool IsPathNode(const mitk::DataNode* node);
XQMODULECOMMON_EXPORT mitk::DataNode::Pointer FindNodeByNameAndStage(
    mitk::DataStorage* dataStorage, std::string_view name, Stage stage);
XQMODULECOMMON_EXPORT std::vector<mitk::DataNode::Pointer> GetNodesByStage(
    mitk::DataStorage* dataStorage, Stage stage);

inline constexpr char kStageProperty[] = "xq.pipeline.stage";
inline constexpr char kVersionProperty[] = "xq.pipeline.version";
inline constexpr char kSourceImageProperty[] = "xq.source.image";
inline constexpr char kSourcePathProperty[] = "xq.source.path";
inline constexpr char kSourceContourGroupsProperty[] = "xq.source.contour_groups";
inline constexpr char kSourceModelProperty[] = "xq.source.model";
inline constexpr char kSourceMeshProperty[] = "xq.source.mesh";
inline constexpr char kFaceRoleCountProperty[] = "xq.simprep.face_role_count";
inline constexpr char kAlgorithmProperty[] = "xq.pipeline.algorithm";

// XQ Design: explicit upstream resolver — downstream stages call this to read
// upstream producer nodes instead of scanning DataStorage ad-hoc. Returning
// nullptr means the declared upstream dependency is missing, and the caller
// should short-circuit with a diagnostic rather than silently succeed.
XQMODULECOMMON_EXPORT mitk::DataNode::Pointer ResolveUpstreamNode(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode* downstreamNode,
    const char* sourceProperty,
    Stage upstreamStage);

// Locate the category folder node ("PathFolder", "SegmentationFolder",
// "ModelFolder", "MeshFolder", "SimulationFolder") that belongs to the
// project hosting `referenceNode`. Falls back to the first matching folder
// found anywhere in DataStorage if no common project is detected.
// Returns nullptr if no folder of the requested type exists — callers
// should then fall back to attaching under the upstream node.
XQMODULECOMMON_EXPORT mitk::DataNode::Pointer FindCategoryFolder(
    mitk::DataStorage* dataStorage,
    Stage stage,
    const mitk::DataNode* referenceNode = nullptr);

// Split semicolon-separated source list (used by kSourceContourGroupsProperty).
XQMODULECOMMON_EXPORT std::vector<std::string> SplitSourceList(std::string_view csv);
XQMODULECOMMON_EXPORT std::string JoinSourceList(const std::vector<std::string>& names);

} // namespace xq::pipeline
