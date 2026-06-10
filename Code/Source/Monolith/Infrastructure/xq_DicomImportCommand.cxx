#include "xq_DicomImportCommand.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace xq::infrastructure
{

namespace
{

xq::core::DataImportCommandResult CommandResult(
    bool succeeded,
    const QString& catalogEntryId,
    const QString& message)
{
    xq::core::DataImportCommandResult result;
    result.Succeeded = succeeded;
    result.CatalogEntryId = catalogEntryId;
    result.Message = message;
    return result;
}

QString NormalizedToken(QString value)
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

xq::core::DataImportRequest BuildDicomImportRequest(const QString& sourcePath)
{
    const QFileInfo sourceInfo(sourcePath);
    QString displayName = sourceInfo.fileName().trimmed();
    if (displayName.isEmpty())
        displayName = sourcePath.trimmed();

    xq::core::DataImportRequest request;
    request.SourcePath = sourcePath.trimmed();
    request.DisplayName = displayName;
    request.RequestedId =
        QStringLiteral("dicom-%1").arg(NormalizedToken(displayName));
    request.Modality = QStringLiteral("DICOM");
    request.WorkflowRole = xq::core::DataWorkflowRole::DICOMSeries;
    return request;
}

} // namespace

DicomImportCommand::DicomImportCommand(
    const xq::core::FileImportPathProvider* pathProvider)
    : m_PathProvider(pathProvider)
{
}

xq::core::DataImportCommandResult DicomImportCommand::RunImport(
    xq::core::ApplicationContext& context)
{
    if (!m_PathProvider)
    {
        return CommandResult(
            false,
            QString(),
            QStringLiteral("DICOM import path provider is required."));
    }

    const QString sourcePath = m_PathProvider->ChooseFilePath().trimmed();
    if (sourcePath.isEmpty())
    {
        return CommandResult(false,
                             QString(),
                             QStringLiteral("DICOM import cancelled."));
    }

    const auto importResult =
        context.DataImports()->Import(BuildDicomImportRequest(sourcePath));

    return CommandResult(importResult.Succeeded,
                         importResult.EntryId,
                         importResult.Message);
}

} // namespace xq::infrastructure
