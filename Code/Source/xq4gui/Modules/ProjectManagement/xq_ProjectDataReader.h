#ifndef XQ_PROJECTDATAREADER_H
#define XQ_PROJECTDATAREADER_H

#include <xqProjectManagementExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>
#include <vector>

struct XQPROJECTMANAGEMENT_EXPORT xq_ProjectDataReadResult
{
    bool ok = false;
    std::vector<mitk::DataNode::Pointer> nodes;
    std::vector<std::string> diagnostics;
};

class XQPROJECTMANAGEMENT_EXPORT xq_ProjectDataReader
{
public:
    // Load node data from a file path.
    // For types MITK supports natively, delegates to mitk::IOUtil::Load.
    // For custom XQ types, uses explicit readers.
    // Returns created DataNodes — caller adds them to DataStorage.
    static xq_ProjectDataReadResult LoadNodeData(const std::string& fullPath);
};

#endif
