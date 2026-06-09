#ifndef XQ_PRESENTATION_QTSCREENSHOTFILEPATHPROVIDER_H
#define XQ_PRESENTATION_QTSCREENSHOTFILEPATHPROVIDER_H

#include "Core/xq_ScreenshotFilePathProvider.h"

class QWidget;

namespace xq::presentation
{

class QtScreenshotFilePathProvider
    : public xq::core::ScreenshotFilePathProvider
{
public:
    explicit QtScreenshotFilePathProvider(QWidget* parent = nullptr);

    QString ScreenshotFilePath() override;

private:
    QWidget* m_Parent = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_QTSCREENSHOTFILEPATHPROVIDER_H
