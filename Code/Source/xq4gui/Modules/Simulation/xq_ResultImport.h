#pragma once

#include <xqModuleSimulationExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>
#include <vector>

struct XQMODULESIMULATION_EXPORT xq_ResultImportEntry
{
    std::string filePath;
    std::string nodeName;
    std::string simulationName;
};

struct XQMODULESIMULATION_EXPORT xq_ResultImportOutcome
{
    bool ok = false;
    mitk::DataNode::Pointer node;
    std::vector<std::string> diagnostics;
    std::vector<std::string> fieldNames;
};

class XQMODULESIMULATION_EXPORT xq_ResultImport
{
public:
    // Import a single VTU or VTP result file into the DataStorage as a result
    // node under the Simulation folder, with scalar field discovery.
    static xq_ResultImportOutcome Import(
        mitk::DataStorage* dataStorage,
        const xq_ResultImportEntry& entry);

    // List available point-data and cell-data array names for a loaded node.
    static std::vector<std::string> GetFieldNames(const mitk::DataNode* node);

    // Set the active scalar on a result node for coloring.
    static bool SetActiveScalar(mitk::DataNode* node, const std::string& name);
};
