#pragma once

#include <xqModuleMultiPhysicsExports.h>

#include <mitkDataNode.h>

#include <string>

// Persistence helpers for xq_MitkMultiPhysicsJob: save to / load from
// an XML file on disk (the same XML format produced by xq_MultiPhysicsXmlWriter).
class XQMODULEMULTIPHYSICS_EXPORT xq_MitkMultiPhysicsJobIO
{
public:
    struct XQMODULEMULTIPHYSICS_EXPORT Result
    {
        bool ok = false;
        std::string diagnostic;
    };

    // Write the job embedded in a data node to `filePath`.  The node's data
    // must be an xq_MitkMultiPhysicsJob.
    static Result Write(const mitk::DataNode* node, const std::string& filePath);

    // Read a job from `filePath` and attach it to `node`.
    static Result Read(mitk::DataNode* node, const std::string& filePath);
};
