#include "xq_MitkFileDataImportCommand.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"

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

xq::core::DataImportRequest BuildImportRequest(const QString& sourcePath)
{
    const QFileInfo fileInfo(sourcePath);
    QString displayName = fileInfo.fileName().trimmed();
    if (displayName.isEmpty())
        displayName = sourcePath.trimmed();

    xq::core::DataImportRequest request;
    request.SourcePath = sourcePath.trimmed();
    request.DisplayName = displayName;
    request.RequestedId =
        QStringLiteral("image-%1").arg(NormalizedToken(displayName));
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

MitkFileDataImportCommand::MitkFileDataImportCommand(
    const xq::core::FileImportPathProvider* pathProvider,
    const MitkFileReader* reader)
    : m_PathProvider(pathProvider)
    , m_Reader(reader)
{
}

xq::core::DataImportCommandResult MitkFileDataImportCommand::RunImport(
    xq::core::ApplicationContext& context)
{
    if (!m_PathProvider)
    {
        return CommandResult(
            false,
            QString(),
            QStringLiteral("File import path provider is required."));
    }

    const QString sourcePath = m_PathProvider->ChooseFilePath().trimmed();
    if (sourcePath.isEmpty())
    {
        return CommandResult(false,
                             QString(),
                             QStringLiteral("File import cancelled."));
    }

    MitkFileImportRequest request;
    request.DataStorage = context.DataStorage();
    request.DataImports = context.DataImports();
    request.DataNodes = context.DataNodes();
    request.Import = BuildImportRequest(sourcePath);
    request.Reader = m_Reader;

    MitkFileImportService service;
    const auto result = service.Import(request);
    return CommandResult(result.Succeeded,
                         result.CatalogEntryId,
                         result.Message);
}

} // namespace xq::infrastructure
