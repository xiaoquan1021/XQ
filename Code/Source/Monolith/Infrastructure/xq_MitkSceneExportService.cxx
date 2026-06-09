#include "xq_MitkSceneExportService.h"

#include <QFileInfo>

#include <mitkSceneIO.h>

namespace
{

void SetError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
}

} // namespace

namespace xq::infrastructure
{

bool MitkSceneExportService::SaveScene(mitk::DataStorage::Pointer storage,
                                       const QString& filePath,
                                       QString* errorMessage)
{
    if (storage.IsNull())
    {
        SetError(errorMessage, QStringLiteral("No MITK DataStorage."));
        return false;
    }

    const auto nodes = storage->GetAll();
    if (nodes.IsNull() || nodes->empty())
    {
        SetError(errorMessage, QStringLiteral("No data to save as MITK scene."));
        return false;
    }

    QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty())
    {
        SetError(errorMessage, QStringLiteral("No MITK scene file path."));
        return false;
    }
    if (!normalizedPath.endsWith(QStringLiteral(".mitk"), Qt::CaseInsensitive))
        normalizedPath += QStringLiteral(".mitk");

    mitk::SceneIO::Pointer sceneIO = mitk::SceneIO::New();
    const bool saved = sceneIO->SaveScene(
        nodes.GetPointer(),
        storage,
        QFileInfo(normalizedPath).absoluteFilePath().toStdString());
    if (!saved)
    {
        SetError(errorMessage,
                 QStringLiteral("MITK SceneIO failed to write %1.")
                     .arg(normalizedPath));
        return false;
    }

    SetError(errorMessage, QString());
    return true;
}

} // namespace xq::infrastructure
