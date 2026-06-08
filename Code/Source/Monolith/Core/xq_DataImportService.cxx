#include "xq_DataImportService.h"

#include "xq_DataHierarchyService.h"
#include "xq_DataSelectionService.h"
#include "xq_TaskRunner.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace xq::core
{

namespace
{

QString WorkflowRoleToken(DataWorkflowRole role)
{
    switch (role)
    {
    case DataWorkflowRole::DICOMSeries:
        return QStringLiteral("dicom");
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
        return QStringLiteral("simulation");
    case DataWorkflowRole::SimulationResult:
        return QStringLiteral("result");
    case DataWorkflowRole::ROMSimulation:
        return QStringLiteral("rom");
    case DataWorkflowRole::Unknown:
        break;
    }

    return QStringLiteral("data");
}

} // namespace

DataImportService::DataImportService(DataCatalogService& dataCatalog,
                                     TaskRunner& taskRunner,
                                     QObject* parent)
    : QObject(parent)
    , m_DataCatalog(dataCatalog)
    , m_TaskRunner(taskRunner)
{
}

DataImportService::DataImportService(DataCatalogService& dataCatalog,
                                     DataHierarchyService& dataHierarchy,
                                     TaskRunner& taskRunner,
                                     QObject* parent)
    : QObject(parent)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(&dataHierarchy)
    , m_TaskRunner(taskRunner)
{
}

DataImportService::DataImportService(DataCatalogService& dataCatalog,
                                     DataHierarchyService& dataHierarchy,
                                     DataSelectionService& dataSelection,
                                     TaskRunner& taskRunner,
                                     QObject* parent)
    : QObject(parent)
    , m_DataCatalog(dataCatalog)
    , m_DataHierarchy(&dataHierarchy)
    , m_DataSelection(&dataSelection)
    , m_TaskRunner(taskRunner)
{
}

DataImportResult DataImportService::Import(const DataImportRequest& request,
                                           QString* errorMessage)
{
    DataImportResult result;

    if (request.SourcePath.trimmed().isEmpty())
    {
        result.Message = QStringLiteral("Import source path is required.");
        SetError(errorMessage, result.Message);
        return result;
    }

    DataCatalogEntry entry;
    entry.Id = request.RequestedId.trimmed();
    if (entry.Id.isEmpty())
        entry.Id = GenerateId(request);
    entry.DisplayName = request.DisplayName.trimmed();
    if (entry.DisplayName.isEmpty())
        entry.DisplayName = QFileInfo(request.SourcePath).fileName();
    if (entry.DisplayName.isEmpty())
        entry.DisplayName = entry.Id;
    entry.SourcePath = request.SourcePath.trimmed();
    entry.Modality = request.Modality.trimmed();
    entry.WorkflowRole = request.WorkflowRole;

    const QString taskName =
        QStringLiteral("Import %1").arg(entry.DisplayName);
    QString taskMessage;
    const bool taskSucceeded = m_TaskRunner.RunBlocking(
        taskName,
        [this, &entry](QString* message) {
            if (m_DataHierarchy)
            {
                QString hierarchyTargetError;
                if (!ValidateHierarchyTarget(*m_DataHierarchy,
                                             entry,
                                             &hierarchyTargetError))
                {
                    if (message)
                        *message = hierarchyTargetError;
                    return false;
                }
            }

            DataCatalogService parsedCatalog;
            parsedCatalog.ReplaceWith(m_DataCatalog);
            QString catalogError;
            const bool registered =
                parsedCatalog.RegisterEntry(entry, &catalogError);
            if (!registered)
            {
                if (message)
                    *message = catalogError;
                return false;
            }

            if (!m_DataHierarchy)
            {
                m_DataCatalog.ReplaceWith(parsedCatalog);
                if (message)
                    *message = QStringLiteral("Imported data catalog entry.");
                return true;
            }

            DataHierarchyService parsedHierarchy;
            parsedHierarchy.ReplaceWith(*m_DataHierarchy);
            QString hierarchyError;
            const bool hierarchyAdded =
                AddHierarchyEntry(parsedHierarchy, entry, &hierarchyError);
            if (!hierarchyAdded)
            {
                if (message)
                    *message = hierarchyError;
                return false;
            }

            m_DataCatalog.ReplaceWith(parsedCatalog);
            m_DataHierarchy->ReplaceWith(parsedHierarchy);
            if (message)
                *message = QStringLiteral("Imported data catalog entry.");
            return true;
        },
        &taskMessage);

    result.Succeeded = taskSucceeded;
    result.EntryId = taskSucceeded ? entry.Id : QString();
    result.Message = taskMessage;
    if (taskSucceeded && m_DataSelection)
    {
        QString selectionMessage;
        if (!m_DataSelection->SelectCatalogEntry(entry.Id, &selectionMessage))
            result.Message = selectionMessage;
    }
    SetError(errorMessage, result.Message);
    return result;
}

bool DataImportService::AddHierarchyEntry(DataHierarchyService& dataHierarchy,
                                          const DataCatalogEntry& entry,
                                          QString* errorMessage)
{
    const QString folderId = RoleFolderId(entry.WorkflowRole);
    if (dataHierarchy.FindNode(folderId) == nullptr)
    {
        if (!dataHierarchy.AddFolder(folderId,
                                     dataHierarchy.RootId(),
                                     RoleFolderDisplayName(entry.WorkflowRole),
                                     errorMessage))
        {
            return false;
        }
    }

    return dataHierarchy.AddDataEntry(HierarchyDataNodeId(entry.Id),
                                      folderId,
                                      entry.Id,
                                      entry.DisplayName,
                                      errorMessage);
}

bool DataImportService::ValidateHierarchyTarget(
    const DataHierarchyService& dataHierarchy,
    const DataCatalogEntry& entry,
    QString* errorMessage) const
{
    const QString folderId = RoleFolderId(entry.WorkflowRole);
    const auto* existingFolder = dataHierarchy.FindNode(folderId);
    if (existingFolder && existingFolder->Kind != DataHierarchyNodeKind::Folder)
    {
        SetError(errorMessage,
                 QStringLiteral("Hierarchy import target is not a folder."));
        return false;
    }

    if (dataHierarchy.FindNode(HierarchyDataNodeId(entry.Id)) != nullptr)
    {
        SetError(errorMessage,
                 QStringLiteral("Duplicate hierarchy node id."));
        return false;
    }

    SetError(errorMessage, QString());
    return true;
}

QString DataImportService::HierarchyDataNodeId(const QString& catalogEntryId)
{
    return QStringLiteral("data-%1").arg(catalogEntryId.trimmed());
}

QString DataImportService::RoleFolderDisplayName(DataWorkflowRole role)
{
    switch (role)
    {
    case DataWorkflowRole::DICOMSeries:
        return QStringLiteral("DICOM");
    case DataWorkflowRole::Image:
        return QStringLiteral("Images");
    case DataWorkflowRole::Path:
        return QStringLiteral("Paths");
    case DataWorkflowRole::Segmentation:
        return QStringLiteral("Segmentations");
    case DataWorkflowRole::Model:
        return QStringLiteral("Models");
    case DataWorkflowRole::Mesh:
        return QStringLiteral("Meshes");
    case DataWorkflowRole::SimulationPrep:
        return QStringLiteral("Simulations");
    case DataWorkflowRole::SimulationResult:
        return QStringLiteral("Simulation Results");
    case DataWorkflowRole::ROMSimulation:
        return QStringLiteral("ROM Simulations");
    case DataWorkflowRole::Unknown:
        break;
    }

    return QStringLiteral("Data");
}

QString DataImportService::RoleFolderId(DataWorkflowRole role)
{
    switch (role)
    {
    case DataWorkflowRole::DICOMSeries:
        return QStringLiteral("dicom");
    case DataWorkflowRole::Image:
        return QStringLiteral("images");
    case DataWorkflowRole::Path:
        return QStringLiteral("paths");
    case DataWorkflowRole::Segmentation:
        return QStringLiteral("segmentations");
    case DataWorkflowRole::Model:
        return QStringLiteral("models");
    case DataWorkflowRole::Mesh:
        return QStringLiteral("meshes");
    case DataWorkflowRole::SimulationPrep:
        return QStringLiteral("simulations");
    case DataWorkflowRole::SimulationResult:
        return QStringLiteral("simulation-results");
    case DataWorkflowRole::ROMSimulation:
        return QStringLiteral("rom-simulations");
    case DataWorkflowRole::Unknown:
        break;
    }

    return QStringLiteral("data");
}

QString DataImportService::GenerateId(const DataImportRequest& request)
{
    QString sourceToken = QFileInfo(request.SourcePath).fileName();
    if (sourceToken.trimmed().isEmpty())
        sourceToken = request.DisplayName;
    if (sourceToken.trimmed().isEmpty())
        sourceToken = QStringLiteral("data");

    return QStringLiteral("%1-%2")
        .arg(WorkflowRoleToken(request.WorkflowRole),
             NormalizedToken(sourceToken));
}

QString DataImportService::NormalizedToken(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                  QStringLiteral("-"));
    value.replace(QRegularExpression(QStringLiteral("^-+|-+$")),
                  QString());

    if (value.isEmpty())
        return QStringLiteral("data");

    return value;
}

void DataImportService::SetError(QString* errorMessage,
                                 const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace xq::core
