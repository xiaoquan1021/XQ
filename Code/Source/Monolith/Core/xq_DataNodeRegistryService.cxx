#include "xq_DataNodeRegistryService.h"

namespace xq::core
{

DataNodeRegistryService::DataNodeRegistryService(QObject* parent)
    : QObject(parent)
{
}

QStringList DataNodeRegistryService::CatalogEntryIds() const
{
    return m_CatalogEntryIds;
}

mitk::DataNode::Pointer DataNodeRegistryService::FindNode(
    const QString& catalogEntryId) const
{
    const QString normalizedId = NormalizedId(catalogEntryId);
    if (normalizedId.isEmpty())
        return nullptr;

    return m_NodesByCatalogEntryId.value(normalizedId);
}

bool DataNodeRegistryService::BindNode(const QString& catalogEntryId,
                                       mitk::DataNode::Pointer node,
                                       QString* errorMessage)
{
    const QString normalizedId = NormalizedId(catalogEntryId);
    if (normalizedId.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data node catalog entry id is required."));
        return false;
    }

    if (node.IsNull())
    {
        SetError(errorMessage, QStringLiteral("Data node is required."));
        return false;
    }

    if (!m_NodesByCatalogEntryId.contains(normalizedId))
        m_CatalogEntryIds.append(normalizedId);

    m_NodesByCatalogEntryId.insert(normalizedId, node);
    SetError(errorMessage, QString());
    emit BindingsChanged();
    return true;
}

bool DataNodeRegistryService::RemoveNode(const QString& catalogEntryId,
                                         QString* errorMessage)
{
    const QString normalizedId = NormalizedId(catalogEntryId);
    if (normalizedId.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data node catalog entry id is required."));
        return false;
    }

    if (!m_NodesByCatalogEntryId.contains(normalizedId))
    {
        SetError(errorMessage,
                 QStringLiteral("Data node binding was not found."));
        return false;
    }

    m_NodesByCatalogEntryId.remove(normalizedId);
    m_CatalogEntryIds.removeAll(normalizedId);
    SetError(errorMessage, QString());
    emit BindingsChanged();
    return true;
}

void DataNodeRegistryService::Clear()
{
    if (m_CatalogEntryIds.isEmpty() && m_NodesByCatalogEntryId.isEmpty())
        return;

    m_CatalogEntryIds.clear();
    m_NodesByCatalogEntryId.clear();
    emit BindingsChanged();
}

QString DataNodeRegistryService::NormalizedId(const QString& catalogEntryId)
{
    return catalogEntryId.trimmed();
}

void DataNodeRegistryService::SetError(QString* errorMessage,
                                       const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
