#ifndef XQ_NODEMETADATAIO_H
#define XQ_NODEMETADATAIO_H

#include <xqProjectManagementExports.h>

#include <mitkDataNode.h>

#include <string>

class XQPROJECTMANAGEMENT_EXPORT xq_NodeMetadataIO
{
public:
    // Write pipeline metadata for a node to an XML sidecar file.
    // Returns true on success, false on failure.
    static bool WriteNodeMetadata(
        const mitk::DataNode* node,
        const std::string& metaPath);

    // Read pipeline metadata from an XML sidecar file and apply it to a node.
    // Does NOT overwrite name if the node already has one.
    // Returns true on success, false if file missing or malformed.
    static bool ReadNodeMetadata(
        mitk::DataNode* node,
        const std::string& metaPath);
};

#endif
