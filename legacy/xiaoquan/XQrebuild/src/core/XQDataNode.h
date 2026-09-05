#ifndef XQ_CORE_XQ_DATA_NODE_H
#define XQ_CORE_XQ_DATA_NODE_H

#include "core/NodeId.h"

#include <string>

namespace xq {

class XQDataNode {
public:
    XQDataNode(const NodeId& id,
               const std::string& domain_type,
               const std::string& display_name);

    const NodeId& id() const;
    const std::string& domain_type() const;
    const std::string& display_name() const;

private:
    NodeId id_;
    std::string domain_type_;
    std::string display_name_;
};

} // namespace xq

#endif // XQ_CORE_XQ_DATA_NODE_H
