#ifndef XQ_IO_PROJECT_SV_PROJECT_READER_H
#define XQ_IO_PROJECT_SV_PROJECT_READER_H

#include "io/project/XQProjectReader.h"

#include <string>

namespace xq {

class SvProjectReader {
public:
    enum class Status {
        Ok,
        ProjectFileNotFound,
        ParseError
    };

    static Status load(const std::string& projectDir, XQProjectReadResult* out);
};

} // namespace xq

#endif // XQ_IO_PROJECT_SV_PROJECT_READER_H
