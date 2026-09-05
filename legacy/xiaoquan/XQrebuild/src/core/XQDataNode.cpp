#include "core/XQDataNode.h"

namespace xq {

XQDataNode::XQDataNode(const NodeId& id,
                       const std::string& domain_type,
                       const std::string& display_name)
    : id_(id)
    , domain_type_(domain_type)
    , display_name_(display_name)
{
}

const NodeId& XQDataNode::id() const
{
    return id_;
}

const std::string& XQDataNode::domain_type() const
{
    return domain_type_;
}

const std::string& XQDataNode::display_name() const
{
    return display_name_;
}

} // namespace xq
