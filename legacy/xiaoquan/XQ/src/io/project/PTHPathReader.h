#ifndef XQ_IO_PROJECT_PTH_PATH_READER_H
#define XQ_IO_PROJECT_PTH_PATH_READER_H

#include "core/NodeId.h"
#include "core/XQPath.h"

#include <string>
#include <vector>

namespace xq {

struct PTHReadResult {
    XQPath path;
    std::string pathName;
    std::string sourceRelativePath;
    std::vector<PathSamplePoint> rawSamplePoints;
};

class PTHPathReader {
public:
    enum class Status {
        Ok,
        FileNotFound,
        ParseError,
        EmptyPath,
    };

    static Status read(const std::string& pthFilePath, PTHReadResult* out);
};

} // namespace xq

#endif // XQ_IO_PROJECT_PTH_PATH_READER_H
