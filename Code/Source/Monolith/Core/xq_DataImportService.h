#ifndef XQ_DATAIMPORTSERVICE_H
#define XQ_DATAIMPORTSERVICE_H

#include "xq_DataCatalogService.h"

#include <QObject>
#include <QString>

namespace xq::core
{

class DataHierarchyService;
class TaskRunner;

struct DataImportRequest
{
    QString RequestedId;
    QString SourcePath;
    QString DisplayName;
    QString Modality;
    DataWorkflowRole WorkflowRole = DataWorkflowRole::Unknown;
};

struct DataImportResult
{
    bool Succeeded = false;
    QString EntryId;
    QString Message;
};

class DataImportService : public QObject
{
    Q_OBJECT

public:
    DataImportService(DataCatalogService& dataCatalog,
                      TaskRunner& taskRunner,
                      QObject* parent = nullptr);
    DataImportService(DataCatalogService& dataCatalog,
                      DataHierarchyService& dataHierarchy,
                      TaskRunner& taskRunner,
                      QObject* parent = nullptr);

    DataImportResult Import(const DataImportRequest& request,
                            QString* errorMessage = nullptr);

private:
    bool AddHierarchyEntry(DataHierarchyService& dataHierarchy,
                           const DataCatalogEntry& entry,
                           QString* errorMessage);
    bool ValidateHierarchyTarget(const DataHierarchyService& dataHierarchy,
                                 const DataCatalogEntry& entry,
                                 QString* errorMessage) const;
    static QString HierarchyDataNodeId(const QString& catalogEntryId);
    static QString RoleFolderDisplayName(DataWorkflowRole role);
    static QString RoleFolderId(DataWorkflowRole role);
    static QString GenerateId(const DataImportRequest& request);
    static QString NormalizedToken(QString value);
    static void SetError(QString* errorMessage, const QString& message);

    DataCatalogService& m_DataCatalog;
    DataHierarchyService* m_DataHierarchy = nullptr;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_DATAIMPORTSERVICE_H
