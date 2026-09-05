#ifndef XQ_VISUALIZATION_SLICE_VIEW_WIDGET_H
#define XQ_VISUALIZATION_SLICE_VIEW_WIDGET_H

#include <QWidget>

#include <memory>

class QVTKOpenGLNativeWidget;
class QEvent;
class QObject;
class QResizeEvent;
class QMouseEvent;
class QLabel;
class QPoint;

namespace xq {

class XQRenderScene;

// One MPR slice view: a QWidget wrapping a QVTKOpenGLNativeWidget whose render
// window mounts one of XQRenderScene's three slice renderers (chosen by axis).
// The scene owns the vtkImageSlice + crosshair actors; this widget only mounts
// the renderer, consumes the wheel to change the slice, and repaints.
//
// Header stays VTK-free: the QVTK widget (Qt+VTK) is forward-declared and held
// as a pointer; the renderer is mounted through XQRenderScene's opaque void*
// handle inside the .cpp.
class XQSliceViewWidget : public QWidget {
    Q_OBJECT

public:
    // scene: the resident render scene (borrowed; must outlive this widget).
    // axis: 0 = x/Sagittal, 1 = y/Coronal, 2 = z/Axial (the XQRenderScene
    // mapping).
    XQSliceViewWidget(XQRenderScene* scene, int axis, QWidget* parent = nullptr);
    ~XQSliceViewWidget() override;

    XQSliceViewWidget(const XQSliceViewWidget&) = delete;
    XQSliceViewWidget& operator=(const XQSliceViewWidget&) = delete;

    int axis() const { return axis_; }

    // Repaints this slice view after the scene changed.
    void renderNow();

    // Updates the bottom-right "idx / count" corner overlay from the scene's
    // current slice index / count for this axis.
    void refreshInfoOverlay();

    // Seed picking (M2). B1b only stores the flag; the wheel/pick behaviour and
    // the voxelPicked emission arrive in B2.
    void setSeedPickingEnabled(bool enabled);
    bool seedPickingEnabled() const { return seedPickingEnabled_; }
    void clearSeedMarker();

    // Probe: whether the empty pick interactor style is currently engaged (pick
    // mode took over from vtkInteractorStyleImage so a click has one deterministic
    // path). With a live interactor it reads the actual current style; headless
    // (offscreen, no interactor) it follows the pick-mode intent flag.
    bool pickStyleActive() const;

    // Top-centre picking-mode banner overlay. An empty text hides it; any
    // non-empty text shows it (translation-agnostic: the caller supplies the
    // already-translated string).
    void setModeHint(const QString& text);

signals:
    // Emitted when the wheel changes this axis's slice index. The main window
    // syncs the matching slider/spin and re-renders the other views.
    void sliceChanged(int axis, int index);
    // A left-click in seed-picking mode (not on a crosshair) resolved to a voxel.
    void voxelPicked(int i, int j, int k);
    // A left-click in seed-picking mode resolved to a world point that fell
    // outside the image volume (worldToVoxelIndex failed). The main window shows a
    // status-bar hint so the click no longer looks silently ignored.
    void pickOutOfBounds();
    // A left-drag window/level interaction finished (release after a non-pick,
    // non-crosshair-drag left press). The receiver reads the scene's new
    // window/level; no payload.
    void windowLevelChanged();

protected:
    // Intercepts the QVTK widget's wheel events to step the slice instead of
    // letting the image interactor zoom, the mouse events to drag the crosshair
    // lines (B2a), and repositions the corner overlay on resize.
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void positionInfoOverlay();
    // Centres the mode-hint banner near the top of the view.
    void positionModeHint();

    // --- crosshair drag (B2a) ---
    // Whether the cursor is over this view's crosshair centre. hitLine(pos)
    // returns 3 only when BOTH lines are within tolerance (i.e. near the
    // intersection), else 0 -- a single line is not grabbable, so a drag always
    // moves the centre in both axes at once (B4c). Returns 0 with no volume / no
    // GL context.
    int hitLine(const QPoint& pos) const;
    // The world coordinate under a display point on this view's focal plane.
    // Returns false (world untouched) with no volume / no GL context.
    bool displayToWorld(const QPoint& pos, double world[3]) const;
    // Applies a drag: for each dragged line, maps the cursor world coordinate to
    // that line's axis slice index and, if changed, moves the slice + emits
    // sliceChanged. Repaints this view.
    void dragTo(const QPoint& pos);
    // Sets the cursor from a hit mask: a small cross over the crosshair centre
    // (mask 3), the default cursor otherwise.
    void applyHitCursor(int hitMask);

    XQRenderScene* scene_;
    int axis_;
    QVTKOpenGLNativeWidget* vtkWidget_;
    QLabel* info_ = nullptr;
    // Top-centre picking-mode banner; hidden unless setModeHint gets a non-empty
    // string.
    QLabel* modeHint_ = nullptr;
    bool seedPickingEnabled_ = false;

    // The two interactor styles this view swaps between: the resident
    // window/level image style (default) and an empty style engaged in pick mode
    // so VTK never steals the press. Held in a shallow pimpl so the header stays
    // VTK-free while both styles keep a stable owner (the interactor only borrows
    // one at a time; the other must not be collected).
    struct StyleHolder;
    std::unique_ptr<StyleHolder> styles_;
    // Tracks whether pick mode has engaged the empty style. Mirrors the actual
    // interactor style when there is one; the sole source of truth headless (no
    // interactor to read).
    bool pickStyleEngaged_ = false;

    // Non-zero while a crosshair drag is in progress; the bitmask of the grabbed
    // line(s) (same encoding as hitLine).
    int draggingLines_ = 0;

    // True between a left press that was left to VTK (not a pick, not a crosshair
    // grab -> a window/level drag) and its release, so the release can emit
    // windowLevelChanged for the navigator spins to refresh.
    bool windowLevelDragging_ = false;
};

} // namespace xq

#endif // XQ_VISUALIZATION_SLICE_VIEW_WIDGET_H
