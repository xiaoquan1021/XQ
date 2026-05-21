#pragma once

#include <xqModuleCommonExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>
#include <vector>

struct XQMODULECOMMON_EXPORT xq_PipelineIssue
{
    enum class Severity { Info, Warning, Error };
    Severity severity = Severity::Info;
    std::string nodeName;
    std::string message;
    std::string suggestedFix;
};

// Check the entire DataStorage for pipeline consistency problems.
XQMODULECOMMON_EXPORT std::vector<xq_PipelineIssue> CheckDataStorage(
    mitk::DataStorage* ds);

// Check a single node and its upstream dependencies.
XQMODULECOMMON_EXPORT std::vector<xq_PipelineIssue> CheckNode(
    mitk::DataStorage* ds,
    const mitk::DataNode* node);
