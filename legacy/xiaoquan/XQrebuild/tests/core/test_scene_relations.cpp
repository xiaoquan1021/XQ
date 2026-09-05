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

    return 0;
}
