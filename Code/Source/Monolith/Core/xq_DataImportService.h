#ifndef XQ_DATAIMPORTSERVICE_H
#define XQ_DATAIMPORTSERVICE_H

#include "xq_DataCatalogService.h"

#include <QObject>
#include <QString>

namespace xq::core
{

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

    DataImportResult Import(const DataImportRequest& request,
                            QString* errorMessage = nullptr);

private:
    static QString GenerateId(const DataImportRequest& request);
    static QString NormalizedToken(QString value);
    static void SetError(QString* errorMessage, const QString& message);

    DataCatalogService& m_DataCatalog;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_DATAIMPORTSERVICE_H
