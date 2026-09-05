#include "ui/XQSceneModel.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQScaleSlot.h"
#include "core/XQScene.h"

#include <QBrush>
#include <QColor>
#include <QIcon>
#include <QString>
#include <QVariant>

#include <iterator>
#include <utility>

namespace xq {

namespace {

// Maps a node's typed domain to its tree icon, using only the scene-tree node
// icon set (node-*.svg) shipped by the resource layer. Image / Path /
// SurfaceModel / Mesh have dedicated icons. Every other domain (group nodes,
// and any future one without a dedicated node icon) falls back to the generic
// project icon.
QIcon domainIcon(XQDomainType domain)
{
    switch (domain) {
    case XQDomainType::Image:
        return QIcon(QStringLiteral(":/xq/node-image.svg"));
    case XQDomainType::Path:
    case XQDomainType::VesselProfile:
        return QIcon(QStringLiteral(":/xq/node-path.svg"));
    case XQDomainType::SurfaceModel:
        return QIcon(QStringLiteral(":/xq/node-model.svg"));
    case XQDomainType::Mesh:
        return QIcon(QStringLiteral(":/xq/node-mesh.svg"));
    case XQDomainType::ContourGroup:
    case XQDomainType::SegmentationMask:
    case XQDomainType::SimulationCase:
    case XQDomainType::FlowResult:
    case XQDomainType::Unknown:
    case XQDomainType::AiAnalysis:
        break;
    }
    return QIcon(QStringLiteral(":/xq/node-project.svg"));
}

// The fixed root ordering: source -> derived chain, Ungrouped last (it only
// appears for legacy / unknown-domain nodes).
constexpr XQScene::Group kGroupOrder[] = {
    XQScene::Group::Images,
    XQScene::Group::Paths,
    XQScene::Group::Segmentations,
    XQScene::Group::Models,
    XQScene::Group::Meshes,
    XQScene::Group::Simulations,
    XQScene::Group::Ungrouped,
};

} // namespace

XQSceneModel::XQSceneModel(QObject* parent)
    : XQSceneModel(nullptr, parent)
{
}

XQSceneModel::XQSceneModel(const XQScene* scene, QObject* parent)
    : QAbstractItemModel(parent)
    , scene_(scene)
{
    rebuild_index();
}

const XQScene* XQSceneModel::scene() const
{
    return scene_;
}

void XQSceneModel::setScene(const XQScene* scene)
{
    beginResetModel();
    scene_ = scene;
    rebuild_index();
    endResetModel();
}

void XQSceneModel::refresh()
{
    beginResetModel();
    rebuild_index();
    endResetModel();
}

const XQDataNode* XQSceneModel::nodeForIndex(const QModelIndex& index) const
{
    return node_for_index(index);
}

QModelIndex XQSceneModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column < 0 || column >= ColumnCount || row < 0) {
        return QModelIndex();
    }

    if (!parent.isValid()) {
        // Root level: one row per group.
        if (row >= static_cast<int>(groups_.size())) {
            return QModelIndex();
        }
        return createIndex(row, column, kGroupInternalId);
    }

    // Child level: only group rows have children.
    if (!is_group_index(parent)) {
        return QModelIndex();
    }
    const int groupPos = parent.row();
    if (groupPos < 0 || groupPos >= static_cast<int>(groups_.size())) {
        return QModelIndex();
    }
    if (row >= static_cast<int>(groups_[static_cast<std::size_t>(groupPos)].node_ids.size())) {
        return QModelIndex();
    }
    return createIndex(row, column, static_cast<quintptr>(groupPos));
}

QModelIndex XQSceneModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || is_group_index(child)) {
        // Root, invalid, or a group row -> no parent.
        return QModelIndex();
    }
    // A node row: its parent is the group at internalId's position.
    const int groupPos = static_cast<int>(child.internalId());
    if (groupPos < 0 || groupPos >= static_cast<int>(groups_.size())) {
        return QModelIndex();
    }
    return createIndex(groupPos, NameColumn, kGroupInternalId);
}

int XQSceneModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(groups_.size());
    }
    if (!is_group_index(parent)) {
        // Node rows are leaves.
        return 0;
    }
    const int groupPos = parent.row();
    if (groupPos < 0 || groupPos >= static_cast<int>(groups_.size())) {
        return 0;
    }
    return static_cast<int>(groups_[static_cast<std::size_t>(groupPos)].node_ids.size());
}

int XQSceneModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant XQSceneModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this) {
        return QVariant();
    }

    // --- group rows ---
    const GroupEntry* group = group_for_index(index);
    if (group != nullptr) {
        switch (role) {
        case Qt::CheckStateRole:
            if (index.column() == NameColumn) {
                return group_check_state(*group);
            }
            return QVariant();
        case Qt::DisplayRole:
            if (index.column() == NameColumn) {
                return group_display_name(group->group);
            }
            return QVariant();
        default:
            return QVariant();
        }
    }

    // --- node rows ---
    const XQDataNode* node = node_for_index(index);
    if (node == nullptr) {
        return QVariant();
    }

    const bool stale = scene_ != nullptr && scene_->is_stale(node->id());

    switch (role) {
    case Qt::CheckStateRole:
        if (index.column() == NameColumn) {
            return nodeVisible(node->id()) ? Qt::Checked : Qt::Unchecked;
        }
        return QVariant();

    case Qt::DisplayRole:
        if (index.column() == NameColumn) {
            return QString::fromStdString(node->display_name());
        }
        return QVariant();

    case Qt::DecorationRole:
        if (index.column() == NameColumn) {
            return domainIcon(node->domainType());
        }
        return QVariant();

    case Qt::ForegroundRole:
        if (index.column() == NameColumn && stale) {
            // Deeper amber so stale rows stay legible on the light tree
            // background.
            return QBrush(QColor(0xB9, 0x6A, 0x00));
        }
        return QVariant();

    case Qt::ToolTipRole:
        if (index.column() == NameColumn) {
            QString tip = QString::fromStdString(node->display_name())
                + QStringLiteral(" [")
                + QString::fromStdString(node->domain_type())
                + QStringLiteral("]");
            tip += QStringLiteral(" (scale: ");
            if (node->hasScaleSlot()) {
                const char* token = scaleSlotToToken(node->scaleSlot().value());
                tip += token[0] == '\0'
                    ? QStringLiteral("invalid")
                    : QString::fromLatin1(token);
            } else {
                tip += QStringLiteral("unspecified");
            }
            tip += QStringLiteral(")");
            if (stale) {
                tip += QStringLiteral(" (stale)");
            }
            return tip;
        }
        return QVariant();

    default:
        return QVariant();
    }
}

Qt::ItemFlags XQSceneModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    // The visibility checkbox lives in the name column for both group rows (a
    // tri-state group toggle) and node rows.
    if (index.column() == NameColumn) {
        f |= Qt::ItemIsUserCheckable;
        if (group_for_index(index) != nullptr) {
            // A group's checkbox reports an aggregate; the view must render the
            // partially-checked state rather than cycling it into a third click
            // value.
            f |= Qt::ItemIsAutoTristate;
        }
    }
    return f;
}

bool XQSceneModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::CheckStateRole || index.column() != NameColumn) {
        return false;
    }
    const bool on = value.toInt() == Qt::Checked;

    // --- group row: toggle every child node, emitting per-child signals ---
    const GroupEntry* group = group_for_index(index);
    if (group != nullptr) {
        const int groupPos = index.row();
        for (int childRow = 0;
             childRow < static_cast<int>(group->node_ids.size());
             ++childRow) {
            const NodeId id = group->node_ids[static_cast<std::size_t>(childRow)];
            if (nodeVisible(id) == on) {
                continue; // already in the target state, no churn
            }
            visible_[id] = on;
            const QModelIndex childIdx =
                createIndex(childRow, NameColumn, static_cast<quintptr>(groupPos));
            emit dataChanged(childIdx, childIdx, {Qt::CheckStateRole});
            emit nodeVisibilityChanged(id, on);
        }
        // Repaint the group row's aggregate checkbox.
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    // --- node row ---
    const XQDataNode* node = node_for_index(index);
    if (node == nullptr) {
        return false;
    }
    const NodeId id = node->id();
    visible_[id] = on;
    emit dataChanged(index, index, {Qt::CheckStateRole});
    emit nodeVisibilityChanged(id, on);
    // The owning group's aggregate may have flipped (e.g. last child unchecked);
    // repaint it too.
    const QModelIndex groupIdx = parent(index);
    if (groupIdx.isValid()) {
        emit dataChanged(groupIdx, groupIdx, {Qt::CheckStateRole});
    }
    return true;
}

bool XQSceneModel::nodeVisible(const NodeId& id) const
{
    const auto it = visible_.find(id);
    // Every id that is currently in the scene has an explicit entry (written by
    // rebuild_index on first sight). A truly unknown id (not in the scene) falls
    // back to visible, matching the pre-tree default.
    return it == visible_.end() ? true : it->second;
}

bool XQSceneModel::hasVisibilityState(const NodeId& id) const
{
    return visible_.find(id) != visible_.end();
}

void XQSceneModel::setNodeVisibleSilently(const NodeId& id, bool on)
{
    visible_[id] = on;
    // Repaint the checkbox for this id if it is currently in the tree, plus its
    // owning group's aggregate. No nodeVisibilityChanged: the caller (render
    // scene) already knows.
    for (int groupPos = 0; groupPos < static_cast<int>(groups_.size()); ++groupPos) {
        const std::vector<NodeId>& ids =
            groups_[static_cast<std::size_t>(groupPos)].node_ids;
        for (int childRow = 0; childRow < static_cast<int>(ids.size()); ++childRow) {
            if (ids[static_cast<std::size_t>(childRow)] == id) {
                const QModelIndex idx =
                    createIndex(childRow, NameColumn, static_cast<quintptr>(groupPos));
                emit dataChanged(idx, idx, {Qt::CheckStateRole});
                const QModelIndex groupIdx =
                    createIndex(groupPos, NameColumn, kGroupInternalId);
                emit dataChanged(groupIdx, groupIdx, {Qt::CheckStateRole});
                return;
            }
        }
    }
}

QVariant XQSceneModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    if (section == NameColumn) {
        return tr("Name");
    }
    return QVariant();
}

bool XQSceneModel::is_group_index(const QModelIndex& index) const
{
    return index.isValid() && index.internalId() == kGroupInternalId;
}

const XQSceneModel::GroupEntry*
XQSceneModel::group_for_index(const QModelIndex& index) const
{
    if (!is_group_index(index) || index.model() != this) {
        return nullptr;
    }
    const int groupPos = index.row();
    if (groupPos < 0 || groupPos >= static_cast<int>(groups_.size())) {
        return nullptr;
    }
    return &groups_[static_cast<std::size_t>(groupPos)];
}

const XQDataNode* XQSceneModel::node_for_index(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this || scene_ == nullptr) {
        return nullptr;
    }
    if (is_group_index(index)) {
        // A group row is not a data node: it is not renderable and drives no
        // opacity/color. Callers rely on this null to skip presentation.
        return nullptr;
    }
    const int groupPos = static_cast<int>(index.internalId());
    if (groupPos < 0 || groupPos >= static_cast<int>(groups_.size())) {
        return nullptr;
    }
    const std::vector<NodeId>& ids =
        groups_[static_cast<std::size_t>(groupPos)].node_ids;
    const int childRow = index.row();
    if (childRow < 0 || childRow >= static_cast<int>(ids.size())) {
        return nullptr;
    }
    return scene_->find(ids[static_cast<std::size_t>(childRow)]);
}

Qt::CheckState XQSceneModel::group_check_state(const GroupEntry& entry) const
{
    if (entry.node_ids.empty()) {
        return Qt::Unchecked;
    }
    int visibleCount = 0;
    for (const NodeId& id : entry.node_ids) {
        if (nodeVisible(id)) {
            ++visibleCount;
        }
    }
    if (visibleCount == 0) {
        return Qt::Unchecked;
    }
    if (visibleCount == static_cast<int>(entry.node_ids.size())) {
        return Qt::Checked;
    }
    return Qt::PartiallyChecked;
}

QString XQSceneModel::group_display_name(XQScene::Group group) const
{
    switch (group) {
    case XQScene::Group::Images:
        return tr("Images");
    case XQScene::Group::Paths:
        return tr("Paths");
    case XQScene::Group::Segmentations:
        return tr("Segmentations");
    case XQScene::Group::Models:
        return tr("Models");
    case XQScene::Group::Meshes:
        return tr("Meshes");
    case XQScene::Group::Simulations:
        return tr("Simulations");
    case XQScene::Group::Ungrouped:
        break;
    }
    return tr("Other");
}

bool XQSceneModel::default_visible_for_domain(XQDomainType domain)
{
    // Modeling-first default: opening a project shows the image and the surface
    // model, with everything else (paths, contours, segmentations, meshes,
    // simulations, unknown) hidden so the 3D scene and slices start clean.
    switch (domain) {
    case XQDomainType::Image:
    case XQDomainType::SurfaceModel:
        return true;
    case XQDomainType::Path:
    case XQDomainType::VesselProfile:
    case XQDomainType::ContourGroup:
    case XQDomainType::SegmentationMask:
    case XQDomainType::Mesh:
    case XQDomainType::SimulationCase:
    case XQDomainType::FlowResult:
    case XQDomainType::AiAnalysis:
    case XQDomainType::Unknown:
        return false;
    }
    return false;
}

void XQSceneModel::rebuild_index()
{
    groups_.clear();
    if (scene_ == nullptr) {
        // A null scene drops every node; forget stale visibility state too.
        visible_.clear();
        return;
    }

    // Bucket the scene's nodes by group, defaulting a newly seen id's visibility
    // by its domain. visit_nodes iterates in NodeId order, so children stay in a
    // stable, id-sorted order within each group.
    std::vector<std::vector<NodeId>> buckets(std::size(kGroupOrder));
    std::unordered_map<NodeId, bool> kept;
    scene_->visit_nodes([&](const XQDataNode& node) {
        const XQDomainType domain = node.domainType();
        const XQScene::Group group = XQScene::groupForDomain(domain);
        for (std::size_t g = 0; g < std::size(kGroupOrder); ++g) {
            if (kGroupOrder[g] == group) {
                buckets[g].push_back(node.id());
                break;
            }
        }
        // Preserve a surviving id's visibility; default a first-seen id by domain
        // so the map always carries an explicit state for every live node.
        const NodeId id = node.id();
        const auto it = visible_.find(id);
        kept.emplace(id, it != visible_.end()
                             ? it->second
                             : default_visible_for_domain(domain));
    });

    // Drop ids that left the scene (kept only holds live ids) so the map does not
    // grow unbounded.
    visible_ = std::move(kept);

    // Materialize only the non-empty groups, in the fixed order.
    for (std::size_t g = 0; g < std::size(kGroupOrder); ++g) {
        if (!buckets[g].empty()) {
            groups_.push_back(GroupEntry{kGroupOrder[g], std::move(buckets[g])});
        }
    }
}

} // namespace xq
