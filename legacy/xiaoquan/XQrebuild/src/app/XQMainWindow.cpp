#include "app/XQMainWindow.h"

#include "ui/XQSceneModel.h"

#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QSizePolicy>
#include <QTreeView>

#include <algorithm>

namespace xq {
namespace {

const int kDefaultRenderWidth = 256;
const int kDefaultRenderHeight = 256;

} // namespace

XQMainWindow::XQMainWindow(const XQScene* scene, QWidget* parent)
    : QMainWindow(parent)
    , sceneModel_(new XQSceneModel(scene, this))
    , sceneTreeView_(new QTreeView(this))
    , imageLabel_(new QLabel(this))
    , imageViewer_()
    , lastRgba_()
{
    setWindowTitle(QStringLiteral("XQ"));

    sceneTreeView_->setObjectName(QStringLiteral("xqSceneTreeView"));
    sceneTreeView_->setModel(sceneModel_);

    QDockWidget* sceneDock = new QDockWidget(QStringLiteral("Scene"), this);
    sceneDock->setObjectName(QStringLiteral("xqSceneDock"));
    sceneDock->setWidget(sceneTreeView_);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    imageLabel_->setObjectName(QStringLiteral("xqImageLabel"));
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMinimumSize(kDefaultRenderWidth, kDefaultRenderHeight);
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCentralWidget(imageLabel_);

    resize(900, 600);
}

void XQMainWindow::setScene(const XQScene* scene)
{
    sceneModel_->setScene(scene);
}

ImageRenderResult XQMainWindow::showImage(const XQImageVolume& image)
{
    const int width = std::max(kDefaultRenderWidth, imageLabel_->width());
    const int height = std::max(kDefaultRenderHeight, imageLabel_->height());

    ImageRenderResult result = imageViewer_.renderToRgba(image, width, height, &lastRgba_);
    if (!result.ok) {
        imageLabel_->clear();
        return result;
    }

    QImage rendered(lastRgba_.data(),
                    result.width,
                    result.height,
                    result.width * 4,
                    QImage::Format_RGBA8888);
    imageLabel_->setPixmap(QPixmap::fromImage(rendered.copy()));

    return result;
}

std::size_t XQMainWindow::lastRgbaByteCount() const
{
    return lastRgba_.size();
}

} // namespace xq
