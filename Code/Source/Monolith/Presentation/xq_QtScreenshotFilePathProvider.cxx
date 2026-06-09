#include "xq_QtScreenshotFilePathProvider.h"

#include <QFileDialog>
#include <QWidget>

namespace xq::presentation
{

QtScreenshotFilePathProvider::QtScreenshotFilePathProvider(QWidget* parent)
    : m_Parent(parent)
{
}

QString QtScreenshotFilePathProvider::ScreenshotFilePath()
{
    return QFileDialog::getSaveFileName(
        m_Parent,
        QStringLiteral("Save Screenshot"),
        QStringLiteral("xq-screenshot.png"),
        QStringLiteral("PNG images (*.png);;All files (*)"));
}

} // namespace xq::presentation
