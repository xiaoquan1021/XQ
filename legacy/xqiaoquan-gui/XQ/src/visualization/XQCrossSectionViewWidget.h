#ifndef XQ_VISUALIZATION_CROSS_SECTION_VIEW_WIDGET_H
#define XQ_VISUALIZATION_CROSS_SECTION_VIEW_WIDGET_H

#include <QPointF>
#include <QVector>
#include <QWidget>

#include <vector>

#include "visualization/XQCrossSectionResampler.h"

class QVTKOpenGLNativeWidget;
class QLabel;
class QObject;
class QEvent;
class QPoint;

namespace xq {

class XQRenderScene;

// Which manual contour a click/drag on the section builds. None = view-only
// (default): the section pans/zooms as before and no drawing happens. POD enum,
// VTK-free so it can live in the header and travel through the contourDrawn
// signal.
enum class DrawMethod {
    None,
    Circle,
    Polygon,
    SeedPick,
};

// Cross-section workbench view: mounts a resident vtkImageReslice pipeline (via
// XQCrossSectionResampler) that cuts a 2D section perpendicular to a path
// tangent out of the scene's resident volume, and shows it through a
// vtkImageSlice. The reslice pipeline and the vtkRenderer are built once;
// setSampleFrame only updates the reslice pose + re-renders -- scrubbing the
// along-path slider never rebuilds the GL context / renderer (P3-1 anti-jank
// contract). Window/level is synced from the main scene.
//
// Beyond displaying the section it hosts a manual drawing layer (P3-2): with a
// draw method engaged, clicks/drags on the section build a circle or polygon in
// the section's local (u, v) mm coordinates and emit contourDrawn when finished;
// the app maps those points to world space and adds an XQContour.
//
// Header stays VTK-free: the QVTK widget (Qt+VTK) is forward-declared and held
// as a pointer; the renderer / slice / resampler / drawing actors live in the
// .cpp.
class XQCrossSectionViewWidget : public QWidget {
    Q_OBJECT

public:
    // scene: the resident render scene (borrowed; supplies the resident
    // vtkImageData and the shared window/level; must outlive this widget).
    explicit XQCrossSectionViewWidget(XQRenderScene* scene, QWidget* parent = nullptr);
    ~XQCrossSectionViewWidget() override;

    XQCrossSectionViewWidget(const XQCrossSectionViewWidget&) = delete;
    XQCrossSectionViewWidget& operator=(const XQCrossSectionViewWidget&) = delete;

    // Re-attaches the scene's current resident vtkImageData to the resampler
    // (call after the scene's volume changed). Clears the section when there is
    // no volume and shows the empty-state hint.
    void refreshImage();

    // Reslices at the given path sample pose and repaints. No-op section (empty
    // state shown) when the scene has no volume. The section plane is
    // perpendicular to pose.tangent (see XQCrossSectionResampler).
    void setSampleFrame(const XQCrossSectionPose& pose);

    // Pulls the scene's current window/level onto the section's image property
    // and repaints (kept simple for this batch: shared values, one-way sync).
    void syncWindowLevel();

    // Repaints the section view.
    void renderNow();

    // The frame the last setSampleFrame produced (forwarded from the resampler;
    // read-only probe for tests / downstream contour mapping).
    XQCrossSectionFrame lastFrame() const;

    // Engages a drawing method. None (default) restores the view-only image
    // interactor style; Circle / Polygon swap in an empty style so the drawing
    // eventFilter owns the press, and clear any in-progress control points.
    void setDrawMethod(DrawMethod method);
    DrawMethod drawMethod() const;

    // Replaces the committed-contour overlay: `contours` are section-local 2D
    // (u, v) mm loops the app already added to the group; the view draws them as
    // closed line loops on top of the slice. Passing an empty list clears the
    // overlay. Only re-renders (never rebuilds the pipeline).
    void setDisplayedContours(const QVector<QVector<QPointF>>& contours);

    // Replaces the threshold live-preview overlay (candidate loop drawn while
    // dragging the threshold slider, before it is committed to the group), in a
    // distinct color over the committed overlay. Section-local 2D (u, v) mm
    // loops; an empty list clears the preview. Only re-renders.
    void setPreviewContour(const QVector<QVector<QPointF>>& contours);

    // Probe: whether the empty drawing interactor style is currently engaged
    // (drawing took over from vtkInteractorStyleImage). With a live interactor it
    // reads the actual current style; headless it follows the draw-method intent.
    bool drawStyleActive() const;

    // Copies the current resliced section's grayscale into `gray` (row-major,
    // idx = y*W + x) and reports its dimensions + pixel size (mm). Returns false
    // (outputs untouched) when there is no resliced section (no volume / no pose
    // yet). Used by the app to run threshold segmentation on the section. Reads
    // the resampler's output vtkImageData; the VTK access lives in the .cpp so
    // the header stays VTK-free.
    bool sectionPixels(std::vector<double>& gray, int& width, int& height,
                       double& pixelSizeMm) const;

signals:
    // Emitted when the user finishes a contour. `method` is the method that drew
    // it; `points` are the section-local 2D (u, v) mm control points (circle:
    // {center, boundary}; polygon: the clicked vertices). The app maps them
    // through unprojectFromFrame using lastFrame() and adds an XQContour at the
    // current arc length.
    void contourDrawn(xq::DrawMethod method, const QVector<QPointF>& points);

    // Emitted when the user clicks a seed point on the section while in the
    // SeedPick draw method. (u, v) is the section-local mm point clicked. The
    // app stores it as the current segmentation seed for threshold / region
    // grow; unlike contourDrawn this places no contour.
    void seedPicked(double u, double v);

private:
    // Shows/hides the centered empty-state guidance banner (no volume / no path).
    void setEmptyStateVisible(bool visible);
    void positionEmptyState();

protected:
    void resizeEvent(class QResizeEvent* event) override;
    // Intercepts the QVTK widget's mouse events to place drawing control points
    // when a draw method is engaged (view-only otherwise).
    bool eventFilter(QObject* watched, class QEvent* event) override;

private:
    XQRenderScene* scene_;
    QVTKOpenGLNativeWidget* vtkWidget_;
    QLabel* emptyState_ = nullptr;

    DrawMethod drawMethod_ = DrawMethod::None;
    // Control points placed so far in the current drawing, in section-local
    // (u, v) mm. Circle: [center] after press, [center, boundary] on release.
    // Polygon: one entry per clicked vertex.
    QVector<QPointF> draftPoints_;
    // True between a circle's center press and its boundary release.
    bool circleDragging_ = false;

    class Impl;
    // Owns the VTK resampler + renderer + slice actor + drawing overlay (built
    // once).
    Impl* impl_;

    // --- drawing helpers (defined in the .cpp) ---
    // Maps a Qt widget mouse position to the section-local (u, v) mm point under
    // it. Returns false (uv untouched) with no GL context / offscreen.
    bool displayToSection(const class QPoint& pos, double* u, double* v) const;
    // Rebuilds the preview overlay (current draft + rubber-band under the cursor)
    // and re-renders. cursorU/cursorV is the live cursor point (ignored when
    // hasCursor is false).
    void updatePreview(double cursorU, double cursorV, bool hasCursor);
    // Clears the in-progress draft + preview overlay (does not touch committed
    // contours) and re-renders.
    void cancelDraft();
    // Emits contourDrawn(method, draftPoints_) and clears the draft.
    void finishContour();
    // Swaps the interactor style to the empty drawing style (engage) or back to
    // the image style; keeps the intent flag in sync for headless probes.
    void engageDrawStyle(bool engage);
};

} // namespace xq

#endif // XQ_VISUALIZATION_CROSS_SECTION_VIEW_WIDGET_H
