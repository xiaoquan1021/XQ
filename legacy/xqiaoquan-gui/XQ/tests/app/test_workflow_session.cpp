#include <app/XQWorkflowSession.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>

#include <QCoreApplication>

#include <cstdio>
#include <memory>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

// Builds an AddNodeCommand that inserts a fresh legacy node with the given id.
std::unique_ptr<xq::XQCommand> makeAddCommand(xq::XQScene* scene, int id)
{
    xq::XQDataNode node(xq::NodeId(id), "volume", "session-node");
    return std::make_unique<xq::AddNodeCommand>(scene, std::move(node), "Add node");
}

} // namespace

int main(int argc, char** argv)
{
    // The session owns a task runner (a QObject with a worker thread), so a
    // QCoreApplication must exist even though the command gateway is synchronous.
    QCoreApplication app(argc, argv);

    // 1. Before attach the gateway is inert: pushCommand returns false and never
    //    fires the callback.
    {
        xq::XQWorkflowSession session;
        int callbacks = 0;
        session.setSceneChangedCallback([&callbacks]() { ++callbacks; });
        xq::XQScene scene;
        CHECK(!session.pushCommand(makeAddCommand(&scene, 1)));
        CHECK(callbacks == 0);
        CHECK(scene.find(xq::NodeId(1)) == nullptr);
    }

    // 2. After attach, a successful pushCommand mutates the scene and fires the
    //    scene-changed callback exactly once.
    {
        xq::XQWorkflowSession session;
        int callbacks = 0;
        session.setSceneChangedCallback([&callbacks]() { ++callbacks; });

        xq::XQScene scene;
        xq::XQCommandStack stack;
        session.attach(&scene, &stack);

        // attach() rebuilds every compiled/enabled controller.
        CHECK(session.pathController() != nullptr);
        CHECK(session.segmentationController() != nullptr);
        CHECK(session.modelingController() != nullptr);
        CHECK(session.meshingController() != nullptr);
        // Scene-only attachment cannot publish the atomic project-level
        // node/asset/lineage bundle required by VesselProfile.
        CHECK(session.vesselProfileController() == nullptr);
        CHECK(session.pathModuleController() == nullptr);
        CHECK(session.centerlineBController() == nullptr);
#if XQ_ENABLE_FLOW
        CHECK(session.hasFlowCapability());
        CHECK(session.flowController() != nullptr);
#else
        CHECK(!session.hasFlowCapability());
        CHECK(session.flowController() == nullptr);
#endif
        CHECK(session.aiController() != nullptr);
        CHECK(session.scene() == &scene);
        CHECK(session.commandStack() == &stack);

        CHECK(session.pushCommand(makeAddCommand(&scene, 10)));
        CHECK(callbacks == 1);
        CHECK(scene.find(xq::NodeId(10)) != nullptr);

        // 3. undo() succeeds and fires the callback exactly once more, reverting
        //    the node.
        CHECK(session.undo());
        CHECK(callbacks == 2);
        CHECK(scene.find(xq::NodeId(10)) == nullptr);

        // 4. redo() succeeds, fires the callback once more, restores the node.
        CHECK(session.redo());
        CHECK(callbacks == 3);
        CHECK(scene.find(xq::NodeId(10)) != nullptr);

        // 5. A no-op undo/redo (nothing left to redo after the redo above)
        //    returns false and does NOT fire the callback -- the render-sync
        //    point only fires on a real scene change.
        const int before = callbacks;
        CHECK(!session.redo()); // redo stack is empty now
        CHECK(callbacks == before);
    }

    // 6. Runtime capability injection can disable the compiled Flow execution
    // path without affecting the non-Flow controller set or command gateway.
    {
        xq::XQWorkflowSession session(xq::WorkflowCapabilities::withoutFlow());
        xq::XQProject project;
        xq::XQCommandStack stack;
        CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
        session.attach(&project, &stack);
        xq::XQScene& scene = project.scene();

        CHECK(!session.hasFlowCapability());
        CHECK(session.flowController() == nullptr);
        CHECK(session.flowSmokeController() == nullptr);
        CHECK(session.pathController() != nullptr);
        CHECK(session.segmentationController() != nullptr);
        CHECK(session.modelingController() != nullptr);
        CHECK(session.meshingController() != nullptr);
        CHECK(session.vesselProfileController() != nullptr);
        CHECK(session.pathModuleController() != nullptr);
        CHECK(session.centerlineBController() != nullptr);
        CHECK(session.aiController() != nullptr);
        CHECK(session.pushCommand(makeAddCommand(&scene, 20)));
        CHECK(scene.find(xq::NodeId(20)) != nullptr);
    }

    std::printf("workflow session gateway checks passed\n");
    return 0;
}
