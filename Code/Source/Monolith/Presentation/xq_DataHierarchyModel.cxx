#include "xq_DataHierarchyModel.h"

#include "Core/xq_DataHierarchyService.h"

#include <QHash>
#include <QVariant>

namespace xq::presentation
{

struct DataHierarchyModel::ModelNode
{
    QString Id;
    QString ParentId;
    QString DisplayName;
    ModelNode* Parent = nullptr;
    QVector<ModelNode*> Children;
};

DataHierarchyModel::DataHierarchyModel(
    xq::core::DataHierarchyService& hierarchy,
    QObject* parent)
    : QAbstractItemModel(parent)
    , m_Hierarchy(hierarchy)
{
    Rebuild();

    connect(&m_Hierarchy,
            &xq::core::DataHierarchyService::NodesChanged,
            this,
            [this]() {
                beginResetModel();
                Rebuild();
                endResetModel();
            });
}

DataHierarchyModel::~DataHierarchyModel() = default;

QModelIndex DataHierarchyModel::index(int row,
                                      int column,
                                      const QModelIndex& parent) const
{
    if (column != 0 || row < 0)
        return QModelIndex();

    const ModelNode* parentNode = NodeForIndex(parent);
    if (!parentNode || row >= parentNode->Children.size())
        return QModelIndex();

    return createIndex(row, column, parentNode->Children.at(row));
}

QModelIndex DataHierarchyModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    const auto* childNode =
        static_cast<const ModelNode*>(child.internalPointer());
    if (!childNode || !childNode->Parent || childNode->Parent == m_Root)
        return QModelIndex();

    return createIndex(RowOfNode(childNode->Parent),
                       0,
                       childNode->Parent);
}

int DataHierarchyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;

    const ModelNode* parentNode = NodeForIndex(parent);
    if (!parentNode)
        return 0;

    return parentNode->Children.size();
}

int DataHierarchyModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant DataHierarchyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto* node = static_cast<const ModelNode*>(index.internalPointer());
    if (!node)
        return QVariant();

    if (role == Qt::DisplayRole)
        return node->DisplayName;
    if (role == NodeIdRole)
        return node->Id;

    return QVariant();
}

Qt::ItemFlags DataHierarchyModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> DataHierarchyModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles.insert(NodeIdRole, QByteArrayLiteral("nodeId"));
    return roles;
}

QModelIndex DataHierarchyModel::IndexForNodeId(const QString& nodeId) const
{
    const QString normalizedNodeId = nodeId.trimmed();
    if (normalizedNodeId.isEmpty() || !m_Root)
        return QModelIndex();

    return IndexForNodeId(m_Root, normalizedNodeId);
}

void DataHierarchyModel::Rebuild()
{
    m_Nodes.clear();
    m_Root = nullptr;

    const auto hierarchyNodes = m_Hierarchy.Nodes();
    QHash<QString, ModelNode*> nodesById;
    nodesById.reserve(hierarchyNodes.size());

    for (const auto& hierarchyNode : hierarchyNodes)
    {
        auto modelNode = std::make_unique<ModelNode>();
        modelNode->Id = hierarchyNode.Id;
        modelNode->ParentId = hierarchyNode.ParentId;
        modelNode->DisplayName = hierarchyNode.DisplayName;

        ModelNode* rawNode = modelNode.get();
        m_Nodes.push_back(std::move(modelNode));
        nodesById.insert(rawNode->Id, rawNode);
    }

    m_Root = nodesById.value(m_Hierarchy.RootId(), nullptr);
    if (!m_Root)
        return;

    for (const auto& hierarchyNode : hierarchyNodes)
    {
        ModelNode* node = nodesById.value(hierarchyNode.Id, nullptr);
        if (!node || node == m_Root)
            continue;

        ModelNode* parent = nodesById.value(hierarchyNode.ParentId, nullptr);
        if (!parent)
            continue;

        node->Parent = parent;
        parent->Children.append(node);
    }
}

DataHierarchyModel::ModelNode* DataHierarchyModel::NodeForIndex(
    const QModelIndex& index) const
{
    if (!index.isValid())
        return m_Root;

    return static_cast<ModelNode*>(index.internalPointer());
}

QModelIndex DataHierarchyModel::IndexForNodeId(
    const ModelNode* parentNode,
    const QString& nodeId) const
{
    if (!parentNode)
        return QModelIndex();

    for (int row = 0; row < parentNode->Children.size(); ++row)
    {
        ModelNode* child = parentNode->Children.at(row);
        if (child->Id == nodeId)
            return createIndex(row, 0, child);

        const QModelIndex descendant = IndexForNodeId(child, nodeId);
        if (descendant.isValid())
            return descendant;
    }

    return QModelIndex();
}

int DataHierarchyModel::RowOfNode(const ModelNode* node) const
{
    if (!node || !node->Parent)
        return 0;

    return node->Parent->Children.indexOf(const_cast<ModelNode*>(node));
}

} // namespace xq::presentation
