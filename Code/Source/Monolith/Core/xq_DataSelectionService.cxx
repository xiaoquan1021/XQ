#include "xq_DataSelectionService.h"

#include "xq_DataCatalogService.h"
#include "xq_DataHierarchyService.h"

namespace xq::core
{

DataSelectionService::DataSelectionService(DataCatalogService& dataCatalog,
                                           DataHierarchyService& dataHierarchy,
                                           QObject* parent)
    : QObject(parent)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(dataHierarchy)
{
}

bool DataSelectionService::HasSelection() const
{
    return !m_SelectedHierarchyNodeId.isEmpty() ||
           !m_SelectedCatalogEntryId.isEmpty();
}

QString DataSelectionService::SelectedHierarchyNodeId() const
{
    return m_SelectedHierarchyNodeId;
}

QString DataSelectionService::SelectedCatalogEntryId() const
{
    return m_SelectedCatalogEntryId;
}

bool DataSelectionService::SelectHierarchyNode(const QString& hierarchyNodeId,
                                               QString* errorMessage)
{
    const QString normalizedNodeId = hierarchyNodeId.trimmed();
    const auto* node = m_DataHierarchy.FindNode(normalizedNodeId);
    if (!node)
    {
        SetError(errorMessage,
                 QStringLiteral("Selected hierarchy node was not found."));
        return false;
    }

    if (node->Kind != DataHierarchyNodeKind::DataEntry)
    {
        SetError(errorMessage,
                 QStringLiteral("Selected hierarchy node is not data."));
        return false;
    }

    if (!m_DataCatalog.FindById(node->DataCatalogEntryId))
    {
        SetError(errorMessage,
                 QStringLiteral("Selected hierarchy node has no catalog entry."));
        return false;
    }

    SetError(errorMessage, QString());
    return SetSelection(node->Id, node->DataCatalogEntryId);
}

bool DataSelectionService::SelectCatalogEntry(const QString& catalogEntryId,
                                              QString* errorMessage)
{
    const QString normalizedEntryId = catalogEntryId.trimmed();
    if (!m_DataCatalog.FindById(normalizedEntryId))
    {
        SetError(errorMessage,
                 QStringLiteral("Selected catalog entry was not found."));
        return false;
    }

    for (const auto& node : m_DataHierarchy.Nodes())
    {
        if (node.Kind == DataHierarchyNodeKind::DataEntry &&
            node.DataCatalogEntryId == normalizedEntryId)
        {
            SetError(errorMessage, QString());
            return SetSelection(node.Id, normalizedEntryId);
        }
    }

    SetError(errorMessage,
             QStringLiteral("Selected catalog entry has no hierarchy node."));
    return false;
}

void DataSelectionService::Clear()
{
    SetSelection(QString(), QString());
}

bool DataSelectionService::SetSelection(const QString& hierarchyNodeId,
                                        const QString& catalogEntryId)
{
    if (m_SelectedHierarchyNodeId == hierarchyNodeId &&
        m_SelectedCatalogEntryId == catalogEntryId)
    {
        return true;
    }

    m_SelectedHierarchyNodeId = hierarchyNodeId;
    m_SelectedCatalogEntryId = catalogEntryId;
    emit SelectionChanged(m_SelectedHierarchyNodeId, m_SelectedCatalogEntryId);
    return true;
}

void DataSelectionService::SetError(QString* errorMessage,
                                    const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
