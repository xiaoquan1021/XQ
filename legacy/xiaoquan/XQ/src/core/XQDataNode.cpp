#include "core/XQDataNode.h"

#include <utility>

namespace xq {

XQDataNode::XQDataNode(const NodeId& id,
                       const std::string& domain_type,
                       const std::string& display_name)
    : id_(id)
    , domain_type_(domain_type)
    , display_name_(display_name)
    , domain_(XQDomainType::Unknown)
    , payload_()
{
}

XQDataNode::XQDataNode(const NodeId& id,
                       XQDomainType domain,
                       const std::string& display_name,
                       std::shared_ptr<XQPayload> payload)
    : id_(id)
    , domain_type_(domainTypeToString(domain))
    , display_name_(display_name)
    , domain_(domain)
    , payload_(std::move(payload))
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

XQDomainType XQDataNode::domainType() const
{
    return domain_;
}

const std::shared_ptr<XQPayload>& XQDataNode::payload() const
{
    return payload_;
}

void XQDataNode::setPayload(XQDomainType domain, std::shared_ptr<XQPayload> payload)
{
    domain_ = domain;
    domain_type_ = domainTypeToString(domain);
    payload_ = std::move(payload);
}

void XQDataNode::setAssetId(const AssetId& id)
{
    asset_id_ = id;
    has_asset_id_ = true;
}

bool XQDataNode::hasAssetId() const
{
    return has_asset_id_;
}

const AssetId& XQDataNode::assetId() const
{
    return asset_id_;
}

} // namespace xq
