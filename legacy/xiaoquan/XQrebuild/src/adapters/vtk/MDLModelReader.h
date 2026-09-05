#ifndef XQ_ADAPTERS_VTK_MDL_MODEL_READER_H
#define XQ_ADAPTERS_VTK_MDL_MODEL_READER_H

#include "core/NodeId.h"
#include "core/XQSurfaceModel.h"

#include <string>

namespace xq {

struct MDLReadResult {
    XQSurfaceModel model;
    std::string modelName;
    std::string sourceRelativePath;
};

class MDLModelReader {
public:
    enum class Status {
        Ok,
        MdlNotFound,
        VtpNotFound,
        MdlParseError,
        VtpReadError,
    };

    static Status read(const std::string& mdlFilePath, MDLReadResult* out);
};

} // namespace xq

#endif // XQ_ADAPTERS_VTK_MDL_MODEL_READER_H
