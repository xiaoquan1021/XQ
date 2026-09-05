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
    , content_revision_(0)
    , scale_slot_()
    , asset_id_()
    , has_asset_id_(false)
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
    , content_revision_(0)
    , scale_slot_()
    , asset_id_()
    , has_asset_id_(false)
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

ContentRevision XQDataNode::contentRevision() const
{
    return content_revision_;
}

void XQDataNode::setContentRevision(ContentRevision revision)
{
    content_revision_ = revision;
}

bool XQDataNode::hasScaleSlot() const
{
    return scale_slot_.has_value();
}

const std::optional<ScaleSlot>& XQDataNode::scaleSlot() const
{
    return scale_slot_;
}

void XQDataNode::setScaleSlot(ScaleSlot slot)
{
    scale_slot_ = slot;
}

void XQDataNode::clearScaleSlot()
{
    scale_slot_.reset();
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
