#ifndef XQ_CORE_ASSET_RELATION_H
#define XQ_CORE_ASSET_RELATION_H

#include "core/asset/AssetId.h"

namespace xq {

// Asset-level derivation lineage: how one asset was computed from another
// (image -> mask -> surface -> volume mesh -> flow result). This lives in the
// Asset layer; scene-graph node relations are unchanged and separate.
struct AssetRelation {
    AssetId source;
    AssetId derived;
};

} // namespace xq

#endif // XQ_CORE_ASSET_RELATION_H
