#include "xq_DataImportService.h"

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
    case DataWorkflowRole::Segmentation:
        return QStringLiteral("segmentation");
    case DataWorkflowRole::Model:
        return QStringLiteral("model");
    case DataWorkflowRole::Mesh:
        return QStringLiteral("mesh");
    case DataWorkflowRole::SimulationResult:
        return QStringLiteral("result");
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
            QString catalogError;
            const bool registered =
                m_DataCatalog.RegisterEntry(entry, &catalogError);
            if (message)
                *message = registered
                               ? QStringLiteral("Imported data catalog entry.")
                               : catalogError;
            return registered;
        },
        &taskMessage);

    result.Succeeded = taskSucceeded;
    result.EntryId = taskSucceeded ? entry.Id : QString();
    result.Message = taskMessage;
    SetError(errorMessage, result.Message);
    return result;
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
