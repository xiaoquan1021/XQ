#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQScene.h>

#include <iostream>

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
        xq::XQScene scene;
        const xq::NodeId source(601);
        const xq::NodeId derived(602);

        if (scene.insert(xq::XQDataNode(source, "volume", "Source Volume"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert source before relation");
        }
        if (scene.insert(xq::XQDataNode(derived, "mesh", "Derived Mesh"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert derived before relation");
        }
        if (scene.link_derived(source, derived) != xq::XQScene::RelationResult::Linked) {
            return fail("link inserted source and derived reports linked");
        }
        if (scene.mark_source_changed(source) != 1) {
            return fail("mark source changed reports one new stale derived");
        }
        if (!scene.is_stale(derived)) {
            return fail("derived is stale after source change");
        }
        if (scene.stale_reason(derived) != xq::XQScene::StaleReason::SourceChanged) {
            return fail("derived stale reason is source changed");
        }
        if (scene.is_stale(source)) {
            return fail("source itself is not stale after source change");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId source(701);
        const xq::NodeId derived(702);
        const xq::NodeId missing(703);

        if (scene.insert(xq::XQDataNode(source, "volume", "Boundary Source"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert boundary source");
        }
        if (scene.insert(xq::XQDataNode(derived, "mesh", "Boundary Derived"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert boundary derived");
        }
        if (scene.link_derived(missing, derived)
            != xq::XQScene::RelationResult::SourceMissing) {
            return fail("missing source reports source missing");
        }
        if (scene.link_derived(source, missing)
            != xq::XQScene::RelationResult::DerivedMissing) {
            return fail("missing derived reports derived missing");
        }
        if (scene.link_derived(source, source)
            != xq::XQScene::RelationResult::SelfRelation) {
            return fail("source equal derived reports self relation");
        }
        if (scene.link_derived(source, derived) != xq::XQScene::RelationResult::Linked) {
            return fail("first duplicate setup link reports linked");
        }
        if (scene.link_derived(source, derived)
            != xq::XQScene::RelationResult::DuplicateRelation) {
            return fail("duplicate source derived link reports duplicate relation");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId source(1001);
        const xq::NodeId mid(1002);
        const xq::NodeId leaf(1003);

        if (scene.insert(xq::XQDataNode(source, "volume", "Chain Source"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert chain source");
        }
        if (scene.insert(xq::XQDataNode(mid, "path", "Chain Mid"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert chain mid");
        }
        if (scene.insert(xq::XQDataNode(leaf, "mesh", "Chain Leaf"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert chain leaf");
        }
        if (scene.link_derived(source, mid) != xq::XQScene::RelationResult::Linked
            || scene.link_derived(mid, leaf) != xq::XQScene::RelationResult::Linked) {
            return fail("link transitive chain");
        }
        if (scene.mark_source_changed(source) != 2) {
            return fail("mark source changed reports all transitive stale nodes");
        }
        if (!scene.is_stale(mid) || !scene.is_stale(leaf)) {
            return fail("chain mid and leaf are stale after source change");
        }
        if (scene.is_stale(source)) {
            return fail("chain source itself is not stale");
        }
        if (scene.stale_reason(mid) != xq::XQScene::StaleReason::SourceChanged
            || scene.stale_reason(leaf) != xq::XQScene::StaleReason::SourceChanged) {
            return fail("chain stale reasons are source changed");
        }
        if (scene.mark_source_changed(source) != 0) {
            return fail("repeated transitive mark does not recount stale nodes");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId source(1101);
        const xq::NodeId left(1102);
        const xq::NodeId right(1103);
        const xq::NodeId joined(1104);

        if (scene.insert(xq::XQDataNode(source, "volume", "Diamond Source"))
            != xq::XQScene::InsertResult::Inserted
            || scene.insert(xq::XQDataNode(left, "path", "Diamond Left"))
                   != xq::XQScene::InsertResult::Inserted
            || scene.insert(xq::XQDataNode(right, "segmentation", "Diamond Right"))
                   != xq::XQScene::InsertResult::Inserted
            || scene.insert(xq::XQDataNode(joined, "mesh", "Diamond Joined"))
                   != xq::XQScene::InsertResult::Inserted) {
            return fail("insert diamond nodes");
        }
        if (scene.link_derived(source, left) != xq::XQScene::RelationResult::Linked
            || scene.link_derived(source, right) != xq::XQScene::RelationResult::Linked
            || scene.link_derived(left, joined) != xq::XQScene::RelationResult::Linked
            || scene.link_derived(right, joined) != xq::XQScene::RelationResult::Linked) {
            return fail("link diamond relations");
        }
        if (scene.mark_source_changed(source) != 3) {
            return fail("diamond propagation counts joined node once");
        }
        if (!scene.is_stale(left) || !scene.is_stale(right) || !scene.is_stale(joined)) {
            return fail("diamond all downstream nodes stale");
        }
        if (scene.is_stale(source)) {
            return fail("diamond source itself is not stale");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId source(801);
        const xq::NodeId derived(802);

        if (scene.insert(xq::XQDataNode(source, "volume", "Repeated Mark Source"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert repeated mark source");
        }
        if (scene.insert(xq::XQDataNode(derived, "mesh", "Repeated Mark Derived"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert repeated mark derived");
        }
        if (scene.link_derived(source, derived) != xq::XQScene::RelationResult::Linked) {
            return fail("link repeated mark relation");
        }
        if (scene.mark_source_changed(source) != 1) {
            return fail("first repeated mark reports one new stale derived");
        }
        if (scene.mark_source_changed(source) != 0) {
            return fail("second repeated mark does not count existing stale derived");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId source(901);
        const xq::NodeId derived(902);

        if (scene.insert(xq::XQDataNode(source, "volume", "Remove Cleanup Source"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert remove cleanup source");
        }
        if (scene.insert(xq::XQDataNode(derived, "mesh", "Remove Cleanup Derived"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert remove cleanup derived");
        }
        if (scene.link_derived(source, derived) != xq::XQScene::RelationResult::Linked) {
            return fail("link remove cleanup relation");
        }
        if (scene.remove(derived) != xq::XQScene::RemoveResult::Removed) {
            return fail("remove linked derived reports removed");
        }
        if (scene.mark_source_changed(source) != 0) {
            return fail("removed derived is not marked through stale relation");
        }
        if (scene.find(derived) != nullptr) {
            return fail("removed derived returns nullptr");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId sourceA(1201);
        const xq::NodeId derivedA(1202);
        const xq::NodeId sourceB(1203);
        const xq::NodeId derivedB(1204);
        scene.insert(xq::XQDataNode(sourceA, "path", "Snapshot Source A"));
        scene.insert(xq::XQDataNode(derivedA, "profile", "Snapshot Derived A"));
        scene.insert(xq::XQDataNode(sourceB, "path", "Snapshot Source B"));
        scene.insert(xq::XQDataNode(derivedB, "profile", "Snapshot Derived B"));
        scene.link_derived(sourceA, derivedA);
        scene.link_derived(sourceB, derivedB);

        scene.mark_source_changed(sourceA);
        const xq::XQScene::StaleSnapshot onlyA = scene.stale_snapshot();
        scene.mark_source_changed(sourceB);
        if (!scene.is_stale(derivedA) || !scene.is_stale(derivedB)) {
            return fail("snapshot fixture has two stale branches");
        }
        if (!scene.restore_stale_snapshot(onlyA)
            || !scene.is_stale(derivedA) || scene.is_stale(derivedB)
            || scene.stale_snapshot() != onlyA) {
            return fail("restore stale snapshot replaces the full stale set");
        }

        const xq::XQScene::StaleSnapshot beforeInvalid = scene.stale_snapshot();
        xq::XQScene::StaleSnapshot missingNode = beforeInvalid;
        missingNode[xq::NodeId(9999)] = xq::XQScene::StaleReason::SourceChanged;
        if (scene.restore_stale_snapshot(missingNode)
            || scene.stale_snapshot() != beforeInvalid) {
            return fail("missing-node stale snapshot fails atomically");
        }
        xq::XQScene::StaleSnapshot noneReason = beforeInvalid;
        noneReason[sourceA] = xq::XQScene::StaleReason::None;
        if (scene.restore_stale_snapshot(noneReason)
            || scene.stale_snapshot() != beforeInvalid) {
            return fail("None stale reason fails atomically");
        }

        if (!scene.restore_stale_snapshot(xq::XQScene::StaleSnapshot())
            || !scene.stale_snapshot().empty()) {
            return fail("empty stale snapshot clears stale state");
        }
    }

    return 0;
}
