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

struct InferredRole
{
    xq::core::DataWorkflowRole Role = xq::core::DataWorkflowRole::Image;
    QString Prefix = QStringLiteral("image");
};

bool ContainsAny(const QString& value, std::initializer_list<QString> tokens)
{
    for (const auto& token : tokens)
    {
        if (value.contains(token))
            return true;
    }

    return false;
}

InferredRole InferRole(const QFileInfo& fileInfo)
{
    const QString fileName = fileInfo.fileName().toLower();
    const QString suffix = fileInfo.suffix().toLower();
    const QString completeSuffix = fileInfo.completeSuffix().toLower();

    if (suffix == QStringLiteral("xqpth") ||
        ContainsAny(fileName, {QStringLiteral("centerline")}))
    {
        return {xq::core::DataWorkflowRole::Path,
                QStringLiteral("path")};
    }

    if (ContainsAny(fileName,
                    {QStringLiteral("segmentation"),
                     QStringLiteral("-seg"),
                     QStringLiteral("_seg"),
                     QStringLiteral("contour"),
                     QStringLiteral("label")}))
    {
        return {xq::core::DataWorkflowRole::Segmentation,
                QStringLiteral("segmentation")};
    }

    if (ContainsAny(fileName,
                    {QStringLiteral("result"),
                     QStringLiteral("flow"),
                     QStringLiteral("pressure"),
                     QStringLiteral("velocity")}))
    {
        return {xq::core::DataWorkflowRole::SimulationResult,
                QStringLiteral("result")};
    }

    if (fileName.contains(QStringLiteral("mesh")) ||
        suffix == QStringLiteral("msh") ||
        suffix == QStringLiteral("mesh"))
    {
        return {xq::core::DataWorkflowRole::Mesh,
                QStringLiteral("mesh")};
    }

    if (fileName.contains(QStringLiteral("model")) ||
        suffix == QStringLiteral("vtp") ||
        suffix == QStringLiteral("stl") ||
        suffix == QStringLiteral("obj") ||
        suffix == QStringLiteral("ply"))
    {
        return {xq::core::DataWorkflowRole::Model,
                QStringLiteral("model")};
    }

    if (suffix == QStringLiteral("vtu") ||
        completeSuffix == QStringLiteral("vtu.gz"))
    {
        return {xq::core::DataWorkflowRole::Mesh,
                QStringLiteral("mesh")};
    }

    return {};
}

xq::core::DataImportRequest BuildImportRequest(const QString& sourcePath)
{
    const QFileInfo fileInfo(sourcePath);
    QString displayName = fileInfo.fileName().trimmed();
    if (displayName.isEmpty())
        displayName = sourcePath.trimmed();

    const InferredRole inferredRole = InferRole(fileInfo);

    xq::core::DataImportRequest request;
    request.SourcePath = sourcePath.trimmed();
    request.DisplayName = displayName;
    request.RequestedId =
        QStringLiteral("%1-%2").arg(inferredRole.Prefix,
                                    NormalizedToken(displayName));
    request.WorkflowRole = inferredRole.Role;
    return request;
}

} // namespace

MitkFileDataImportCommand::MitkFileDataImportCommand(
    const xq::core::FileImportPathProvider* pathProvider,
    const MitkFileReader* reader,
    xq::core::RenderRefreshService* renderRefresh)
    : m_PathProvider(pathProvider)
    , m_Reader(reader)
    , m_RenderRefresh(renderRefresh)
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
    if (result.Succeeded && m_RenderRefresh)
        m_RenderRefresh->RefreshDataStorage(context.DataStorage());

    return CommandResult(result.Succeeded,
                         result.CatalogEntryId,
                         result.Message);
}

} // namespace xq::infrastructure
