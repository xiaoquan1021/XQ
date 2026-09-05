#ifndef XQ_ADAPTERS_VTK_MSH_MESH_READER_H
#define XQ_ADAPTERS_VTK_MSH_MESH_READER_H

#include "core/NodeId.h"
#include "core/XQMesh.h"

#include <string>

namespace xq {

struct MSHReadResult {
    XQMesh mesh;
    std::string meshName;
    std::string sourceRelativePath;
};

class MSHMeshReader {
public:
    enum class Status {
        Ok,
        MshNotFound,
        VtuNotFound,
        VtpNotFound,
        MshParseError,
        VtuReadError,
        VtpReadError,
    };

    static Status read(const std::string& mshFilePath, MSHReadResult* out);
};

} // namespace xq

#endif // XQ_ADAPTERS_VTK_MSH_MESH_READER_H
