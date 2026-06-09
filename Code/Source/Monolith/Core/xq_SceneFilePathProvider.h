#ifndef XQ_SCENEFILEPATHPROVIDER_H
#define XQ_SCENEFILEPATHPROVIDER_H

#include <QString>

namespace xq::core
{

class SceneFilePathProvider
{
public:
    virtual ~SceneFilePathProvider() = default;

    virtual QString SceneFilePath() = 0;
};

} // namespace xq::core

#endif // XQ_SCENEFILEPATHPROVIDER_H
