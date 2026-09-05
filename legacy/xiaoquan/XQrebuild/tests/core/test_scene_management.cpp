#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
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
    xq::XQScene scene;
    const xq::NodeId id(101);

    if (scene.find(id) != nullptr) {
        return fail("missing NodeId returns nullptr");
    }

    const xq::XQDataNode node(id, "volume", "Brain Volume");
    if (scene.insert(node) != xq::XQScene::InsertResult::Inserted) {
        return fail("first insert reports inserted");
    }

    const xq::XQDataNode* found = scene.find(id);
    if (!node_matches(found, id, "volume", "Brain Volume")) {
        return fail("find returns inserted node");
    }

    const xq::XQScene& const_scene = scene;
    if (const_scene.find(id) != found) {
        return fail("const find returns the same stored node");
    }

    const xq::XQDataNode duplicate(id, "mesh", "Duplicate");
    if (scene.insert(duplicate) != xq::XQScene::InsertResult::DuplicateNodeId) {
        return fail("duplicate NodeId reports deterministic diagnostic");
    }
    if (!node_matches(scene.find(id), id, "volume", "Brain Volume")) {
        return fail("duplicate NodeId does not replace existing node");
    }

    xq::XQProject project;
    xq::XQScene* project_scene = &project.scene();
    if (project_scene != &project.scene()) {
        return fail("project scene accessor returns one scene instance");
    }

    const xq::XQProject& const_project = project;
    if (&const_project.scene() != project_scene) {
        return fail("const project scene accessor returns the same scene instance");
    }

    const xq::NodeId project_node_id(202);
    if (project.scene().insert(xq::XQDataNode(project_node_id, "case", "Project Case"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("project scene accepts nodes");
    }
    if (project_scene->find(project_node_id) == nullptr) {
        return fail("project owns the scene used by its accessor");
    }

    return 0;
}
