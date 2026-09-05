#ifndef XQ_IO_PROJECT_XQ_PROJECT_WRITER_H
#define XQ_IO_PROJECT_XQ_PROJECT_WRITER_H

#include "core/XQProject.h"

#include <string>

namespace xq {

class XQProjectWriter {
public:
    enum class Status {
        Ok,
        FileOpenError,
        WriteError
    };

    static Status save(const XQProject& project, const std::string& projectFilePath);
};

} // namespace xq

#endif // XQ_IO_PROJECT_XQ_PROJECT_WRITER_H
