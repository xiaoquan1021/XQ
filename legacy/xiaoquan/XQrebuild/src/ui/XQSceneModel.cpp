#include "ui/XQSceneModel.h"

#include "core/XQDataNode.h"
#include "core/XQScene.h"

#include <QString>
#include <QVariant>

namespace xq {

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

QModelIndex XQSceneModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return QModelIndex();
    }
    if (row < 0 || row >= rowCount(parent)) {
        return QModelIndex();
    }
    if (column < 0 || column >= ColumnCount) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex XQSceneModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int XQSceneModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(node_ids_.size());
}

int XQSceneModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant XQSceneModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    const XQDataNode* node = node_for_index(index);
    if (node == nullptr) {
        return QVariant();
    }

    switch (index.column()) {
    case DisplayNameColumn:
        return QString::fromStdString(node->display_name());
    case DomainTypeColumn:
        return QString::fromStdString(node->domain_type());
    case StaleColumn:
        return scene_ != nullptr && scene_->is_stale(node->id());
    default:
        return QVariant();
    }
}

const XQDataNode* XQSceneModel::node_for_index(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this || scene_ == nullptr) {
        return nullptr;
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(node_ids_.size())) {
        return nullptr;
    }

    return scene_->find(node_ids_[static_cast<std::size_t>(row)]);
}

void XQSceneModel::rebuild_index()
{
    node_ids_.clear();
    if (scene_ == nullptr) {
        return;
    }

    scene_->visit_nodes([this](const XQDataNode& node) {
        node_ids_.push_back(node.id());
    });
}

} // namespace xq
