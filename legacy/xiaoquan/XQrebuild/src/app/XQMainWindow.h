#ifndef XQ_APP_XQ_MAIN_WINDOW_H
#define XQ_APP_XQ_MAIN_WINDOW_H

#include "visualization/XQImageViewer.h"

#include <QMainWindow>

#include <cstddef>
#include <vector>

class QLabel;
class QTreeView;

namespace xq {

class XQImageVolume;
class XQScene;
class XQSceneModel;

class XQMainWindow : public QMainWindow {
public:
    explicit XQMainWindow(const XQScene* scene = nullptr, QWidget* parent = nullptr);

    void setScene(const XQScene* scene);
    ImageRenderResult showImage(const XQImageVolume& image);
    std::size_t lastRgbaByteCount() const;

private:
    XQSceneModel* sceneModel_;
    QTreeView* sceneTreeView_;
    QLabel* imageLabel_;
    XQImageViewer imageViewer_;
    std::vector<unsigned char> lastRgba_;
};

} // namespace xq

#endif // XQ_APP_XQ_MAIN_WINDOW_H
