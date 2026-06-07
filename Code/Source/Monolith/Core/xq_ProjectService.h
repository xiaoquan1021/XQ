#ifndef XQ_PROJECTSERVICE_H
#define XQ_PROJECTSERVICE_H

#include <QObject>
#include <QString>

#include <optional>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;

struct ProjectMetadata
{
    QString Name;
    QString ProjectFilePath;
    QString WorkspaceDirectory;
    QString SchemaVersion;
};

class ProjectService : public QObject
{
    Q_OBJECT

public:
    explicit ProjectService(QObject* parent = nullptr);

    static QString SupportedSchemaVersion();

    bool HasActiveProject() const;
    const ProjectMetadata* CurrentProject() const;

    bool CreateProject(const QString& name,
                       const QString& projectFilePath,
                       QString* errorMessage = nullptr);
    bool SaveProject(QString* errorMessage = nullptr) const;
    bool SaveProject(const DataCatalogService& dataCatalog,
                     QString* errorMessage = nullptr) const;
    bool SaveProject(const DataCatalogService& dataCatalog,
                     const DataHierarchyService& dataHierarchy,
                     QString* errorMessage = nullptr) const;
    bool OpenProject(const QString& projectFilePath,
                     QString* errorMessage = nullptr);
    bool OpenProject(const QString& projectFilePath,
                     DataCatalogService& dataCatalog,
                     QString* errorMessage = nullptr);
    bool OpenProject(const QString& projectFilePath,
                     DataCatalogService& dataCatalog,
                     DataHierarchyService& dataHierarchy,
                     QString* errorMessage = nullptr);

signals:
    void ProjectChanged(const xq::core::ProjectMetadata& project);

private:
    static void SetError(QString* errorMessage, const QString& message);

    std::optional<ProjectMetadata> m_CurrentProject;
};

} // namespace xq::core

#endif // XQ_PROJECTSERVICE_H
