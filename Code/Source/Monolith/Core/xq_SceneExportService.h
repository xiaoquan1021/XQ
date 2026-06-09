#ifndef XQ_SCENEEXPORTSERVICE_H
#define XQ_SCENEEXPORTSERVICE_H

#include <QString>

#include <mitkDataStorage.h>

namespace xq::core
{

class SceneExportService
{
public:
    virtual ~SceneExportService() = default;

    virtual bool SaveScene(mitk::DataStorage::Pointer storage,
                           const QString& filePath,
                           QString* errorMessage) = 0;
};

} // namespace xq::core

#endif // XQ_SCENEEXPORTSERVICE_H
