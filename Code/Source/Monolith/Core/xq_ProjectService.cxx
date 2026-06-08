#include "xq_ProjectService.h"

#include "xq_DataCatalogService.h"
#include "xq_DataHierarchyService.h"
#include "xq_WorkflowOperationService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

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
    case DataWorkflowRole::Path:
        return QStringLiteral("path");
    case DataWorkflowRole::Segmentation:
        return QStringLiteral("segmentation");
    case DataWorkflowRole::Model:
        return QStringLiteral("model");
    case DataWorkflowRole::Mesh:
        return QStringLiteral("mesh");
    case DataWorkflowRole::SimulationPrep:
        return QStringLiteral("simulation-prep");
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
    else if (value == QStringLiteral("path"))
        *role = DataWorkflowRole::Path;
    else if (value == QStringLiteral("segmentation"))
        *role = DataWorkflowRole::Segmentation;
    else if (value == QStringLiteral("model"))
        *role = DataWorkflowRole::Model;
    else if (value == QStringLiteral("mesh"))
        *role = DataWorkflowRole::Mesh;
    else if (value == QStringLiteral("simulation-prep"))
        *role = DataWorkflowRole::SimulationPrep;
    else if (value == QStringLiteral("simulation-result"))
        *role = DataWorkflowRole::SimulationResult;
    else
        return false;

    return true;
}

QString DataHierarchyNodeKindToString(DataHierarchyNodeKind kind)
{
    switch (kind)
    {
    case DataHierarchyNodeKind::Folder:
        return QStringLiteral("folder");
    case DataHierarchyNodeKind::DataEntry:
        return QStringLiteral("data-entry");
    }

    return QStringLiteral("folder");
}

bool DataHierarchyNodeKindFromString(const QString& value,
                                     DataHierarchyNodeKind* kind)
{
    if (value == QStringLiteral("folder"))
        *kind = DataHierarchyNodeKind::Folder;
    else if (value == QStringLiteral("data-entry"))
        *kind = DataHierarchyNodeKind::DataEntry;
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

QJsonObject DataHierarchyNodeToJson(const DataHierarchyNode& node)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), node.Id);
    object.insert(QStringLiteral("parentId"), node.ParentId);
    object.insert(QStringLiteral("displayName"), node.DisplayName);
    object.insert(QStringLiteral("kind"),
                  DataHierarchyNodeKindToString(node.Kind));
    if (node.Kind == DataHierarchyNodeKind::DataEntry)
    {
        object.insert(QStringLiteral("dataCatalogEntryId"),
                      node.DataCatalogEntryId);
    }

    return object;
}

QJsonObject VariantMapToJson(const QVariantMap& values)
{
    QJsonObject object;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        object.insert(it.key(), QJsonValue::fromVariant(it.value()));
    return object;
}

QJsonObject WorkflowOperationStateToJson(
    const WorkflowOperationState& state)
{
    QJsonObject object;
    object.insert(QStringLiteral("workflowId"), state.WorkflowId);
    object.insert(QStringLiteral("selectedOperationId"),
                  state.SelectedOperationId);

    QJsonArray operationsArray;
    QStringList operationIds = state.ParameterValuesByOperationId.keys();
    operationIds.sort();
    for (const auto& operationId : operationIds)
    {
        QJsonObject operationObject;
        operationObject.insert(QStringLiteral("operationId"), operationId);
        operationObject.insert(
            QStringLiteral("parameters"),
            VariantMapToJson(
                state.ParameterValuesByOperationId.value(operationId)));
        operationsArray.append(operationObject);
    }
    object.insert(QStringLiteral("operations"), operationsArray);

    if (!state.SelectedOperationId.isEmpty() &&
        state.ParameterValuesByOperationId.contains(state.SelectedOperationId))
    {
        object.insert(
            QStringLiteral("parameters"),
            VariantMapToJson(state.ParameterValuesByOperationId.value(
                state.SelectedOperationId)));
    }

    return object;
}

QJsonObject ProjectToJson(const ProjectMetadata& project,
                          const DataCatalogService* dataCatalog,
                          const DataHierarchyService* dataHierarchy,
                          const WorkflowOperationService* workflowOperations)
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

    if (dataHierarchy)
    {
        QJsonArray dataHierarchyArray;
        for (const auto& node : dataHierarchy->Nodes())
        {
            if (node.Id == dataHierarchy->RootId())
                continue;

            dataHierarchyArray.append(DataHierarchyNodeToJson(node));
        }
        projectObject.insert(QStringLiteral("dataHierarchy"),
                             dataHierarchyArray);
    }

    if (workflowOperations)
    {
        QJsonArray workflowOperationsArray;
        for (const auto& state : workflowOperations->State())
            workflowOperationsArray.append(WorkflowOperationStateToJson(state));
        projectObject.insert(QStringLiteral("workflowOperations"),
                             workflowOperationsArray);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), project.SchemaVersion);
    root.insert(QStringLiteral("project"), projectObject);
    return root;
}

bool WriteProjectJson(const ProjectMetadata& project,
                      const DataCatalogService* dataCatalog,
                      const DataHierarchyService* dataHierarchy,
                      const WorkflowOperationService* workflowOperations,
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

    const QJsonDocument document(
        ProjectToJson(project,
                      dataCatalog,
                      dataHierarchy,
                      workflowOperations));
    projectFile.write(document.toJson(QJsonDocument::Indented));
    if (errorMessage)
        *errorMessage = QString();
    return true;
}

bool ReadProjectJson(const QString& projectFilePath,
                     ProjectMetadata* project,
                     QJsonObject* projectObject,
                     QString* errorMessage)
{
    QFile projectFile(projectFilePath);
    if (!projectFile.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to read project file.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(projectFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Project file is not valid JSON.");
        return false;
    }

    const QJsonObject root = document.object();
    const QString schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toString();
    if (schemaVersion != ProjectService::SupportedSchemaVersion())
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Unsupported project schema version.");
        return false;
    }

    const QJsonObject parsedProjectObject =
        root.value(QStringLiteral("project")).toObject();
    const QString name =
        parsedProjectObject.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Project name is missing.");
        return false;
    }

    QString workspaceDirectory =
        parsedProjectObject.value(QStringLiteral("workspaceDirectory"))
            .toString();
    if (workspaceDirectory.trimmed().isEmpty())
        workspaceDirectory = QString::fromLatin1(kDefaultWorkspaceDirectory);
    if (QDir::isAbsolutePath(workspaceDirectory))
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Project workspace directory must be relative.");
        return false;
    }

    project->Name = name;
    project->ProjectFilePath = AbsoluteFilePath(projectFilePath);
    project->WorkspaceDirectory = workspaceDirectory;
    project->SchemaVersion = schemaVersion;
    *projectObject = parsedProjectObject;
    if (errorMessage)
        *errorMessage = QString();
    return true;
}

bool LoadDataCatalog(const QJsonObject& projectObject,
                     DataCatalogService& dataCatalog,
                     QString* errorMessage)
{
    const QJsonValue dataCatalogValue =
        projectObject.value(QStringLiteral("dataCatalog"));
    if (dataCatalogValue.isArray())
    {
        const QJsonArray dataCatalogArray = dataCatalogValue.toArray();
        for (const auto& item : dataCatalogArray)
        {
            if (!item.isObject())
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Invalid data catalog entry.");
                return false;
            }

            const QJsonObject itemObject = item.toObject();
            DataWorkflowRole role = DataWorkflowRole::Unknown;
            if (!WorkflowRoleFromString(
                    itemObject.value(QStringLiteral("workflowRole")).toString(),
                    &role))
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Unsupported data workflow role.");
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
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Project data catalog must be an array.");
        return false;
    }

    if (errorMessage)
        *errorMessage = QString();
    return true;
}

bool LoadDataHierarchy(const QJsonObject& projectObject,
                       DataHierarchyService& dataHierarchy,
                       QString* errorMessage)
{
    const QJsonValue dataHierarchyValue =
        projectObject.value(QStringLiteral("dataHierarchy"));
    if (dataHierarchyValue.isArray())
    {
        const QJsonArray dataHierarchyArray = dataHierarchyValue.toArray();
        for (const auto& item : dataHierarchyArray)
        {
            if (!item.isObject())
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Invalid data hierarchy node.");
                return false;
            }

            const QJsonObject itemObject = item.toObject();
            DataHierarchyNodeKind kind = DataHierarchyNodeKind::Folder;
            if (!DataHierarchyNodeKindFromString(
                    itemObject.value(QStringLiteral("kind")).toString(),
                    &kind))
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Unsupported hierarchy node kind.");
                return false;
            }

            const QString id = itemObject.value(QStringLiteral("id")).toString();
            const QString parentId =
                itemObject.value(QStringLiteral("parentId")).toString();
            const QString displayName =
                itemObject.value(QStringLiteral("displayName")).toString();
            if (kind == DataHierarchyNodeKind::Folder)
            {
                if (!dataHierarchy.AddFolder(id,
                                             parentId,
                                             displayName,
                                             errorMessage))
                {
                    return false;
                }
            }
            else
            {
                const QString dataCatalogEntryId =
                    itemObject.value(QStringLiteral("dataCatalogEntryId"))
                        .toString();
                if (!dataHierarchy.AddDataEntry(id,
                                                parentId,
                                                dataCatalogEntryId,
                                                displayName,
                                                errorMessage))
                {
                    return false;
                }
            }
        }
    }
    else if (!dataHierarchyValue.isUndefined())
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Project data hierarchy must be an array.");
        return false;
    }

    if (errorMessage)
        *errorMessage = QString();
    return true;
}

bool LoadWorkflowOperations(const QJsonObject& projectObject,
                            WorkflowOperationService& workflowOperations,
                            QString* errorMessage)
{
    const QJsonValue workflowOperationsValue =
        projectObject.value(QStringLiteral("workflowOperations"));
    QVector<WorkflowOperationState> states;
    if (workflowOperationsValue.isArray())
    {
        const QJsonArray workflowOperationsArray =
            workflowOperationsValue.toArray();
        for (const auto& item : workflowOperationsArray)
        {
            if (!item.isObject())
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Invalid workflow operation state.");
                return false;
            }

            const QJsonObject itemObject = item.toObject();
            WorkflowOperationState state;
            state.WorkflowId =
                itemObject.value(QStringLiteral("workflowId")).toString();
            state.SelectedOperationId =
                itemObject.value(QStringLiteral("selectedOperationId"))
                    .toString();

            const QJsonValue operationsValue =
                itemObject.value(QStringLiteral("operations"));
            if (operationsValue.isArray())
            {
                const QJsonArray operationsArray = operationsValue.toArray();
                for (const auto& operationItem : operationsArray)
                {
                    if (!operationItem.isObject())
                    {
                        if (errorMessage)
                            *errorMessage =
                                QStringLiteral("Invalid workflow operation state.");
                        return false;
                    }

                    const QJsonObject operationObject =
                        operationItem.toObject();
                    const QString operationId =
                        operationObject.value(QStringLiteral("operationId"))
                            .toString();
                    state.ParameterValuesByOperationId.insert(
                        operationId,
                        operationObject.value(QStringLiteral("parameters"))
                            .toObject()
                            .toVariantMap());
                }
            }
            else if (!operationsValue.isUndefined())
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Workflow operations must be an array.");
                return false;
            }

            const QJsonValue parametersValue =
                itemObject.value(QStringLiteral("parameters"));
            if (parametersValue.isObject() &&
                !state.SelectedOperationId.trimmed().isEmpty())
            {
                state.ParameterValuesByOperationId.insert(
                    state.SelectedOperationId,
                    parametersValue.toObject().toVariantMap());
            }
            else if (!parametersValue.isUndefined() &&
                     !parametersValue.isObject())
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Workflow operation parameters must be an object.");
                return false;
            }

            states.push_back(state);
        }
    }
    else if (!workflowOperationsValue.isUndefined())
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Project workflow operations must be an array.");
        return false;
    }

    return workflowOperations.ApplyState(states, errorMessage);
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
    emit ProjectChanged(*m_CurrentProject);
    return true;
}

bool ProjectService::SaveProject(QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject,
                            nullptr,
                            nullptr,
                            nullptr,
                            errorMessage);
}

bool ProjectService::SaveProject(const DataCatalogService& dataCatalog,
                                 QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject,
                            &dataCatalog,
                            nullptr,
                            nullptr,
                            errorMessage);
}

bool ProjectService::SaveProject(const DataCatalogService& dataCatalog,
                                 const DataHierarchyService& dataHierarchy,
                                 QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject,
                            &dataCatalog,
                            &dataHierarchy,
                            nullptr,
                            errorMessage);
}

bool ProjectService::SaveProject(
    const DataCatalogService& dataCatalog,
    const DataHierarchyService& dataHierarchy,
    const WorkflowOperationService& workflowOperations,
    QString* errorMessage) const
{
    if (!m_CurrentProject.has_value())
    {
        SetError(errorMessage, QStringLiteral("No active project to save."));
        return false;
    }

    return WriteProjectJson(*m_CurrentProject,
                            &dataCatalog,
                            &dataHierarchy,
                            &workflowOperations,
                            errorMessage);
}

bool ProjectService::OpenProject(const QString& projectFilePath,
                                 QString* errorMessage)
{
    ProjectMetadata project;
    QJsonObject projectObject;
    if (!ReadProjectJson(projectFilePath,
                         &project,
                         &projectObject,
                         errorMessage))
        return false;

    m_CurrentProject = project;
    SetError(errorMessage, QString());
    emit ProjectChanged(*m_CurrentProject);
    return true;
}

bool ProjectService::OpenProject(const QString& projectFilePath,
                                 DataCatalogService& dataCatalog,
                                 QString* errorMessage)
{
    ProjectMetadata project;
    QJsonObject projectObject;
    if (!ReadProjectJson(projectFilePath,
                         &project,
                         &projectObject,
                         errorMessage))
        return false;

    DataCatalogService parsedCatalog;
    if (!LoadDataCatalog(projectObject, parsedCatalog, errorMessage))
        return false;

    dataCatalog.ReplaceWith(parsedCatalog);
    m_CurrentProject = project;
    SetError(errorMessage, QString());
    emit ProjectChanged(*m_CurrentProject);
    return true;
}

bool ProjectService::OpenProject(const QString& projectFilePath,
                                 DataCatalogService& dataCatalog,
                                 DataHierarchyService& dataHierarchy,
                                 QString* errorMessage)
{
    ProjectMetadata project;
    QJsonObject projectObject;
    if (!ReadProjectJson(projectFilePath,
                         &project,
                         &projectObject,
                         errorMessage))
    {
        return false;
    }

    DataCatalogService parsedCatalog;
    if (!LoadDataCatalog(projectObject, parsedCatalog, errorMessage))
        return false;

    DataHierarchyService parsedHierarchy;
    if (!LoadDataHierarchy(projectObject, parsedHierarchy, errorMessage))
        return false;

    dataCatalog.ReplaceWith(parsedCatalog);
    dataHierarchy.ReplaceWith(parsedHierarchy);
    m_CurrentProject = project;
    SetError(errorMessage, QString());
    emit ProjectChanged(*m_CurrentProject);
    return true;
}

bool ProjectService::OpenProject(
    const QString& projectFilePath,
    DataCatalogService& dataCatalog,
    DataHierarchyService& dataHierarchy,
    WorkflowOperationService& workflowOperations,
    QString* errorMessage)
{
    ProjectMetadata project;
    QJsonObject projectObject;
    if (!ReadProjectJson(projectFilePath,
                         &project,
                         &projectObject,
                         errorMessage))
    {
        return false;
    }

    DataCatalogService parsedCatalog;
    if (!LoadDataCatalog(projectObject, parsedCatalog, errorMessage))
        return false;

    DataHierarchyService parsedHierarchy;
    if (!LoadDataHierarchy(projectObject, parsedHierarchy, errorMessage))
        return false;

    WorkflowOperationService parsedWorkflowOperations;
    for (const auto& state : workflowOperations.State())
    {
        parsedWorkflowOperations.RegisterOperations(
            state.WorkflowId,
            workflowOperations.OperationsForWorkflow(state.WorkflowId));
    }
    if (!LoadWorkflowOperations(projectObject,
                                parsedWorkflowOperations,
                                errorMessage))
    {
        return false;
    }

    dataCatalog.ReplaceWith(parsedCatalog);
    dataHierarchy.ReplaceWith(parsedHierarchy);
    workflowOperations.ReplaceStateWith(parsedWorkflowOperations);
    m_CurrentProject = project;
    SetError(errorMessage, QString());
    emit ProjectChanged(*m_CurrentProject);
    return true;
}

void ProjectService::SetError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
