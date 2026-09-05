#ifndef XQ_IO_PROJECT_CTGR_CONTOUR_READER_H
#define XQ_IO_PROJECT_CTGR_CONTOUR_READER_H

#include "core/NodeId.h"
#include "core/XQContourGroup.h"

#include <string>

namespace xq {

struct CTGRReadResult {
    XQContourGroup group;
    std::string groupName;
    std::string sourceRelativePath;
};

class CTGRContourReader {
public:
    enum class Status {
        Ok,
        FileNotFound,
        ParseError,
        EmptyGroup,
    };

    static Status read(const std::string& ctgrFilePath, CTGRReadResult* out);
};

} // namespace xq

#endif // XQ_IO_PROJECT_CTGR_CONTOUR_READER_H
