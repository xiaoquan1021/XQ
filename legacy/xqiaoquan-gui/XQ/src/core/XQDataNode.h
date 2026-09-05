#ifndef XQ_CORE_XQ_DATA_NODE_H
#define XQ_CORE_XQ_DATA_NODE_H

#include "core/NodeId.h"
#include "core/XQDomainType.h"
#include "core/XQDerivationStamp.h"
#include "core/XQPayload.h"
#include "core/XQScaleSlot.h"
#include "core/asset/AssetId.h"

#include <memory>
#include <optional>
#include <string>

namespace xq {

class XQDataNode {
public:
    // Legacy string-typed constructor (kept for back-compat). The typed
    // domain defaults to Unknown and the payload handle is empty.
    XQDataNode(const NodeId& id,
               const std::string& domain_type,
               const std::string& display_name);

    // Payload-aware constructor: carries a typed domain plus an XQ-owned
    // payload handle. domain_type() reflects the string form of the domain.
    XQDataNode(const NodeId& id,
               XQDomainType domain,
               const std::string& display_name,
               std::shared_ptr<XQPayload> payload);

    const NodeId& id() const;
    const std::string& domain_type() const;
    const std::string& display_name() const;

    XQDomainType domainType() const;
    const std::shared_ptr<XQPayload>& payload() const;
    void setPayload(XQDomainType domain, std::shared_ptr<XQPayload> payload);

    // Semantic content version. Readers and non-semantic materialization may
    // restore/preserve this value but must not advance it; semantic edit
    // commands own revision increments.
    ContentRevision contentRevision() const;
    void setContentRevision(ContentRevision revision);

    // Optional biological/physical scale identity. Legacy/default nodes remain
    // absent until a caller explicitly assigns a scale; rendering LOD never
    // reads or writes this field.
    bool hasScaleSlot() const;
    const std::optional<ScaleSlot>& scaleSlot() const;
    void setScaleSlot(ScaleSlot slot);
    void clearScaleSlot();

    // Optional reference to the data asset this node carries. NodeId identifies
    // the scene object; assetId identifies the underlying data asset. The same
    // assetId may be referenced by several nodes (not enforced unique here).
    void setAssetId(const AssetId& id);
    bool hasAssetId() const;
    const AssetId& assetId() const;

private:
    NodeId id_;
    std::string domain_type_;
    std::string display_name_;
    XQDomainType domain_;
    std::shared_ptr<XQPayload> payload_;
    ContentRevision content_revision_;
    std::optional<ScaleSlot> scale_slot_;
    AssetId asset_id_;
    bool has_asset_id_ = false;
};

} // namespace xq

#endif // XQ_CORE_XQ_DATA_NODE_H
