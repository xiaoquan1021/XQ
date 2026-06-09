#ifndef XQ_SCREENSHOTFILEPATHPROVIDER_H
#define XQ_SCREENSHOTFILEPATHPROVIDER_H

#include <QString>

namespace xq::core
{

class ScreenshotFilePathProvider
{
public:
    virtual ~ScreenshotFilePathProvider() = default;

    virtual QString ScreenshotFilePath() = 0;
};

} // namespace xq::core

#endif // XQ_SCREENSHOTFILEPATHPROVIDER_H
