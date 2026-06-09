#ifndef XQ_PRESENTATION_QTSCENEFILEPATHPROVIDER_H
#define XQ_PRESENTATION_QTSCENEFILEPATHPROVIDER_H

#include "Core/xq_SceneFilePathProvider.h"

class QWidget;

namespace xq::presentation
{

class QtSceneFilePathProvider : public xq::core::SceneFilePathProvider
{
public:
    explicit QtSceneFilePathProvider(QWidget* parent = nullptr);

    QString SceneFilePath() override;

private:
    QWidget* m_Parent = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_QTSCENEFILEPATHPROVIDER_H
