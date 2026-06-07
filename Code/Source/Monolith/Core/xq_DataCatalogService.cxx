#include "xq_DataCatalogService.h"

namespace xq::core
{

DataCatalogService::DataCatalogService(QObject* parent)
    : QObject(parent)
{
}

QVector<DataCatalogEntry> DataCatalogService::Entries() const
{
    return m_Entries;
}

const DataCatalogEntry* DataCatalogService::FindById(const QString& id) const
{
    const QString normalizedId = NormalizedId(id);
    if (normalizedId.isEmpty())
        return nullptr;

    for (const auto& entry : m_Entries)
    {
        if (entry.Id == normalizedId)
            return &entry;
    }

    return nullptr;
}

void DataCatalogService::ReplaceWith(const DataCatalogService& other)
{
    m_Entries = other.m_Entries;
}

bool DataCatalogService::RegisterEntry(const DataCatalogEntry& entry,
                                       QString* errorMessage)
{
    const QString normalizedId = NormalizedId(entry.Id);
    if (normalizedId.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Data id is required."));
        return false;
    }

    if (entry.SourcePath.trimmed().isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Data source path is required."));
        return false;
    }

    if (FindById(normalizedId) != nullptr)
    {
        SetError(errorMessage, QStringLiteral("Duplicate data id."));
        return false;
    }

    DataCatalogEntry normalizedEntry = entry;
    normalizedEntry.Id = normalizedId;
    normalizedEntry.DisplayName = normalizedEntry.DisplayName.trimmed();
    if (normalizedEntry.DisplayName.isEmpty())
        normalizedEntry.DisplayName = normalizedId;
    normalizedEntry.SourcePath = normalizedEntry.SourcePath.trimmed();
    normalizedEntry.Modality = normalizedEntry.Modality.trimmed();

    m_Entries.append(normalizedEntry);
    SetError(errorMessage, QString());
    return true;
}

bool DataCatalogService::RenameEntry(const QString& id,
                                     const QString& displayName,
                                     QString* errorMessage)
{
    const QString normalizedId = NormalizedId(id);
    if (normalizedId.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Data id is required."));
        return false;
    }

    const QString normalizedDisplayName = displayName.trimmed();
    if (normalizedDisplayName.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Data display name is required."));
        return false;
    }

    for (auto& entry : m_Entries)
    {
        if (entry.Id == normalizedId)
        {
            entry.DisplayName = normalizedDisplayName;
            SetError(errorMessage, QString());
            return true;
        }
    }

    SetError(errorMessage, QStringLiteral("Data entry was not found."));
    return false;
}

bool DataCatalogService::RemoveEntry(const QString& id,
                                     QString* errorMessage)
{
    const QString normalizedId = NormalizedId(id);
    if (normalizedId.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Data id is required."));
        return false;
    }

    for (int i = 0; i < m_Entries.size(); ++i)
    {
        if (m_Entries.at(i).Id == normalizedId)
        {
            m_Entries.removeAt(i);
            SetError(errorMessage, QString());
            return true;
        }
    }

    SetError(errorMessage, QStringLiteral("Data entry was not found."));
    return false;
}

QString DataCatalogService::NormalizedId(const QString& id)
{
    return id.trimmed();
}

void DataCatalogService::SetError(QString* errorMessage,
                                  const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
