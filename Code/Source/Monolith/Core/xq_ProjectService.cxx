#include "xq_ProjectService.h"

#include "xq_DataCatalogService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace xq::core
{

namespace
{

constexpr const char* kSchemaVersion = "2.0";
constexpr const char* kDefaultWorkspaceDirectory = "workspace";

QString AbsoluteFilePath(const QString& filePath)
{
    return QFileInfo(filePath).absoluteFilePath();
}

QString WorkflowRoleToString(DataWorkflowRole role)
{
    switch (role)
    {
    case DataWorkflowRole::Unknown:
        return QStringLiteral("unknown");
    case DataWorkflowRole::DICOMSeries:
        return QStringLiteral("dicom-series");
    case DataWorkflowRole::Image:
        return QStringLiteral("image");
    case DataWorkflowRole::Segmentation:
        return QStringLiteral("segmentation");
    case DataWorkflowRole::Model:
        return QStringLiteral("model");
    case DataWorkflowRole::Mesh:
        return QStringLiteral("mesh");
    case DataWorkflowRole::SimulationResult:
        return QStringLiteral("simulation-result");
    }

    return QStringLiteral("unknown");
}

bool WorkflowRoleFromString(const QString& value, DataWorkflowRole* role)
{
    if (value == QStringLiteral("unknown"))
        *role = DataWorkflowRole::Unknown;
    else if (value == QStringLiteral("dicom-series"))
        *role = DataWorkflowRole::DICOMSeries;
    else if (value == QStringLiteral("image"))
        *role = DataWorkflowRole::Image;
    else if (value == QStringLiteral("segmentation"))
        *role = DataWorkflowRole::Segmentation;
    else if (value == QStringLiteral("model"))
        *role = DataWorkflowRole::Model;
    else if (value == QStringLiteral("mesh"))
        *role = DataWorkflowRole::Mesh;
    else if (value == QStringLiteral("simulation-result"))
        *role = DataWorkflowRole::SimulationResult;
    else
        return false;

    return true;
}

QJsonObject DataCatalogEntryToJson(const DataCatalogEntry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), entry.Id);
    object.insert(QStringLiteral("displayName"), entry.DisplayName);
    object.insert(QStringLiteral("sourcePath"), entry.SourcePath);
    object.insert(QStringLiteral("modality"), entry.Modality);
    object.insert(QStringLiteral("workflowRole"),
                  WorkflowRoleToString(entry.WorkflowRole));
    return object;
}

QJsonObject ProjectToJson(const ProjectMetadata& project,
                          const DataCatalogService* dataCatalog)
{
    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"), project.Name);
    projectObject.insert(QStringLiteral("workspaceDirectory"),
                         project.WorkspaceDirectory);

    if (dataCatalog)
    {
        QJsonArray dataCatalogArray;
        for (const auto& entry : dataCatalog->Entries())
            dataCatalogArray.append(DataCatalogEntryToJson(entry));
        projectObject.insert(QStringLiteral("dataCatalog"), dataCatalogArray);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), project.SchemaVersion);
    root.insert(QStringLiteral("project"), projectObject);
    return root;
}

bool WriteProjectJson(const ProjectMetadata& project,
                      const DataCatalogService* dataCatalog,
                      QString* errorMessage)
{
    const QFileInfo projectFileInfo(project.ProjectFilePath);
    QDir parentDirectory = projectFileInfo.dir();
    if (!parentDirectory.exists() &&
        !QDir().mkpath(parentDirectory.absolutePath()))
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Unable to create project directory.");
        return false;
    }

    QFile projectFile(project.ProjectFilePath);
    if (!projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to write project file.");
        return false;
    }

    const QJsonDocument document(ProjectToJson(project, dataCatalog));
    projectFile.write(document.toJson(QJsonDocument::Indented));
    if (errorMessage)
        *errorMessage = QString();
    return true;
}

} // namespace

ProjectService::ProjectService(QObject* parent)
    : QObject(parent)
{
}

QString ProjectService::SupportedSchemaVersion()
{
    return QString::fromLatin1(kSchemaVersion);
}

bool ProjectService::HasActiveProject() const
{
    return m_CurrentProject.has_value();
}

const ProjectMetadata* ProjectService::CurrentProject() const
{
    if (!m_CurrentProject.has_value())
        return nullptr;

    return &(*m_CurrentProject);
}

bool ProjectService::CreateProject(const QString& name,
                                   const QString& projectFilePath,
                                   QString* errorMessage)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("Project name is required."));
        return false;
    }

    if (projectFilePath.trimmed().isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Project file path is required."));
        return false;
    }

    ProjectMetadata project;
    project.Name = trimmedName;
    project.ProjectFilePath = AbsoluteFilePath(projectFilePath);
    project.WorkspaceDirectory = QString::fromLatin1(kDefaultWorkspaceDirectory);
    project.SchemaVersion = SupportedSchemaVersion();

    m_CurrentProject = project;
    SetError(errorMessage, QString());
    return true;
}

bool ProjectService::SaveProject(QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject, nullptr, errorMessage);
}

bool ProjectService::SaveProject(const DataCatalogService& dataCatalog,
                                 QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject, &dataCatalog, errorMessage);
}

bool ProjectService::OpenProject(const QString& projectFilePath,
                                 QString* errorMessage)
{
    QFile projectFile(projectFilePath);
    if (!projectFile.open(QIODevice::ReadOnly))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to read project file."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(projectFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        SetError(errorMessage,
                 QStringLiteral("Project file is not valid JSON."));
        return false;
    }

    const QJsonObject root = document.object();
    const QString schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toString();
    if (schemaVersion != SupportedSchemaVersion())
    {
        SetError(errorMessage,
                 QStringLiteral("Unsupported project schema version."));
        return false;
    }

    const QJsonObject projectObject =
        root.value(QStringLiteral("project")).toObject();
    const QString name =
        projectObject.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Project name is missing."));
        return false;
    }

    QString workspaceDirectory =
        projectObject.value(QStringLiteral("workspaceDirectory")).toString();
    if (workspaceDirectory.trimmed().isEmpty())
        workspaceDirectory = QString::fromLatin1(kDefaultWorkspaceDirectory);
    if (QDir::isAbsolutePath(workspaceDirectory))
    {
        SetError(errorMessage,
                 QStringLiteral("Project workspace directory must be relative."));
        return false;
    }

    ProjectMetadata project;
    project.Name = name;
    project.ProjectFilePath = AbsoluteFilePath(projectFilePath);
    project.WorkspaceDirectory = workspaceDirectory;
    project.SchemaVersion = schemaVersion;

    m_CurrentProject = project;
    SetError(errorMessage, QString());
    return true;
}

bool ProjectService::OpenProject(const QString& projectFilePath,
                                 DataCatalogService& dataCatalog,
                                 QString* errorMessage)
{
    QFile projectFile(projectFilePath);
    if (!projectFile.open(QIODevice::ReadOnly))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to read project file."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(projectFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        SetError(errorMessage,
                 QStringLiteral("Project file is not valid JSON."));
        return false;
    }

    const QJsonObject root = document.object();
    const QString schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toString();
    if (schemaVersion != SupportedSchemaVersion())
    {
        SetError(errorMessage,
                 QStringLiteral("Unsupported project schema version."));
        return false;
    }

    const QJsonObject projectObject =
        root.value(QStringLiteral("project")).toObject();
    const QString name =
        projectObject.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
    {
        SetError(errorMessage,
                 QStringLiteral("Project name is missing."));
        return false;
    }

    QString workspaceDirectory =
        projectObject.value(QStringLiteral("workspaceDirectory")).toString();
    if (workspaceDirectory.trimmed().isEmpty())
        workspaceDirectory = QString::fromLatin1(kDefaultWorkspaceDirectory);
    if (QDir::isAbsolutePath(workspaceDirectory))
    {
        SetError(errorMessage,
                 QStringLiteral("Project workspace directory must be relative."));
        return false;
    }

    const QJsonValue dataCatalogValue =
        projectObject.value(QStringLiteral("dataCatalog"));
    if (dataCatalogValue.isArray())
    {
        const QJsonArray dataCatalogArray = dataCatalogValue.toArray();
        for (const auto& item : dataCatalogArray)
        {
            if (!item.isObject())
            {
                SetError(errorMessage,
                         QStringLiteral("Invalid data catalog entry."));
                return false;
            }

            const QJsonObject itemObject = item.toObject();
            DataWorkflowRole role = DataWorkflowRole::Unknown;
            if (!WorkflowRoleFromString(
                    itemObject.value(QStringLiteral("workflowRole")).toString(),
                    &role))
            {
                SetError(errorMessage,
                         QStringLiteral("Unsupported data workflow role."));
                return false;
            }

            DataCatalogEntry entry;
            entry.Id = itemObject.value(QStringLiteral("id")).toString();
            entry.DisplayName =
                itemObject.value(QStringLiteral("displayName")).toString();
            entry.SourcePath =
                itemObject.value(QStringLiteral("sourcePath")).toString();
            entry.Modality =
                itemObject.value(QStringLiteral("modality")).toString();
            entry.WorkflowRole = role;

            if (!dataCatalog.RegisterEntry(entry, errorMessage))
                return false;
        }
    }
    else if (!dataCatalogValue.isUndefined())
    {
        SetError(errorMessage,
                 QStringLiteral("Project data catalog must be an array."));
        return false;
    }

    ProjectMetadata project;
    project.Name = name;
    project.ProjectFilePath = AbsoluteFilePath(projectFilePath);
    project.WorkspaceDirectory = workspaceDirectory;
    project.SchemaVersion = schemaVersion;

    m_CurrentProject = project;
    SetError(errorMessage, QString());
    return true;
}

void ProjectService::SetError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
