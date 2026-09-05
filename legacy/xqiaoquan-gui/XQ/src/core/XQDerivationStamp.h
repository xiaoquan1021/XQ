#ifndef XQ_CORE_XQ_DERIVATION_STAMP_H
#define XQ_CORE_XQ_DERIVATION_STAMP_H

#include "core/NodeId.h"
#include "core/asset/AssetId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xq {

using ContentRevision = unsigned long long;

// Immutable-input identity captured when a derived payload is computed.
// Persistence and semantic revision mutation are delivered by later T1
// checkpoints; this value contract is established now so domain payloads do not
// invent incompatible provenance shapes.
struct DerivationInputStamp {
    NodeId nodeId;
    ContentRevision contentRevision = 0;
    std::optional<AssetId> assetId;
    std::string assetFingerprint;
};

struct DerivationStamp {
    std::string algorithmId;
    std::string algorithmVersion;
    std::string parameterSummary;
    std::optional<std::uint64_t> randomSeed;
    std::vector<DerivationInputStamp> inputs;
};

} // namespace xq

#endif // XQ_CORE_XQ_DERIVATION_STAMP_H
