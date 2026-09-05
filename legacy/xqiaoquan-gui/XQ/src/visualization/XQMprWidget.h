#ifndef XQ_VISUALIZATION_MPR_WIDGET_H
#define XQ_VISUALIZATION_MPR_WIDGET_H

#include <QWidget>

class QFrame;
class QGridLayout;
class QLabel;
class QEvent;

namespace xq {

class XQRenderScene;
class XQSliceViewWidget;
class XQVolumeViewWidget;

// MITK-style 2x2 four-view container over one resident XQRenderScene: top-left
// Axial (red frame), top-right Sagittal (green), bottom-left Coronal (blue),
// bottom-right 3D (yellow). The three slice cells are XQSliceViewWidget, the 3D
// cell is XQVolumeViewWidget; every cell mounts one of the scene's four resident
// renderers.
class XQMprWidget : public QWidget {
    Q_OBJECT

public:
    // View layout: Quad = 2x2 all shown; Single = only Axial (fills the grid).
    enum class LayoutMode { Quad, Single };

    // scene: the resident render scene (borrowed; must outlive this widget).
    explicit XQMprWidget(XQRenderScene* scene, QWidget* parent = nullptr);
    ~XQMprWidget() override;

    XQMprWidget(const XQMprWidget&) = delete;
    XQMprWidget& operator=(const XQMprWidget&) = delete;

    // Slice count / current index for an axis (0 = x/Sagittal, 1 = y/Coronal,
    // 2 = z/Axial). Forwarded to the scene.
    int sliceCount(int axis) const;
    int currentSlice(int axis) const;

    // Layout mode. Single hides Sagittal/Coronal/3D and spans Axial across the
    // whole grid.
    void setLayoutMode(LayoutMode mode);
    LayoutMode layoutMode() const;

    // Repaints all four cells.
    void renderAll();

    // Crosshair visibility (forwarded to the scene, then repaint).
    void setCrosshairVisible(bool visible);

    // Seed picking (M2). Forwarded to the three slice views. B1b only stores the
    // flag downstream; behaviour arrives in B2.
    void setSeedPickingEnabled(bool enabled);
    void clearSeedMarker();

    // Picking-mode banner shown on the three slice views (empty text hides it).
    // Forwarded to each slice view's setModeHint.
    void setModeHint(const QString& text);

signals:
    // A wheel in one slice view changed that axis's slice index. Forwarded from
    // the slice views so the main window can sync sliders + re-render others.
    void sliceChanged(int axis, int index);
    // A seed pick resolved to a voxel in one of the slice views (forwarded from
    // each view's voxelPicked). The main window routes it by pick mode.
    void seedPicked(int i, int j, int k);
    // A pick in one of the slice views landed outside the image volume (forwarded
    // from each view's pickOutOfBounds). The main window shows a status-bar hint.
    void pickOutOfBounds();
    // A window/level drag finished in one of the slice views (forwarded). The
    // main window re-reads the scene's window/level into the navigator spins.
    void windowLevelChanged();

protected:
    // Repositions the corner axis-name overlays on frame resize.
    bool eventFilter(QObject* watched, QEvent* event) override;
    // Retranslates the axis-name overlays on a language switch.
    void changeEvent(QEvent* event) override;

private:
    void retranslateAxisNames();

    XQRenderScene* scene_;
    QGridLayout* grid_ = nullptr;
    LayoutMode layoutMode_ = LayoutMode::Quad;

    XQSliceViewWidget* axial_ = nullptr;
    XQSliceViewWidget* sagittal_ = nullptr;
    XQSliceViewWidget* coronal_ = nullptr;
    XQVolumeViewWidget* volume_ = nullptr;

    QFrame* axialFrame_ = nullptr;
    QFrame* sagittalFrame_ = nullptr;
    QFrame* coronalFrame_ = nullptr;
    QFrame* volumeFrame_ = nullptr;

    QLabel* axialName_ = nullptr;
    QLabel* sagittalName_ = nullptr;
    QLabel* coronalName_ = nullptr;
    QLabel* volumeName_ = nullptr;
};

} // namespace xq

#endif // XQ_VISUALIZATION_MPR_WIDGET_H
