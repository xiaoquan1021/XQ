#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQProject.h>
#include <core/asset/AssetId.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>

#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

// --- Compile-time type isolation (D5) ---------------------------------------
// AssetId and NodeId are distinct strong types. Neither converts to the other,
// nor to/from a raw integer, implicitly. If any of these started to compile as
// an implicit conversion, the static_asserts below would fire at build time.
static_assert(!std::is_convertible<xq::AssetId, xq::NodeId>::value,
              "AssetId must not implicitly convert to NodeId");
static_assert(!std::is_convertible<xq::NodeId, xq::AssetId>::value,
              "NodeId must not implicitly convert to AssetId");
static_assert(!std::is_convertible<xq::AssetId, xq::AssetId::ValueType>::value,
              "AssetId must not implicitly convert to its underlying integer");
static_assert(!std::is_convertible<xq::AssetId::ValueType, xq::AssetId>::value,
              "raw integer must not implicitly convert to AssetId (constructor is explicit)");
static_assert(!std::is_convertible<xq::AssetId, xq::NodeId::ValueType>::value,
              "AssetId must not implicitly convert to NodeId's integer type");

} // namespace

int main()
{
    // --- 1. Asset registration + identity stability -------------------------
    xq::AssetRegistry registry;
    if (registry.assetCount() != 0) {
        return fail("fresh registry has no assets");
    }

    const xq::AssetId image =
        registry.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    const xq::AssetId mask =
        registry.createAsset(xq::AssetCategory::Derived, xq::AssetKind::SegmentationMask);
    const xq::AssetId surface =
        registry.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);

    if (!image.is_valid() || !mask.is_valid() || !surface.is_valid()) {
        return fail("createAsset returns valid ids");
    }
    if (registry.assetCount() != 3) {
        return fail("registry tracks created assets");
    }

    // createAsset hands out distinct ids; identity is stable.
    std::set<xq::AssetId::ValueType> seen;
    seen.insert(image.value());
    seen.insert(mask.value());
    seen.insert(surface.value());
    if (seen.size() != 3) {
        return fail("createAsset ids are distinct");
    }

    const xq::AssetRecord* imageRec = registry.find(image);
    if (imageRec == nullptr) {
        return fail("created asset is findable");
    }
    if (imageRec->id != image) {
        return fail("found record carries its own id (identity stable)");
    }
    if (imageRec->category != xq::AssetCategory::ExternalSource ||
        imageRec->kind != xq::AssetKind::Image) {
        return fail("record preserves category/kind");
    }
    if (registry.find(xq::AssetId(999999)) != nullptr) {
        return fail("unknown id is not found");
    }

    // Non-const find can mutate the record (e.g. attach a blob).
    xq::AssetRecord* surfaceRec = registry.find(surface);
    if (surfaceRec == nullptr) {
        return fail("non-const find returns the record");
    }
    xq::BufferRef points;
    points.relPath = "p.assets/blobs/ab/abcd.bin";
    points.byteCount = 96;
    points.sha256 = "abcd";
    points.elementType = xq::BlobElementType::F64;
    points.components = 3;
    points.elementCount = 4;
    surfaceRec->blobs.emplace_back("points", points);
    if (registry.find(surface)->blobs.size() != 1) {
        return fail("mutation through non-const find persists");
    }

    // --- 2. Duplicate id rejection + registerAsset --------------------------
    // registerAsset under a fresh explicit id succeeds; advancing next_id_.
    const xq::AssetId injected(5000);
    if (!registry.registerAsset(injected, xq::AssetCategory::Derived, xq::AssetKind::Mesh)) {
        return fail("registerAsset accepts a fresh explicit id");
    }
    if (registry.assetCount() != 4) {
        return fail("registerAsset adds a record");
    }
    // Re-registering the same id is rejected, registry unchanged.
    if (registry.registerAsset(injected, xq::AssetCategory::Derived, xq::AssetKind::Mesh)) {
        return fail("registerAsset rejects a duplicate id");
    }
    // An already-created id is also rejected.
    if (registry.registerAsset(image, xq::AssetCategory::ExternalSource, xq::AssetKind::Image)) {
        return fail("registerAsset rejects a previously created id");
    }
    if (registry.assetCount() != 4) {
        return fail("rejected registerAsset leaves the registry unchanged");
    }
    // Invalid id is rejected.
    if (registry.registerAsset(xq::AssetId::invalid(), xq::AssetCategory::Derived,
                               xq::AssetKind::Mesh)) {
        return fail("registerAsset rejects the invalid id");
    }

    // After injecting id 5000, the next createAsset must not collide with it.
    const xq::AssetId afterInject =
        registry.createAsset(xq::AssetCategory::Derived, xq::AssetKind::FlowResult);
    if (afterInject.value() <= injected.value()) {
        return fail("createAsset skips past an injected higher id");
    }
    if (registry.find(afterInject) == nullptr) {
        return fail("post-inject createAsset is findable");
    }

    // --- 3. Asset lineage ---------------------------------------------------
    registry.addRelation(image, mask);     // image -> mask
    registry.addRelation(mask, surface);   // mask  -> surface
    registry.addRelation(surface, injected); // surface -> mesh
    if (registry.relationCount() != 3) {
        return fail("registry tracks relations");
    }

    std::vector<std::pair<xq::AssetId, xq::AssetId>> rels;
    registry.visit_relations([&](const xq::AssetId& s, const xq::AssetId& d) {
        rels.emplace_back(s, d);
    });
    if (rels.size() != 3) {
        return fail("visit_relations visits every relation");
    }
    bool sawImageToMask = false;
    for (const auto& r : rels) {
        if (r.first == image && r.second == mask) {
            sawImageToMask = true;
        }
    }
    if (!sawImageToMask) {
        return fail("lineage preserves source -> derived direction");
    }

    // visit_assets covers every record exactly once.
    std::set<xq::AssetId::ValueType> visited;
    registry.visit_assets([&](const xq::AssetRecord& rec) {
        visited.insert(rec.id.value());
    });
    if (visited.size() != registry.assetCount()) {
        return fail("visit_assets visits every asset once");
    }

    // --- 4. Clear records and lineage without reusing automatic ids ----------
    registry.clear();
    if (registry.assetCount() != 0) {
        return fail("clear removes all asset records");
    }
    if (registry.relationCount() != 0) {
        return fail("clear removes all asset relations");
    }
    if (registry.find(image) != nullptr
        || registry.find(mask) != nullptr
        || registry.find(surface) != nullptr
        || registry.find(injected) != nullptr
        || registry.find(afterInject) != nullptr) {
        return fail("clear makes old asset ids unreachable");
    }
    const xq::AssetId afterClear =
        registry.createAsset(xq::AssetCategory::Derived, xq::AssetKind::AiAnalysis);
    if (afterClear.value() <= afterInject.value()) {
        return fail("createAsset remains monotonic after clear");
    }
    if (registry.assetCount() != 1 || registry.find(afterClear) == nullptr) {
        return fail("registry can create an asset after clear");
    }

    // --- 5. Multiple nodes reference the same asset -------------------------
    xq::XQProject project;
    xq::AssetRegistry& projReg = project.assetRegistry();
    const xq::AssetId shared =
        projReg.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);

    xq::XQDataNode nodeA(xq::NodeId(1), xq::XQDomainType::Unknown, "A", nullptr);
    xq::XQDataNode nodeB(xq::NodeId(2), xq::XQDomainType::Unknown, "B", nullptr);
    if (nodeA.hasAssetId() || nodeB.hasAssetId()) {
        return fail("a node has no assetId until set");
    }
    nodeA.setAssetId(shared);
    nodeB.setAssetId(shared);
    if (!nodeA.hasAssetId() || !nodeB.hasAssetId()) {
        return fail("setAssetId marks the node as bound");
    }
    if (nodeA.assetId() != shared || nodeB.assetId() != shared) {
        return fail("both nodes reference the same assetId");
    }
    if (!(nodeA.assetId() == nodeB.assetId())) {
        return fail("the same asset is shared across nodes");
    }

    // A node with no payload/asset stays unbound (non-asset nodes are allowed).
    const xq::XQDataNode plain(xq::NodeId(3), "image", "plain");
    if (plain.hasAssetId()) {
        return fail("an unbound node reports no assetId");
    }

    // const access to the registry through the project.
    const xq::XQProject& constProject = project;
    if (constProject.assetRegistry().find(shared) == nullptr) {
        return fail("const project exposes the asset registry");
    }

    // --- 6. AssetId serialize round-trip ------------------------------------
    const std::string serialized = shared.serialize();
    xq::AssetId parsed;
    if (!xq::AssetId::parse(serialized, &parsed)) {
        return fail("serialized AssetId parses");
    }
    if (parsed != shared) {
        return fail("AssetId serialize/parse round-trip is identity-stable");
    }
    if (xq::AssetId::parse("not-an-id", &parsed)) {
        return fail("invalid AssetId text is rejected");
    }

    // --- 7. id-space exhaustion refuses rather than wrapping to 0 -----------
    // Registering the max-value id must not let next_id_ wrap to the invalid id
    // 0; the subsequent createAsset must refuse (return invalid) instead of
    // reissuing 0 or a duplicate.
    {
        xq::AssetRegistry exhausted;
        const xq::AssetId maxId(
            (std::numeric_limits<xq::AssetId::ValueType>::max)());
        if (!exhausted.registerAsset(maxId, xq::AssetCategory::Derived,
                                     xq::AssetKind::Mesh)) {
            return fail("registerAsset accepts the max-value id");
        }
        const xq::AssetId afterMax =
            exhausted.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);
        if (afterMax.is_valid()) {
            return fail("createAsset refuses after id space is exhausted");
        }
        if (afterMax != xq::AssetId::invalid()) {
            return fail("exhausted createAsset returns the invalid id");
        }
        // The refused create must not have added a bogus record.
        if (exhausted.assetCount() != 1) {
            return fail("refused createAsset adds no record");
        }
    }

    std::cout << "test_asset_registry: all checks passed" << std::endl;
    return 0;
}
