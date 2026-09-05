#pragma once

#include <xqModuleMultiPhysicsExports.h>

#include "xq_MultiPhysicsJob.h"

#include <string>

// Serializes a xq_MultiPhysicsJob to a deterministic XML string.
// Field order is fixed.  Empty fields emit default values.
class XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsXmlWriter
{
public:
    struct XQMODULEMULTIPHYSICS_EXPORT Result
    {
        bool ok = false;
        std::string xml;
        std::string diagnostic;
    };

    // Write the job to an XML string.  Returns ok=false with diagnostic
    // when validation fails (e.g. no domains).
    static Result Write(const xq_MultiPhysicsJob& job);

    // Parse XML back into a job.  Only reads the fields written by Write().
    static Result Read(const std::string& xml, xq_MultiPhysicsJob& outJob);
};
