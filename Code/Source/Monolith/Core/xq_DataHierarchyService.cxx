#include "xq_DataHierarchyService.h"

namespace xq::core
{

namespace
{

constexpr const char* kRootId = "root";

} // namespace

DataHierarchyService::DataHierarchyService(QObject* parent)
    : QObject(parent)
{
    DataHierarchyNode root;
    root.Id = QString::fromLatin1(kRootId);
    root.DisplayName = QStringLiteral("Project");
    root.Kind = DataHierarchyNodeKind::Folder;
    m_Nodes.append(root);
}

QString DataHierarchyService::RootId() const
{
    return QString::fromLatin1(kRootId);
}

QVector<DataHierarchyNode> DataHierarchyService::Nodes() const
{
    return m_Nodes;
}

const DataHierarchyNode* DataHierarchyService::FindNode(const QString& id) const
{
    const QString normalizedId = NormalizedId(id);
    if (normalizedId.isEmpty())
        return nullptr;

    for (const auto& node : m_Nodes)
    {
        if (node.Id == normalizedId)
            return &node;
    }

    return nullptr;
}

QVector<DataHierarchyNode> DataHierarchyService::ChildrenOf(
    const QString& parentId) const
{
    const QString normalizedParentId = NormalizedId(parentId);
    QVector<DataHierarchyNode> children;
    for (const auto& node : m_Nodes)
    {
        if (node.ParentId == normalizedParentId)
            children.append(node);
    }

    return children;
}

void DataHierarchyService::ReplaceWith(const DataHierarchyService& other)
{
    m_Nodes = other.m_Nodes;
    emit NodesChanged();
}

bool DataHierarchyService::AddFolder(const QString& id,
                                     const QString& parentId,
                                     const QString& displayName,
                                     QString* errorMessage)
{
    DataHierarchyNode node;
    node.Id = id;
    node.ParentId = parentId;
    node.DisplayName = displayName;
    node.Kind = DataHierarchyNodeKind::Folder;
    return AddNode(node, errorMessage);
}

bool DataHierarchyService::AddDataEntry(const QString& id,
                                        const QString& parentId,
                                        const QString& dataCatalogEntryId,
                                        const QString& displayName,
                                        QString* errorMessage)
{
    DataHierarchyNode node;
    node.Id = id;
    node.ParentId = parentId;
    node.DisplayName = displayName;
    node.DataCatalogEntryId = dataCatalogEntryId;
    node.Kind = DataHierarchyNodeKind::DataEntry;
    return AddNode(node, errorMessage);
}

bool DataHierarchyService::RenameDataEntriesForCatalogEntry(
    const QString& dataCatalogEntryId,
    const QString& displayName,
    QString* errorMessage)
{
    const QString normalizedEntryId = dataCatalogEntryId.trimmed();
    if (normalizedEntryId.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data catalog entry id is required."));
        return false;
    }

    const QString normalizedDisplayName = displayName.trimmed();
    if (normalizedDisplayName.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Hierarchy display name is required."));
        return false;
    }

    bool renamed = false;
    for (auto& node : m_Nodes)
    {
        if (node.Kind == DataHierarchyNodeKind::DataEntry &&
            node.DataCatalogEntryId == normalizedEntryId)
        {
            node.DisplayName = normalizedDisplayName;
            renamed = true;
        }
    }

    if (!renamed)
    {
        SetError(errorMessage,
                 QStringLiteral("Data hierarchy entry was not found."));
        return false;
    }

    SetError(errorMessage, QString());
    emit NodesChanged();
    return true;
}

bool DataHierarchyService::RemoveDataEntriesForCatalogEntry(
    const QString& dataCatalogEntryId,
    QString* errorMessage)
{
    const QString normalizedEntryId = dataCatalogEntryId.trimmed();
    if (normalizedEntryId.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data catalog entry id is required."));
        return false;
    }

    bool removed = false;
    for (int i = m_Nodes.size() - 1; i >= 0; --i)
    {
        const auto& node = m_Nodes.at(i);
        if (node.Kind == DataHierarchyNodeKind::DataEntry &&
            node.DataCatalogEntryId == normalizedEntryId)
        {
            m_Nodes.removeAt(i);
            removed = true;
        }
    }

    if (!removed)
    {
        SetError(errorMessage,
                 QStringLiteral("Data hierarchy entry was not found."));
        return false;
    }

    SetError(errorMessage, QString());
    emit NodesChanged();
    return true;
}

bool DataHierarchyService::AddNode(DataHierarchyNode node,
                                   QString* errorMessage)
{
    node.Id = NormalizedId(node.Id);
    node.ParentId = NormalizedId(node.ParentId);
    node.DisplayName = node.DisplayName.trimmed();
    node.DataCatalogEntryId = node.DataCatalogEntryId.trimmed();

    if (node.Id.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Hierarchy node id is required."));
        return false;
    }

    if (FindNode(node.Id) != nullptr)
    {
        SetError(errorMessage, QStringLiteral("Duplicate hierarchy node id."));
        return false;
    }

    if (node.ParentId.isEmpty() || FindNode(node.ParentId) == nullptr)
    {
        SetError(errorMessage,
                 QStringLiteral("Hierarchy parent id was not found."));
        return false;
    }

    if (node.DisplayName.isEmpty())
        node.DisplayName = node.Id;

    if (node.Kind == DataHierarchyNodeKind::DataEntry &&
        node.DataCatalogEntryId.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data hierarchy entry requires catalog id."));
        return false;
    }

    m_Nodes.append(node);
    SetError(errorMessage, QString());
    emit NodesChanged();
    return true;
}

QString DataHierarchyService::NormalizedId(const QString& id)
{
    return id.trimmed();
}

void DataHierarchyService::SetError(QString* errorMessage,
                                    const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
