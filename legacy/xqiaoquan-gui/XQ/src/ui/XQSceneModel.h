#ifndef XQ_UI_XQ_SCENE_MODEL_H
#define XQ_UI_XQ_SCENE_MODEL_H

#include "core/NodeId.h"
#include "core/XQScene.h"

#include <QAbstractItemModel>

#include <unordered_map>
#include <vector>

namespace xq {

class XQDataNode;
class XQScene;

class XQSceneModel : public QAbstractItemModel {
    Q_OBJECT

public:
    // Two-level tree (MITK-style): root rows are non-empty type groups, child
    // rows are the nodes within a group. The tree is single-column: the name
    // column carries the visibility checkbox (CheckStateRole), the domain icon,
    // the full display name, an amber tint for stale nodes, and a tooltip that
    // spells out "name [type] (scale: token|unspecified)" -- so domain type,
    // biological scale identity, and staleness stay reachable without stealing
    // width from the narrow dock. The header is hidden.
    // Consumers reference these enum constants, never raw indices.
    enum Column {
        NameColumn = 0,
        ColumnCount = 1
    };

    explicit XQSceneModel(QObject* parent = nullptr);
    explicit XQSceneModel(const XQScene* scene, QObject* parent = nullptr);

    const XQScene* scene() const;
    void setScene(const XQScene* scene);
    void refresh();

    // Resolves the data node behind a model index (e.g. for selection-driven
    // rendering). Returns null for an invalid / foreign index or a null scene.
    // Public wrapper over the private mapping, added without changing existing
    // behaviour.
    const XQDataNode* nodeForIndex(const QModelIndex& index) const;

    // Visibility checkbox state (owned in the UI layer, design §3.4 -- core keeps
    // no such field). A never-seen id falls back to a modeling-first per-domain
    // default (Image + SurfaceModel visible, everything else hidden);
    // refresh()/setScene() preserve the state of ids that survive.
    bool nodeVisible(const NodeId& id) const;
    // True when this id has an explicit stored visibility state (the user toggled
    // it, or it was set silently). False for a never-seen id -- lets the caller
    // distinguish "first sight, apply the render-side default" from "restore the
    // known UI state".
    bool hasVisibilityState(const NodeId& id) const;
    // Sets a node's checkbox state without emitting nodeVisibilityChanged (used
    // when the render scene, not the user, is the source of truth -- e.g. a mesh
    // that mounts hidden by default). Still emits dataChanged so the checkbox
    // repaints.
    void setNodeVisibleSilently(const NodeId& id, bool on);

    QModelIndex index(int row,
                      int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index,
                 const QVariant& value,
                 int role = Qt::EditRole) override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    // Emitted when the user toggles a node's visibility checkbox (setData with
    // Qt::CheckStateRole). The main window connects this to the render scene.
    void nodeVisibilityChanged(const NodeId& id, bool visible);

private:
    // One root row per non-empty type group, in the fixed order Images -> Paths
    // -> Segmentations -> Models -> Meshes -> Simulations -> Ungrouped.
    struct GroupEntry {
        XQScene::Group group;
        std::vector<NodeId> node_ids;
    };

    // Index encoding (internalId, a quintptr):
    //   group row -> kGroupInternalId ; its model row is the group's position in
    //                groups_.
    //   node row  -> the owning group's position in groups_ ; its model row is the
    //                node's position within that group.
    static constexpr quintptr kGroupInternalId =
        static_cast<quintptr>(-1);
    bool is_group_index(const QModelIndex& index) const;
    const GroupEntry* group_for_index(const QModelIndex& index) const;
    const XQDataNode* node_for_index(const QModelIndex& index) const;
    // Aggregate tri-state for a group row: Checked when every child is visible,
    // Unchecked when none are, PartiallyChecked otherwise. Empty group -> Unchecked.
    Qt::CheckState group_check_state(const GroupEntry& entry) const;
    // Localized display name for a group (tr'd in XQSceneModel context).
    QString group_display_name(XQScene::Group group) const;
    void rebuild_index();
    // The default visibility a node gets the first time it is seen: modeling-first
    // (Image + SurfaceModel visible, everything else hidden).
    static bool default_visible_for_domain(XQDomainType domain);

    const XQScene* scene_;
    std::vector<GroupEntry> groups_;
    // Per-node visibility checkbox state. An id absent from the map has never been
    // seen; nodeVisible() then falls back to default_visible_for_domain().
    std::unordered_map<NodeId, bool> visible_;
};

} // namespace xq

#endif // XQ_UI_XQ_SCENE_MODEL_H
