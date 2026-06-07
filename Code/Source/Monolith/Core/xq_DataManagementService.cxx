#include "xq_DataManagementService.h"

#include "xq_DataCatalogService.h"
#include "xq_DataHierarchyService.h"
#include "xq_DataSelectionService.h"
#include "xq_TaskRunner.h"

namespace xq::core
{

DataManagementService::DataManagementService(
    DataCatalogService& dataCatalog,
    DataHierarchyService& dataHierarchy,
    DataSelectionService& dataSelection,
    TaskRunner& taskRunner,
    QObject* parent)
    : QObject(parent)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(dataHierarchy)
    , m_DataSelection(dataSelection)
    , m_TaskRunner(taskRunner)
{
}

bool DataManagementService::RenameEntry(const QString& dataCatalogEntryId,
                                        const QString& displayName,
                                        QString* errorMessage)
{
    const QString normalizedEntryId = dataCatalogEntryId.trimmed();
    const QString normalizedDisplayName = displayName.trimmed();

    QString taskMessage;
    const bool succeeded = m_TaskRunner.RunBlocking(
        QStringLiteral("Rename Data"),
        [this, &normalizedEntryId, &normalizedDisplayName](QString* message) {
            DataCatalogService parsedCatalog;
            parsedCatalog.ReplaceWith(m_DataCatalog);
            DataHierarchyService parsedHierarchy;
            parsedHierarchy.ReplaceWith(m_DataHierarchy);

            QString catalogError;
            if (!parsedCatalog.RenameEntry(normalizedEntryId,
                                           normalizedDisplayName,
                                           &catalogError))
            {
                SetError(message, catalogError);
                return false;
            }

            QString hierarchyError;
            if (!parsedHierarchy.RenameDataEntriesForCatalogEntry(
                    normalizedEntryId,
                    normalizedDisplayName,
                    &hierarchyError))
            {
                SetError(message, hierarchyError);
                return false;
            }

            m_DataCatalog.ReplaceWith(parsedCatalog);
            m_DataHierarchy.ReplaceWith(parsedHierarchy);
            SetError(message, QStringLiteral("Renamed data entry."));
            return true;
        },
        &taskMessage);

    SetError(errorMessage, taskMessage);
    return succeeded;
}

bool DataManagementService::RemoveEntry(const QString& dataCatalogEntryId,
                                        QString* errorMessage)
{
    const QString normalizedEntryId = dataCatalogEntryId.trimmed();
    const bool removedEntryWasSelected =
        m_DataSelection.SelectedCatalogEntryId() == normalizedEntryId;

    QString taskMessage;
    const bool succeeded = m_TaskRunner.RunBlocking(
        QStringLiteral("Remove Data"),
        [this, &normalizedEntryId](QString* message) {
            DataCatalogService parsedCatalog;
            parsedCatalog.ReplaceWith(m_DataCatalog);
            DataHierarchyService parsedHierarchy;
            parsedHierarchy.ReplaceWith(m_DataHierarchy);

            QString catalogError;
            if (!parsedCatalog.RemoveEntry(normalizedEntryId, &catalogError))
            {
                SetError(message, catalogError);
                return false;
            }

            QString hierarchyError;
            if (!parsedHierarchy.RemoveDataEntriesForCatalogEntry(
                    normalizedEntryId,
                    &hierarchyError))
            {
                SetError(message, hierarchyError);
                return false;
            }

            m_DataCatalog.ReplaceWith(parsedCatalog);
            m_DataHierarchy.ReplaceWith(parsedHierarchy);
            SetError(message, QStringLiteral("Removed data entry."));
            return true;
        },
        &taskMessage);

    if (succeeded && removedEntryWasSelected)
        m_DataSelection.Clear();

    SetError(errorMessage, taskMessage);
    return succeeded;
}

void DataManagementService::SetError(QString* errorMessage,
                                     const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
