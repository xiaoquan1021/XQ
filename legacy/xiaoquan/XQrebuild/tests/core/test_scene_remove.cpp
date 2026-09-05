#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQScene.h>

#include <iostream>
#include <string>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

bool node_matches(const xq::XQDataNode* node,
                  const xq::NodeId& id,
                  const std::string& domain_type,
                  const std::string& display_name)
{
    return node != nullptr
        && node->id() == id
        && node->domain_type() == domain_type
        && node->display_name() == display_name;
}

} // namespace

int main()
{
    {
        xq::XQScene scene;
        const xq::NodeId id(301);

        if (scene.insert(xq::XQDataNode(id, "volume", "Removable Volume"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert node before remove");
        }
        if (scene.remove(id) != xq::XQScene::RemoveResult::Removed) {
            return fail("remove inserted NodeId reports removed");
        }
        if (scene.find(id) != nullptr) {
            return fail("removed NodeId returns nullptr");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId existing_id(401);
        const xq::NodeId missing_id(402);

        if (scene.insert(xq::XQDataNode(existing_id, "mesh", "Existing Mesh"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert existing node before missing remove");
        }
        if (scene.remove(missing_id) != xq::XQScene::RemoveResult::NotFound) {
            return fail("remove missing NodeId reports not found");
        }
        if (!node_matches(scene.find(existing_id), existing_id, "mesh", "Existing Mesh")) {
            return fail("remove missing NodeId does not affect existing node");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId id(501);

        if (scene.insert(xq::XQDataNode(id, "case", "Repeat Remove Case"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert node before repeated remove");
        }
        if (scene.remove(id) != xq::XQScene::RemoveResult::Removed) {
            return fail("first repeated remove reports removed");
        }
        if (scene.remove(id) != xq::XQScene::RemoveResult::NotFound) {
            return fail("second repeated remove reports not found");
        }
    }

    return 0;
}
