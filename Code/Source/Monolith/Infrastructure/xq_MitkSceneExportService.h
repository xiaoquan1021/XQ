#ifndef XQ_INFRASTRUCTURE_MITKSCENEEXPORTSERVICE_H
#define XQ_INFRASTRUCTURE_MITKSCENEEXPORTSERVICE_H

#include "Core/xq_SceneExportService.h"

namespace xq::infrastructure
{

class MitkSceneExportService : public xq::core::SceneExportService
{
public:
    bool SaveScene(mitk::DataStorage::Pointer storage,
                   const QString& filePath,
                   QString* errorMessage) override;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITKSCENEEXPORTSERVICE_H
