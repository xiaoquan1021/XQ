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

// Read options (M9b-E). lazyGeometry: when true, surface/mesh assets that carry
// their geometry blobs are NOT materialized into resident handles; instead the
// payload is stamped with the asset's id (geometryAssetId) and the services
// layer resolves the geometry on demand via GeometryResourceManager. Default
// false keeps the eager whole-blob materialization (byte-for-byte unchanged).
struct XQProjectReadOptions {
    bool lazyGeometry = false;
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

    // Overload taking read options (M9b-E). The no-options overload forwards
    // {} (eager), so existing callers are unaffected.
    static Status load(const std::string& projectFilePath, XQProjectReadResult* out,
                       const XQProjectReadOptions& options);
};

} // namespace xq

#endif // XQ_IO_PROJECT_XQ_PROJECT_READER_H
