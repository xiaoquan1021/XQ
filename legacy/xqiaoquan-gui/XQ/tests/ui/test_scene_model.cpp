#include <ui/XQSceneModel.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQScaleSlot.h>
#include <core/XQScene.h>

#include <QCoreApplication>
#include <QMetaType>
#include <QModelIndex>
#include <QObject>
#include <QVariant>

#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

int fail(const char* check)
{
    std::fprintf(stderr, "FAIL: %s\n", check);
    return 1;
}

std::size_t count_nodes(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) {
        ++count;
    });
    return count;
}

// Number of non-empty type groups in the scene (the model's root row count).
int group_count(const xq::XQScene& scene)
{
    std::set<xq::XQScene::Group> groups;
    scene.visit_nodes([&groups](const xq::XQDataNode& node) {
        groups.insert(xq::XQScene::groupForDomain(node.domainType()));
    });
    return static_cast<int>(groups.size());
}

// Finds the root group row whose display name matches, then the child row within
// it whose display name matches. Returns an invalid index if not found.
QModelIndex find_node(const xq::XQSceneModel& model, const char* nodeName)
{
    for (int g = 0; g < model.rowCount(QModelIndex()); ++g) {
        const QModelIndex groupIdx =
            model.index(g, xq::XQSceneModel::NameColumn, QModelIndex());
        for (int n = 0; n < model.rowCount(groupIdx); ++n) {
            const QModelIndex childIdx =
                model.index(n, xq::XQSceneModel::NameColumn, groupIdx);
            if (childIdx.data(Qt::DisplayRole).toString().toStdString() == nodeName) {
                return childIdx;
            }
        }
    }
    return QModelIndex();
}

// Verifies the tree shape (root = non-empty groups, children = nodes) and that
// node rows expose their scene data through the single name column: name via
// DisplayRole, domain type and biological scale via tooltip tokens, staleness
// via the amber ForegroundRole brush.
int verify_tree_matches_scene(const xq::XQScene& scene, const xq::XQSceneModel& model)
{
    if (model.rowCount(QModelIndex()) != group_count(scene)) {
        return fail("root rowCount matches the non-empty group count");
    }
    // Single-column tree: only the name column survives.
    if (xq::XQSceneModel::ColumnCount != 1
        || model.columnCount(QModelIndex()) != 1) {
        return fail("model is single-column (name only)");
    }
    if (model.headerData(xq::XQSceneModel::NameColumn,
                         Qt::Horizontal,
                         Qt::DisplayRole).toString().toStdString() != "Name") {
        return fail("scene model exposes the name header");
    }

    // Every child of every group row must round-trip to a scene node whose data
    // matches; the sum of child counts must equal the scene node count.
    int totalChildren = 0;
    for (int g = 0; g < model.rowCount(QModelIndex()); ++g) {
        const QModelIndex groupIdx =
            model.index(g, xq::XQSceneModel::NameColumn, QModelIndex());
        if (!groupIdx.isValid()) {
            return fail("group index is valid");
        }
        if (model.parent(groupIdx).isValid()) {
            return fail("group parent is root");
        }
        // A group row is not a data node (not renderable).
        if (model.nodeForIndex(groupIdx) != nullptr) {
            return fail("group row has no backing data node");
        }
        const int children = model.rowCount(groupIdx);
        if (children == 0) {
            return fail("a materialized group is non-empty");
        }
        totalChildren += children;

        for (int n = 0; n < children; ++n) {
            const QModelIndex nameIdx =
                model.index(n, xq::XQSceneModel::NameColumn, groupIdx);
            if (!nameIdx.isValid()) {
                return fail("node index is valid");
            }
            // A single-column tree: only column 0 exists; column 1 is invalid.
            if (model.index(n, 1, groupIdx).isValid()) {
                return fail("no second column exists on a node row");
            }
            if (model.parent(nameIdx) != groupIdx) {
                return fail("node parent is its group row");
            }
            if (model.rowCount(nameIdx) != 0) {
                return fail("node row is a leaf");
            }
            const xq::XQDataNode* node = model.nodeForIndex(nameIdx);
            if (node == nullptr) {
                return fail("node row resolves to a scene node");
            }
            if (model.data(nameIdx, Qt::DisplayRole).toString().toStdString()
                != node->display_name()) {
                return fail("display name data matches scene node");
            }
            // Domain type moved off its own column onto the tooltip token
            // "[<type>]"; nodeForIndex still exposes the typed node, and the
            // tooltip must spell the type out so it stays user-reachable.
            const std::string tip =
                model.data(nameIdx, Qt::ToolTipRole).toString().toStdString();
            const std::string typeToken = "[" + node->domain_type() + "]";
            if (tip.find(typeToken) == std::string::npos) {
                return fail("node tooltip carries the domain-type token");
            }
            const std::string scaleToken = node->hasScaleSlot()
                ? xq::scaleSlotToToken(node->scaleSlot().value())
                : "unspecified";
            const std::string scaleProjection =
                "(scale: " + scaleToken + ")";
            if (tip.find(scaleProjection) == std::string::npos) {
                return fail("node tooltip reports scale or unspecified without a default");
            }
            // Staleness moved off its own column onto the amber ForegroundRole
            // brush: stale nodes tint, fresh nodes do not. Cross-check against
            // the scene so the coverage the stale column carried is preserved.
            const QVariant fg = model.data(nameIdx, Qt::ForegroundRole);
            const bool tinted = fg.isValid();
            if (tinted != scene.is_stale(node->id())) {
                return fail("stale nodes tint amber, fresh nodes do not");
            }
        }
    }
    if (totalChildren != static_cast<int>(count_nodes(scene))) {
        return fail("child rows across all groups sum to the scene node count");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Three groups: Paths (one vessel profile), Models (two surface models),
    // and Meshes (one mesh). The mesh is derived from surface_a and marked
    // stale, exercising the stale projection.
    xq::XQScene scene;
    const xq::NodeId surface_a(10);
    const xq::NodeId surface_b(20);
    const xq::NodeId profile_id(25);
    const xq::NodeId mesh_id(30);

    if (scene.insert(xq::XQDataNode(surface_a, xq::XQDomainType::SurfaceModel,
                                    "Surface A", nullptr))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert surface A");
    }
    xq::XQDataNode surfaceBNode(surface_b, xq::XQDomainType::SurfaceModel,
                                "Surface B", nullptr);
    surfaceBNode.setScaleSlot(xq::ScaleSlot::Micro);
    if (scene.insert(surfaceBNode)
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert surface B");
    }
    if (scene.insert(xq::XQDataNode(profile_id, xq::XQDomainType::VesselProfile,
                                    "Vessel Profile", nullptr))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert vessel profile");
    }
    if (scene.insert(xq::XQDataNode(mesh_id, xq::XQDomainType::Mesh,
                                    "Derived Mesh", nullptr))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert mesh");
    }
    if (scene.link_derived(surface_a, mesh_id) != xq::XQScene::RelationResult::Linked) {
        return fail("link surface to mesh");
    }
    if (scene.mark_source_changed(surface_a) != 1) {
        return fail("mark mesh stale");
    }

    const std::size_t node_count_before_model = count_nodes(scene);
    xq::XQSceneModel model(&scene);

    if (verify_tree_matches_scene(scene, model) != 0) {
        return 1;
    }
    if (count_nodes(scene) != node_count_before_model) {
        return fail("model observation does not modify scene node count");
    }

    // Root has exactly three group rows (Paths, Models, Meshes) in fixed order.
    if (model.rowCount(QModelIndex()) != 3) {
        return fail("scene with three groups has three root rows");
    }
    const QModelIndex pathsGroup =
        model.index(0, xq::XQSceneModel::NameColumn, QModelIndex());
    const QModelIndex modelsGroup =
        model.index(1, xq::XQSceneModel::NameColumn, QModelIndex());
    const QModelIndex meshesGroup =
        model.index(2, xq::XQSceneModel::NameColumn, QModelIndex());
    if (model.rowCount(pathsGroup) != 1) {
        return fail("Paths group holds the vessel profile");
    }
    if (model.rowCount(modelsGroup) != 2) {
        return fail("Models group holds the two surface models");
    }
    if (model.rowCount(meshesGroup) != 1) {
        return fail("Meshes group holds the one mesh");
    }

    // --- default visibility is modeling-first (per-domain) ---
    // SurfaceModel nodes default Checked; the Mesh node defaults Unchecked.
    const QModelIndex surfA = find_node(model, "Surface A");
    const QModelIndex surfB = find_node(model, "Surface B");
    const QModelIndex profileIdx = find_node(model, "Vessel Profile");
    const QModelIndex meshIdx = find_node(model, "Derived Mesh");
    if (!surfA.isValid() || !surfB.isValid() || !profileIdx.isValid()
        || !meshIdx.isValid()) {
        return fail("all four nodes are locatable in the tree");
    }
    if (model.parent(profileIdx) != pathsGroup) {
        return fail("VesselProfile is projected into the Paths group");
    }
    const std::string profileTip =
        model.data(profileIdx, Qt::ToolTipRole).toString().toStdString();
    if (profileTip.find("[vessel_profile]") == std::string::npos) {
        return fail("VesselProfile tooltip keeps its independent domain token");
    }
    if (model.data(surfA, Qt::CheckStateRole).toInt() != Qt::Checked
        || model.data(surfB, Qt::CheckStateRole).toInt() != Qt::Checked) {
        return fail("SurfaceModel nodes default to Checked (modeling-first)");
    }
    if (model.data(profileIdx, Qt::CheckStateRole).toInt() != Qt::Unchecked
        || model.data(meshIdx, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("VesselProfile and Mesh nodes default to Unchecked");
    }
    if (!model.nodeVisible(surface_a) || !model.nodeVisible(surface_b)) {
        return fail("nodeVisible true for SurfaceModel defaults");
    }
    if (model.nodeVisible(profile_id) || model.nodeVisible(mesh_id)) {
        return fail("nodeVisible false for VesselProfile and Mesh defaults");
    }

    // --- group check column: user-checkable + tri-state aggregate ---
    if ((model.flags(modelsGroup) & Qt::ItemIsUserCheckable) == 0) {
        return fail("group row is user-checkable");
    }
    // Both surface models are visible by default -> the Models group is Checked.
    if (model.data(modelsGroup, Qt::CheckStateRole).toInt() != Qt::Checked) {
        return fail("all-visible group aggregates to Checked");
    }
    // The profile and mesh are hidden by default, so both groups are Unchecked.
    if (model.data(pathsGroup, Qt::CheckStateRole).toInt() != Qt::Unchecked
        || model.data(meshesGroup, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("all-hidden groups aggregate to Unchecked");
    }
    // A group row backs no data node.
    if (model.nodeForIndex(modelsGroup) != nullptr) {
        return fail("group row nodeForIndex is null");
    }

    // --- group setData(Unchecked) hides every child, one signal per child ---
    int signalCount = 0;
    std::vector<xq::NodeId> signalIds;
    std::vector<bool> signalStates;
    QObject::connect(&model, &xq::XQSceneModel::nodeVisibilityChanged,
                     [&](const xq::NodeId& id, bool visible) {
                         ++signalCount;
                         signalIds.push_back(id);
                         signalStates.push_back(visible);
                     });

    if (!model.setData(modelsGroup, Qt::Unchecked, Qt::CheckStateRole)) {
        return fail("group setData(Unchecked) is accepted");
    }
    if (signalCount != 2) {
        return fail("group toggle emits one signal per (changed) child");
    }
    for (bool state : signalStates) {
        if (state) {
            return fail("group Unchecked carries visible=false to children");
        }
    }
    if (model.data(surfA, Qt::CheckStateRole).toInt() != Qt::Unchecked
        || model.data(surfB, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("group Unchecked flips every child to Unchecked");
    }
    if (model.data(modelsGroup, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("group aggregate reads back Unchecked");
    }

    // --- checking one child back -> group becomes PartiallyChecked ---
    signalCount = 0;
    if (!model.setData(surfA, Qt::Checked, Qt::CheckStateRole)) {
        return fail("child setData(Checked) is accepted");
    }
    if (signalCount != 1) {
        return fail("child toggle emits exactly one signal");
    }
    if (model.data(modelsGroup, Qt::CheckStateRole).toInt() != Qt::PartiallyChecked) {
        return fail("one-of-two visible aggregates to PartiallyChecked");
    }

    // --- refresh preserves per-node toggled state ---
    model.refresh();
    const QModelIndex surfA2 = find_node(model, "Surface A");
    const QModelIndex surfB2 = find_node(model, "Surface B");
    if (!surfA2.isValid() || !surfB2.isValid()) {
        return fail("nodes survive refresh");
    }
    if (model.data(surfA2, Qt::CheckStateRole).toInt() != Qt::Checked
        || model.data(surfB2, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("refresh preserves each child's toggled visibility");
    }

    // --- there is no column beyond the name column ---
    // The old Type/Status columns are gone, so an index at column 1 cannot even
    // be constructed; setData therefore has no non-name column to reject on.
    const QModelIndex secondColumn =
        model.index(surfA2.row(), 1, model.parent(surfA2));
    if (secondColumn.isValid()) {
        return fail("no column exists past the name column");
    }
    if (model.setData(secondColumn, Qt::Checked, Qt::CheckStateRole)) {
        return fail("setData on an invalid index is rejected");
    }

    return 0;
}
