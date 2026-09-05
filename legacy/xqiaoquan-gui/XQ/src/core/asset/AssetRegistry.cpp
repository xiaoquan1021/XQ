#include "core/asset/AssetRegistry.h"

#include <algorithm>
#include <limits>

namespace xq {

AssetRegistry::AssetRegistry()
    : next_id_(1) // 0 is AssetId::invalid()
{
}

AssetId AssetRegistry::nextAvailableAssetId() const
{
    if (next_id_ == 0
        || next_id_ == (std::numeric_limits<AssetId::ValueType>::max)()) {
        return AssetId::invalid();
    }
    return AssetId(next_id_);
}

AssetId AssetRegistry::createAsset(AssetCategory category, AssetKind kind)
{
    const AssetId id = nextAvailableAssetId();
    if (!id.is_valid()) {
        // id space exhausted (or already wrapped): refuse rather than reissue 0/dup.
        return AssetId::invalid();
    }

    ++next_id_;

    AssetRecord record;
    record.id = id;
    record.category = category;
    record.kind = kind;
    records_.insert(std::make_pair(id, record));
    return id;
}

bool AssetRegistry::registerAsset(const AssetId& id, AssetCategory category, AssetKind kind)
{
    if (!id.is_valid()) {
        return false;
    }
    if (records_.find(id) != records_.end()) {
        return false;
    }

    AssetRecord record;
    record.id = id;
    record.category = category;
    record.kind = kind;
    records_.insert(std::make_pair(id, record));

    if (id.value() >= next_id_) {
        if (id.value() == (std::numeric_limits<AssetId::ValueType>::max)()) {
            // The +1 would wrap to the invalid id 0. Park next_id_ at max so the
            // guard in createAsset refuses to mint further ids rather than
            // reissuing 0/duplicates. This asset stays registered; max itself is
            // a valid id, not the invalid sentinel (which is 0).
            next_id_ = id.value();
        } else {
            next_id_ = id.value() + 1;
        }
    }
    return true;
}

bool AssetRegistry::registerAsset(const AssetRecord& record)
{
    if (!registerAsset(record.id, record.category, record.kind)) {
        return false;
    }
    std::map<AssetId, AssetRecord>::iterator it = records_.find(record.id);
    if (it == records_.end()) {
        return false;
    }
    it->second = record;
    return true;
}

bool AssetRegistry::unregisterAsset(const AssetId& id)
{
    if (records_.erase(id) == 0) {
        return false;
    }
    relations_.erase(
        std::remove_if(relations_.begin(), relations_.end(),
                       [&id](const AssetRelation& relation) {
                           return relation.source == id || relation.derived == id;
                       }),
        relations_.end());
    return true;
}

AssetRecord* AssetRegistry::find(const AssetId& id)
{
    std::map<AssetId, AssetRecord>::iterator it = records_.find(id);
    if (it == records_.end()) {
        return 0;
    }
    return &it->second;
}

const AssetRecord* AssetRegistry::find(const AssetId& id) const
{
    std::map<AssetId, AssetRecord>::const_iterator it = records_.find(id);
    if (it == records_.end()) {
        return 0;
    }
    return &it->second;
}

void AssetRegistry::clear()
{
    records_.clear();
    relations_.clear();
}

std::size_t AssetRegistry::assetCount() const
{
    return records_.size();
}

void AssetRegistry::visit_assets(const std::function<void(const AssetRecord&)>& visitor) const
{
    if (!visitor) {
        return;
    }
    for (std::map<AssetId, AssetRecord>::const_iterator it = records_.begin();
         it != records_.end(); ++it) {
        visitor(it->second);
    }
}

bool AssetRegistry::addRelation(const AssetId& source, const AssetId& derived)
{
    if (!source.is_valid() || !derived.is_valid() || source == derived
        || records_.find(source) == records_.end()
        || records_.find(derived) == records_.end()
        || hasRelation(source, derived)) {
        return false;
    }
    AssetRelation relation;
    relation.source = source;
    relation.derived = derived;
    relations_.push_back(relation);
    return true;
}

bool AssetRegistry::hasRelation(const AssetId& source, const AssetId& derived) const
{
    for (std::vector<AssetRelation>::const_iterator it = relations_.begin();
         it != relations_.end(); ++it) {
        if (it->source == source && it->derived == derived) {
            return true;
        }
    }
    return false;
}

bool AssetRegistry::removeRelation(const AssetId& source, const AssetId& derived)
{
    for (std::vector<AssetRelation>::iterator it = relations_.begin();
         it != relations_.end(); ++it) {
        if (it->source == source && it->derived == derived) {
            relations_.erase(it);
            return true;
        }
    }
    return false;
}

std::size_t AssetRegistry::relationCount() const
{
    return relations_.size();
}

void AssetRegistry::visit_relations(
    const std::function<void(const AssetId& source, const AssetId& derived)>& visitor) const
{
    if (!visitor) {
        return;
    }
    for (std::vector<AssetRelation>::const_iterator it = relations_.begin();
         it != relations_.end(); ++it) {
        visitor(it->source, it->derived);
    }
}

} // namespace xq
