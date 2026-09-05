#include <ui/XQSceneModel.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQScene.h>

#include <QCoreApplication>
#include <QMetaType>
#include <QModelIndex>
#include <QVariant>

#include <cstdio>
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

std::vector<xq::NodeId> scene_node_ids(const xq::XQScene& scene)
{
    std::vector<xq::NodeId> ids;
    scene.visit_nodes([&ids](const xq::XQDataNode& node) {
        ids.push_back(node.id());
    });
    return ids;
}

int verify_model_matches_scene(const xq::XQScene& scene, const xq::XQSceneModel& model)
{
    const std::vector<xq::NodeId> ids = scene_node_ids(scene);

    if (model.rowCount(QModelIndex()) != static_cast<int>(ids.size())) {
        return fail("root rowCount matches scene node count");
    }
    if (model.columnCount(QModelIndex()) != xq::XQSceneModel::ColumnCount) {
        return fail("columnCount exposes display name, domain type, and stale columns");
    }

    for (int row = 0; row < static_cast<int>(ids.size()); ++row) {
        const QModelIndex first_column = model.index(row, 0, QModelIndex());
        if (!first_column.isValid()) {
            return fail("node index is valid");
        }
        if (model.parent(first_column).isValid()) {
            return fail("flat node parent is root");
        }
        if (model.rowCount(first_column) != 0) {
            return fail("flat node has no children");
        }

        const xq::XQDataNode* node = scene.find(ids[static_cast<std::size_t>(row)]);
        if (node == nullptr) {
            return fail("expected scene node is present");
        }

        const QModelIndex display_index =
            model.index(row, xq::XQSceneModel::DisplayNameColumn, QModelIndex());
        const QModelIndex domain_index =
            model.index(row, xq::XQSceneModel::DomainTypeColumn, QModelIndex());
        const QModelIndex stale_index =
            model.index(row, xq::XQSceneModel::StaleColumn, QModelIndex());

        if (model.data(display_index, Qt::DisplayRole).toString().toStdString()
            != node->display_name()) {
            return fail("display name data matches scene node");
        }
        if (model.data(domain_index, Qt::DisplayRole).toString().toStdString()
            != node->domain_type()) {
            return fail("domain type data matches scene node");
        }

        const QVariant stale_data = model.data(stale_index, Qt::DisplayRole);
        if (stale_data.metaType().id() != QMetaType::Bool) {
            return fail("stale data is a bool");
        }
        if (stale_data.toBool() != scene.is_stale(node->id())) {
            return fail("stale data matches scene state");
        }
    }

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::XQScene scene;
    const xq::NodeId source_id(10);
    const xq::NodeId derived_id(20);
    const xq::NodeId case_id(30);

    if (scene.insert(xq::XQDataNode(source_id, "volume", "Source Volume"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert source node");
    }
    if (scene.insert(xq::XQDataNode(derived_id, "mesh", "Derived Mesh"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert derived node");
    }
    if (scene.insert(xq::XQDataNode(case_id, "case", "Simulation Case"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert case node");
    }
    if (scene.link_derived(source_id, derived_id) != xq::XQScene::RelationResult::Linked) {
        return fail("link source to derived");
    }
    if (scene.mark_source_changed(source_id) != 1) {
        return fail("mark derived node stale");
    }

    const std::size_t node_count_before_model = count_nodes(scene);
    xq::XQSceneModel model(&scene);

    if (verify_model_matches_scene(scene, model) != 0) {
        return 1;
    }
    if (count_nodes(scene) != node_count_before_model) {
        return fail("model observation does not modify scene node count");
    }

    const xq::NodeId path_id(40);
    if (scene.insert(xq::XQDataNode(path_id, "path", "Centerline Path"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert node before refresh");
    }

    const std::size_t node_count_before_refresh = count_nodes(scene);
    model.refresh();

    if (verify_model_matches_scene(scene, model) != 0) {
        return 1;
    }
    if (count_nodes(scene) != node_count_before_refresh) {
        return fail("refresh does not modify scene node count");
    }

    return 0;
}
