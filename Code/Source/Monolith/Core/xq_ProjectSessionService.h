#ifndef XQ_PROJECTSESSIONSERVICE_H
#define XQ_PROJECTSESSIONSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;
class ProjectService;
class TaskRunner;

class ProjectSessionService : public QObject
{
    Q_OBJECT

public:
    ProjectSessionService(ProjectService& projectService,
                          DataCatalogService& dataCatalog,
                          DataHierarchyService& dataHierarchy,
                          TaskRunner& taskRunner,
                          QObject* parent = nullptr);

    bool Save(QString* errorMessage = nullptr);
    bool Open(const QString& projectFilePath,
              QString* errorMessage = nullptr);

private:
    static void SetError(QString* errorMessage, const QString& message);

    ProjectService& m_ProjectService;
    DataCatalogService& m_DataCatalog;
    DataHierarchyService& m_DataHierarchy;
    TaskRunner& m_TaskRunner;
};

} // namespace xq::core

#endif // XQ_PROJECTSESSIONSERVICE_H
