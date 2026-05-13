#pragma once

#include <xqModuleROMSimulationExports.h>

#include <mitkDataNode.h>

#include <string>

// Persistence helpers for xq_MitkROMJob: save to / load from an XML file.
class XQMODULEROMSIMULATION_EXPORT xq_MitkROMJobIO
{
public:
    struct XQMODULEROMSIMULATION_EXPORT Result
    {
        bool ok = false;
        std::string diagnostic;
    };

    // Write the job embedded in a data node to `filePath`.
    static Result Write(const mitk::DataNode* node, const std::string& filePath);

    // Read a job from `filePath` and attach it to `node`.
    static Result Read(mitk::DataNode* node, const std::string& filePath);
};
