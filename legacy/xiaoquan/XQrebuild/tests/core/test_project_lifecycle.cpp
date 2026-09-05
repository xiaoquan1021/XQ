#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
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
        xq::XQProject project;

        if (project.state() != xq::XQProject::LifecycleState::Created) {
            return fail("project starts created");
        }
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open created project reports ok");
        }
        if (project.state() != xq::XQProject::LifecycleState::Open) {
            return fail("open moves project to open");
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
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open created project before duplicate open");
        }
        if (project.open() != xq::XQProject::LifecycleResult::InvalidTransition) {
            return fail("open open project reports invalid transition");
        }
        if (project.state() != xq::XQProject::LifecycleState::Open) {
            return fail("invalid duplicate open leaves project open");
        }
    }

    {
        xq::XQProject project;
        xq::XQScene* scene = &project.scene();

        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open project for scene identity");
        }
        if (&project.scene() != scene) {
            return fail("open keeps project scene identity");
        }
        if (project.close() != xq::XQProject::LifecycleResult::Ok) {
            return fail("close project for scene identity");
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
    }

    return 0;
}
