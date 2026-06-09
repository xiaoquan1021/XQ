#ifndef XQ_PROJECTSESSIONSERVICE_H
#define XQ_PROJECTSESSIONSERVICE_H

#include <QObject>
#include <QString>

#include <mitkDataStorage.h>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;
class DataSelectionService;
class DataNodeRegistryService;
class ProjectService;
class TaskRunner;
class WorkflowOperationService;

class ProjectSessionService : public QObject
{
    Q_OBJECT

public:
    ProjectSessionService(ProjectService& projectService,
                          DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);
    ProjectSessionService(ProjectService& projectService,
                          DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          DataSelectionService& dataSelection,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);
    ProjectSessionService(ProjectService& projectService,
                          DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          DataSelectionService& dataSelection,
                          WorkflowOperationService& workflowOperations,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);
    ProjectSessionService(ProjectService& projectService,
                          DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          DataSelectionService& dataSelection,
                          DataNodeRegistryService& dataNodes,
                          mitk::DataStorage::Pointer dataStorage,
                          WorkflowOperationService& workflowOperations,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);

    bool Save(QString* errorMessage = nullptr);
    bool SaveAs(const QString& name,
                const QString& projectFilePath,
                QString* errorMessage = nullptr);
    bool Open(const QString& projectFilePath,
              QString* errorMessage = nullptr);
    bool Close(QString* errorMessage = nullptr);

private:
    static void SetError(QString* errorMessage, const QString& message);

    ProjectService& m_ProjectService;
    DataCatalogService& m_DataCatalog;
    DataHierarchyService& m_DataHierarchy;
    DataSelectionService* m_DataSelection = nullptr;
    DataNodeRegistryService* m_DataNodes = nullptr;
    mitk::DataStorage::Pointer m_DataStorage;
    WorkflowOperationService* m_WorkflowOperations = nullptr;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_PROJECTSESSIONSERVICE_H
