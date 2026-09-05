#ifndef XQ_PROJECTDATAWRITER_H
#define XQ_PROJECTDATAWRITER_H

#include <xqProjectManagementExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>
#include <vector>

struct XQPROJECTMANAGEMENT_EXPORT xq_ProjectDataWriteResult
{
    bool ok = false;
    std::string savedPath;       // actual path written (may differ from requested due to extension)
    std::string relPath;         // relative path for .xqproj entry
    std::vector<std::string> diagnostics;
};

class XQPROJECTMANAGEMENT_EXPORT xq_ProjectDataWriter
{
public:
    // Save node data to the given base path (without extension).
    // For types MITK supports natively, delegates to mitk::IOUtil::Save.
    // For custom XQ types, uses explicit writers and returns the actual saved path.
    // On failure, result.ok is false and diagnostics explain why.
    static xq_ProjectDataWriteResult SaveNodeData(
        const mitk::DataNode* node,
        const std::string& savePath,
        const std::string& subdir);
};

#endif
