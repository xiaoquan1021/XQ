#include "xq_ProjectService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

QJsonObject ProjectToJson(const ProjectMetadata& project)
{
    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"), project.Name);
    projectObject.insert(QStringLiteral("workspaceDirectory"),
                         project.WorkspaceDirectory);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), project.SchemaVersion);
    root.insert(QStringLiteral("project"), projectObject);
    return root;
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

    const QFileInfo projectFileInfo(m_CurrentProject->ProjectFilePath);
    QDir parentDirectory = projectFileInfo.dir();
    if (!parentDirectory.exists() &&
        !QDir().mkpath(parentDirectory.absolutePath()))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to create project directory."));
        return false;
    }

    QFile projectFile(m_CurrentProject->ProjectFilePath);
    if (!projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        SetError(errorMessage,
                 QStringLiteral("Unable to write project file."));
        return false;
    }

    const QJsonDocument document(ProjectToJson(*m_CurrentProject));
    projectFile.write(document.toJson(QJsonDocument::Indented));
    SetError(errorMessage, QString());
    return true;
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

void ProjectService::SetError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
