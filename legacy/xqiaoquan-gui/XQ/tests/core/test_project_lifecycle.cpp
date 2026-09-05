#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/asset/AssetId.h>
#include <core/asset/AssetRegistry.h>

#include <cstdint>
#include <iostream>
#include <utility>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

} // namespace

int main()
{
    {
        xq::XQProject project;

        if (project.state() != xq::XQProject::LifecycleState::Created) {
            return fail("project starts created");
        }
        if (project.lifecycleEpoch() != 0) {
            return fail("project starts at lifecycle epoch zero");
        }
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open created project reports ok");
        }
        if (project.state() != xq::XQProject::LifecycleState::Open) {
            return fail("open moves project to open");
        }
        if (project.lifecycleEpoch() != 1) {
            return fail("successful open increments lifecycle epoch");
        }
    }

    {
        xq::XQProject project;
        const xq::NodeId node_id(1001);

        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project before close cleanup");
        }
        if (project.scene().insert(xq::XQDataNode(node_id, "volume", "Lifecycle Volume"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert node into open project scene");
        }
        if (project.scene().find(node_id) == nullptr) {
            return fail("inserted project scene node is found");
        }
        if (project.close() != xq::XQProject::LifecycleResult::Ok) {
            return fail("close open project reports ok");
        }
        if (project.state() != xq::XQProject::LifecycleState::Closed) {
            return fail("close moves project to closed");
        }
        if (project.scene().find(node_id) != nullptr) {
            return fail("close clears project scene nodes");
        }
    }

    {
        xq::XQProject project;
        const xq::NodeId source_node_id(1051);
        const xq::NodeId derived_node_id(1052);

        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project before close asset cleanup");
        }

        xq::AssetRegistry& registry = project.assetRegistry();
        const xq::AssetId source_asset =
            registry.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
        const xq::AssetId derived_asset =
            registry.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);
        registry.addRelation(source_asset, derived_asset);

        xq::XQDataNode source_node(source_node_id, "image", "Asset Source");
        source_node.setAssetId(source_asset);
        xq::XQDataNode derived_node(derived_node_id, "model", "Asset Derived");
        derived_node.setAssetId(derived_asset);

        if (project.scene().insert(source_node) != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(derived_node) != xq::XQScene::InsertResult::Inserted) {
            return fail("insert asset-backed nodes before close");
        }
        if (registry.assetCount() != 2 || registry.relationCount() != 1) {
            return fail("asset registry populated before close");
        }

        if (project.close() != xq::XQProject::LifecycleResult::Ok) {
            return fail("close project with asset state reports ok");
        }
        if (project.scene().find(source_node_id) != nullptr
            || project.scene().find(derived_node_id) != nullptr) {
            return fail("close clears asset-backed scene nodes");
        }
        if (project.assetRegistry().assetCount() != 0) {
            return fail("close clears project asset records");
        }
        if (project.assetRegistry().relationCount() != 0) {
            return fail("close clears project asset relations");
        }
        if (project.assetRegistry().find(source_asset) != nullptr
            || project.assetRegistry().find(derived_asset) != nullptr) {
            return fail("close makes old project asset ids unreachable");
        }

        if (project.reopen() != xq::XQProject::LifecycleResult::Ok) {
            return fail("reopen asset-cleaned project reports ok");
        }
        if (project.assetRegistry().assetCount() != 0
            || project.assetRegistry().relationCount() != 0) {
            return fail("reopen does not restore closed asset state");
        }
        const xq::AssetId post_reopen_asset =
            project.assetRegistry().createAsset(xq::AssetCategory::Derived, xq::AssetKind::Mesh);
        if (post_reopen_asset.value() <= derived_asset.value()) {
            return fail("reopen keeps project asset ids monotonic");
        }
    }

    {
        xq::XQProject project;
        const xq::NodeId old_node_id(1101);
        const xq::NodeId new_node_id(1102);

        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project before reopen");
        }
        if (project.scene().insert(xq::XQDataNode(old_node_id, "case", "Closed Case"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert node before reopen");
        }
        if (project.close() != xq::XQProject::LifecycleResult::Ok) {
            return fail("close project before reopen");
        }
        if (project.reopen() != xq::XQProject::LifecycleResult::Ok) {
            return fail("reopen closed project reports ok");
        }
        if (project.state() != xq::XQProject::LifecycleState::Open) {
            return fail("reopen moves project to open");
        }
        if (project.scene().find(old_node_id) != nullptr) {
            return fail("reopened project scene remains empty from close");
        }
        if (project.scene().insert(xq::XQDataNode(new_node_id, "mesh", "Reopened Mesh"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert node after reopen");
        }
        if (project.scene().find(new_node_id) == nullptr) {
            return fail("reopened project scene can find new node");
        }
    }

    {
        xq::XQProject project;
        const std::uint64_t initial_epoch = project.lifecycleEpoch();

        if (project.close() != xq::XQProject::LifecycleResult::InvalidTransition) {
            return fail("close created project reports invalid transition");
        }
        if (project.state() != xq::XQProject::LifecycleState::Created) {
            return fail("invalid close leaves project created");
        }
        if (project.reopen() != xq::XQProject::LifecycleResult::InvalidTransition) {
            return fail("reopen created project reports invalid transition");
        }
        if (project.state() != xq::XQProject::LifecycleState::Created) {
            return fail("invalid reopen leaves project created");
        }
        if (project.lifecycleEpoch() != initial_epoch) {
            return fail("invalid lifecycle transitions preserve epoch");
        }
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open created project before duplicate open");
        }
        const std::uint64_t open_epoch = project.lifecycleEpoch();
        if (project.open() != xq::XQProject::LifecycleResult::InvalidTransition) {
            return fail("open open project reports invalid transition");
        }
        if (project.state() != xq::XQProject::LifecycleState::Open) {
            return fail("invalid duplicate open leaves project open");
        }
        if (project.lifecycleEpoch() != open_epoch) {
            return fail("invalid duplicate open preserves epoch");
        }
    }

    {
        xq::XQProject project;
        xq::XQScene* scene = &project.scene();

        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project for scene identity");
        }
        const std::uint64_t open_epoch = project.lifecycleEpoch();
        if (&project.scene() != scene) {
            return fail("open keeps project scene identity");
        }
        if (project.close() != xq::XQProject::LifecycleResult::Ok) {
            return fail("close project for scene identity");
        }
        const std::uint64_t closed_epoch = project.lifecycleEpoch();
        if (closed_epoch != open_epoch + 1) {
            return fail("successful close increments lifecycle epoch");
        }
        if (&project.scene() != scene) {
            return fail("close keeps project scene identity");
        }
        if (project.reopen() != xq::XQProject::LifecycleResult::Ok) {
            return fail("reopen project for scene identity");
        }
        if (&project.scene() != scene) {
            return fail("reopen keeps project scene identity");
        }
        if (project.lifecycleEpoch() != closed_epoch + 1) {
            return fail("successful reopen increments lifecycle epoch");
        }
    }

    {
        const xq::NodeId copied_node_id(1201);
        xq::XQProject source;
        if (source.open() != xq::XQProject::LifecycleResult::Ok
            || source.scene().insert(
                   xq::XQDataNode(copied_node_id, "path", "Assigned Path"))
                != xq::XQScene::InsertResult::Inserted) {
            return fail("seed project replacement source");
        }

        xq::XQProject target;
        if (target.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project replacement target");
        }
        const std::uint64_t before_copy_assignment = target.lifecycleEpoch();
        target = source;
        if (target.lifecycleEpoch() != before_copy_assignment + 1
            || target.state() != xq::XQProject::LifecycleState::Open
            || target.scene().find(copied_node_id) == nullptr) {
            return fail("copy assignment replaces state and advances epoch");
        }

        const std::uint64_t before_self_assignment = target.lifecycleEpoch();
        target = target;
        if (target.lifecycleEpoch() != before_self_assignment) {
            return fail("self assignment preserves lifecycle epoch");
        }

        xq::XQProject replacement = source;
        const std::uint64_t before_move_assignment = target.lifecycleEpoch();
        target = std::move(replacement);
        if (target.lifecycleEpoch() != before_move_assignment + 1
            || target.state() != xq::XQProject::LifecycleState::Open
            || target.scene().find(copied_node_id) == nullptr) {
            return fail("move assignment replaces state and advances epoch");
        }
    }

    return 0;
}
