#ifndef XQ_DATAMANAGEMENTSERVICE_H
#define XQ_DATAMANAGEMENTSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;
class DataNodeRegistryService;
class DataSelectionService;
class TaskRunner;

class DataManagementService : public QObject
{
    Q_OBJECT

public:
    DataManagementService(DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          DataSelectionService& dataSelection,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);
    DataManagementService(DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          DataSelectionService& dataSelection,
                          DataNodeRegistryService& dataNodes,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);

    bool RenameEntry(const QString& dataCatalogEntryId,
                     const QString& displayName,
                     QString* errorMessage = nullptr);
    bool RemoveEntry(const QString& dataCatalogEntryId,
                     QString* errorMessage = nullptr);

private:
    static void SetError(QString* errorMessage, const QString& message);

    DataCatalogService& m_DataCatalog;
    DataHierarchyService& m_DataHierarchy;
    DataSelectionService& m_DataSelection;
    DataNodeRegistryService* m_DataNodes = nullptr;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_DATAMANAGEMENTSERVICE_H
