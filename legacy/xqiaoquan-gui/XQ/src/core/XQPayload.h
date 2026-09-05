#ifndef XQ_CORE_XQ_PAYLOAD_H
#define XQ_CORE_XQ_PAYLOAD_H

#include "core/XQDomainType.h"

#include <memory>

namespace xq {

// Abstract handle for a node's XQ-owned domain data. Concrete payloads (image
// volume, path, contour group, ...) derive from this. core stays free of any
// external library type; payloads carry only XQ-owned data.
//
// clone() supports edit-style commands that copy the old payload while keeping
// the original node id and provenance (see command-and-scene.md).
class XQPayload {
public:
    virtual ~XQPayload() = default;

    virtual XQDomainType domainType() const = 0;
    virtual std::shared_ptr<XQPayload> clone() const = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_PAYLOAD_H
