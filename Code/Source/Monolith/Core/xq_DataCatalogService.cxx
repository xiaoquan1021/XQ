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
