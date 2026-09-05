#ifndef XQ_IO_PROJECT_XQ_PROJECT_READER_H
#define XQ_IO_PROJECT_XQ_PROJECT_READER_H

#include "core/Diagnostics.h"
#include "core/XQProject.h"

#include <string>
#include <vector>

namespace xq {

struct XQProjectReadResult {
    XQProject project;
    std::vector<Diagnostic> diagnostics;
};

class XQProjectReader {
public:
    enum class Status {
        Ok,
        FileNotFound,
        ParseError,
        UnsupportedVersion
    };

    static Status load(const std::string& projectFilePath, XQProjectReadResult* out);
};

} // namespace xq

#endif // XQ_IO_PROJECT_XQ_PROJECT_READER_H
