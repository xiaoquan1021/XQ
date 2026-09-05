#include "visualization/XQSliceViewWidget.h"

#include "visualization/XQRenderScene.h"

#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyle.h>
#include <vtkInteractorStyleImage.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <cmath>

namespace xq {
namespace {

constexpr int kMinimumSliceCellSize = 96;

// Pixel tolerance for grabbing a crosshair line with the cursor.
constexpr double kCrosshairHitTolerance = 8.0;

// axis 0 = x/Sagittal, 1 = y/Coronal, 2 = z/Axial (XQRenderScene mapping).
ViewId viewForAxis(int axis)
{
    switch (axis) {
    case 0:
        return ViewId::Sagittal;
    case 1:
        return ViewId::Coronal;
    default:
        return ViewId::Axial;
    }
}

// The corner overlay object names predate the rewrite; the structural tests
// (test_main_window) key on them, so they are kept verbatim.
const char* infoObjectName(int axis)
{
    switch (axis) {
    case 0:
        return "xqMprInfoSagittal";
    case 1:
        return "xqMprInfoCoronal";
    default:
        return "xqMprInfoAxial";
    }
}

// True on the offscreen platform (headless ctest): no usable GL context, so
// QVTKOpenGLNativeWidget cannot create one and paintGL would crash. The render
// window is left unmounted there (the widget is still a structurally valid
// placeholder). Real desktop platforms mount it normally.
bool isOffscreenPlatform()
{
    return QGuiApplication::platformName() == QLatin1String("offscreen");
}

// The render window's physical height (VTK display has a bottom-left origin), or
// 0 when the renderer/window is unavailable.
double renderWindowHeight(vtkRenderer* renderer)
{
    if (renderer == nullptr || renderer->GetRenderWindow() == nullptr) {
        return 0.0;
    }
    const int* size = renderer->GetRenderWindow()->GetSize();
    return size != nullptr ? static_cast<double>(size[1]) : 0.0;
}

} // namespace

// Owns the two interactor styles the view swaps between. image is the default
// window/level style; pick is an empty base style that does nothing, engaged in
// pick mode so VTK never window/levels or steals the press from the eventFilter.
struct XQSliceViewWidget::StyleHolder {
    vtkSmartPointer<vtkInteractorStyleImage> image;
    vtkSmartPointer<vtkInteractorStyle> pick;
};

XQSliceViewWidget::XQSliceViewWidget(XQRenderScene* scene, int axis, QWidget* parent)
    : QWidget(parent)
    , scene_(scene)
    , axis_(axis)
    , vtkWidget_(new QVTKOpenGLNativeWidget(this))
{
    setMinimumSize(kMinimumSliceCellSize, kMinimumSliceCellSize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vtkWidget_->setMinimumSize(kMinimumSliceCellSize, kMinimumSliceCellSize);
    vtkWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vtkWidget_);

    // Both interactor styles are built unconditionally (even offscreen, so probes
    // are well-defined): the default window/level image style and an empty style
    // engaged in pick mode. The interactor only ever borrows one; the holder keeps
    // the other alive so a style swap never collects a still-needed style.
    styles_ = std::make_unique<StyleHolder>();
    styles_->image = vtkSmartPointer<vtkInteractorStyleImage>::New();
    styles_->pick = vtkSmartPointer<vtkInteractorStyle>::New();

    // Mount the scene's slice renderer for this axis on our own render window
    // (four widgets = four GL contexts; the renderer is per-view, the heavy data
    // is shared inside the scene). vtkRendererHandle documents its void* as a
    // vtkRenderer*; casting it here is the agreed sink-file contract. Skipped
    // under the offscreen platform (no GL context -> QVTK paintGL would crash).
    if (!isOffscreenPlatform()) {
        vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
        vtkWidget_->setRenderWindow(renderWindow.Get());
        vtkRenderer* renderer =
            static_cast<vtkRenderer*>(scene_->vtkRendererHandle(viewForAxis(axis_)));
        if (renderer != nullptr) {
            renderWindow->AddRenderer(renderer);
        }

        // 2D image interaction: left-drag = window/level, right/scroll = zoom (B2
        // refines). The wheel is intercepted below to step the slice instead. Pick
        // mode swaps this out for the empty style (setSeedPickingEnabled).
        if (renderWindow->GetInteractor() != nullptr) {
            renderWindow->GetInteractor()->SetInteractorStyle(styles_->image);
        }
    }

    // Bottom-right "idx / count" overlay.
    info_ = new QLabel(this);
    info_->setObjectName(infoObjectName(axis_));
    info_->setStyleSheet(QStringLiteral(
        "QLabel { color: #FFFFFF; background: rgba(31, 41, 55, 0.72); "
        "border: none; border-radius: 3px; padding: 2px; }"));
    info_->raise();

    // Top-centre picking-mode banner. Hidden until setModeHint gets a non-empty
    // string; the object name is a stable test anchor.
    modeHint_ = new QLabel(this);
    modeHint_->setObjectName(QStringLiteral("xqSliceModeHint"));
    modeHint_->setStyleSheet(QStringLiteral(
        "QLabel { color: #FFFFFF; background: rgba(17, 24, 39, 0.82); "
        "border: none; border-radius: 4px; padding: 4px 8px; }"));
    modeHint_->hide();
    modeHint_->raise();

    // Wheel + mouse need to reach us before the interactor; watch the QVTK
    // widget. Mouse tracking makes button-free MouseMove events flow so the
    // crosshair hover cursor updates without a held button.
    vtkWidget_->setMouseTracking(true);
    vtkWidget_->installEventFilter(this);

    refreshInfoOverlay();
}

XQSliceViewWidget::~XQSliceViewWidget() = default;

void XQSliceViewWidget::renderNow()
{
    if (!vtkWidget_->isValid()) {
        // No GL context yet (before first expose, or an offscreen QPA with no GL
        // as in headless ctest). Do not even queue a repaint: entering QVTK's
        // paintGL without a context crashes. Once the widget is exposed, QVTK
        // drives the first paint itself.
        return;
    }
    if (vtkWidget_->renderWindow() != nullptr) {
        vtkWidget_->renderWindow()->Render();
    }
    vtkWidget_->update();
}

void XQSliceViewWidget::refreshInfoOverlay()
{
    if (info_ == nullptr || scene_ == nullptr) {
        return;
    }
    const int count = scene_->sliceCount(axis_);
    const int index = scene_->sliceIndex(axis_);
    if (count <= 0 || index < 0) {
        info_->clear();
        return;
    }
    // 1-based display: "idx / count".
    info_->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(count));
    info_->adjustSize();
    positionInfoOverlay();
    info_->raise();
}

void XQSliceViewWidget::setSeedPickingEnabled(bool enabled)
{
    seedPickingEnabled_ = enabled;

    // Take over VTK interaction: in pick mode swap in the empty style so the
    // interactor never window/levels or grabs the press ahead of our eventFilter
    // (the native-GL press-order race that made seed markers appear only
    // sometimes); on exit restore the window/level image style. The flag mirrors
    // this and is the sole source of truth offscreen (no interactor to read).
    pickStyleEngaged_ = enabled;
    vtkRenderWindow* window = vtkWidget_->renderWindow();
    vtkRenderWindowInteractor* interactor =
        window != nullptr ? window->GetInteractor() : nullptr;
    if (interactor != nullptr && styles_) {
        interactor->SetInteractorStyle(enabled ? styles_->pick.Get()
                                               : styles_->image.Get());
    }

    // Cross cursor in pick mode; back to the default arrow otherwise (the hover
    // handler further refines it over a crosshair line).
    if (enabled) {
        vtkWidget_->setCursor(Qt::CrossCursor);
    } else {
        vtkWidget_->unsetCursor();
    }
}

bool XQSliceViewWidget::pickStyleActive() const
{
    // With a live interactor, read the truth from VTK: is the empty pick style the
    // one currently installed. Headless (offscreen, no interactor), fall back to
    // the intent flag so the invariant still holds under ctest.
    vtkRenderWindow* window = vtkWidget_->renderWindow();
    vtkRenderWindowInteractor* interactor =
        window != nullptr ? window->GetInteractor() : nullptr;
    if (interactor != nullptr && styles_) {
        return interactor->GetInteractorStyle() == styles_->pick.Get();
    }
    return pickStyleEngaged_;
}

void XQSliceViewWidget::clearSeedMarker()
{
    if (scene_ != nullptr) {
        scene_->clearSeedMarker();
    }
    renderNow();
}

void XQSliceViewWidget::setModeHint(const QString& text)
{
    if (modeHint_ == nullptr) {
        return;
    }
    if (text.isEmpty()) {
        modeHint_->hide();
        return;
    }
    modeHint_->setText(text);
    modeHint_->adjustSize();
    positionModeHint();
    modeHint_->show();
    modeHint_->raise();
}

bool XQSliceViewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != vtkWidget_ || scene_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Wheel: {
        QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
        const int steps = wheel->angleDelta().y() > 0 ? 1 : -1;
        const int count = scene_->sliceCount(axis_);
        if (count > 0) {
            const int next = scene_->sliceIndex(axis_) + steps;
            scene_->setSliceIndex(axis_, next);
            const int applied = scene_->sliceIndex(axis_);
            refreshInfoOverlay();
            renderNow();
            emit sliceChanged(axis_, applied);
        }
        // Consume: the image interactor must not also zoom on the wheel.
        return true;
    }
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            const int mask = hitLine(mouse->pos());
            if (mask != 0) {
                // Grab the line(s); consume so the image interactor does not also
                // window/level on this drag.
                draggingLines_ = mask;
                return true;
            }
            if (seedPickingEnabled_) {
                // Seed pick: click -> world -> voxel -> voxelPicked. Consume so
                // VTK does not also window/level in pick mode. displayToWorld is
                // false with no volume / no GL context -> nothing emitted.
                double world[3] = {0.0, 0.0, 0.0};
                int i = 0;
                int j = 0;
                int k = 0;
                if (displayToWorld(mouse->pos(), world)) {
                    // displayToWorld only resolves the two in-plane axes; the third
                    // (this view's slice-normal axis == axis_) comes back at the
                    // camera's focal depth, not this slice. Pin it to the current
                    // slice plane so the picked point lands on the slice the user is
                    // looking at (else every point collapses onto one plane, e.g. the
                    // volume's far x edge in the sagittal view). Mirrors hitLine's
                    // per-line pin at :457.
                    world[axis_] = scene_->sliceWorldCoord(axis_);
                    if (scene_->worldToVoxelIndex(world, &i, &j, &k)) {
                        emit voxelPicked(i, j, k);
                    } else {
                        // Clicked past the image edge: the world point is off the
                        // volume. Signal it so the main window can hint instead of
                        // the click looking silently ignored.
                        emit pickOutOfBounds();
                    }
                }
                return true;
            }
            // A plain left press (not a pick, not a crosshair grab): VTK will
            // window/level; remember it so the release notifies the navigator.
            windowLevelDragging_ = true;
        }
        // Not on a crosshair (or not the left button): let VTK handle it
        // (window/level, pan, zoom).
        return false;
    }
    case QEvent::MouseMove: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (draggingLines_ != 0) {
            dragTo(mouse->pos());
            return true;
        }
        // Hover: update the cursor from what is under it, but let VTK keep any
        // active interaction (it only runs a button is held, which we are not).
        const int hover = hitLine(mouse->pos());
        if (seedPickingEnabled_ && hover == 0) {
            // Keep the pick cross where there is no crosshair to grab.
            vtkWidget_->setCursor(Qt::CrossCursor);
        } else {
            applyHitCursor(hover);
        }
        return false;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && draggingLines_ != 0) {
            draggingLines_ = 0;
            applyHitCursor(hitLine(mouse->pos()));
            return true;
        }
        if (mouse->button() == Qt::LeftButton && windowLevelDragging_) {
            // A window/level drag just ended (VTK already applied it to the shared
            // image property); tell the navigator to re-read the new values.
            windowLevelDragging_ = false;
            emit windowLevelChanged();
        }
        return false;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void XQSliceViewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionInfoOverlay();
    positionModeHint();
}

void XQSliceViewWidget::positionInfoOverlay()
{
    if (info_ == nullptr) {
        return;
    }
    constexpr int margin = 4;
    const int x = width() - info_->width() - margin;
    const int y = height() - info_->height() - margin;
    info_->move(x, y);
}

void XQSliceViewWidget::positionModeHint()
{
    if (modeHint_ == nullptr) {
        return;
    }
    constexpr int margin = 6;
    const int x = (width() - modeHint_->width()) / 2;
    modeHint_->move(x > margin ? x : margin, margin);
}

// --- crosshair drag (B2a) --------------------------------------------------
//
// Coordinate convention: all VTK display maths runs in the render window's
// physical pixels (its GetSize()). A Qt mouse pos() is in logical pixels with a
// top-left origin, so it is scaled by the widget device-pixel-ratio and the y is
// flipped (VTK display has a bottom-left origin) before use.

bool XQSliceViewWidget::displayToWorld(const QPoint& pos, double world[3]) const
{
    if (scene_ == nullptr || isOffscreenPlatform() || !scene_->hasVolume()) {
        return false;
    }
    vtkRenderer* renderer =
        static_cast<vtkRenderer*>(scene_->vtkRendererHandle(viewForAxis(axis_)));
    const double winH = renderWindowHeight(renderer);
    if (renderer == nullptr || winH <= 0.0) {
        return false;
    }
    const double dpr = vtkWidget_->devicePixelRatioF();
    const double dispX = pos.x() * dpr;
    const double dispY = winH - 1.0 - pos.y() * dpr;
    renderer->SetDisplayPoint(dispX, dispY, 0.0);
    renderer->DisplayToWorld();
    double w[4] = {0.0, 0.0, 0.0, 1.0};
    renderer->GetWorldPoint(w);
    if (w[3] != 0.0) {
        world[0] = w[0] / w[3];
        world[1] = w[1] / w[3];
        world[2] = w[2] / w[3];
    } else {
        world[0] = w[0];
        world[1] = w[1];
        world[2] = w[2];
    }
    return true;
}

int XQSliceViewWidget::hitLine(const QPoint& pos) const
{
    if (scene_ == nullptr || isOffscreenPlatform() || !scene_->hasVolume()) {
        return 0;
    }
    vtkRenderer* renderer =
        static_cast<vtkRenderer*>(scene_->vtkRendererHandle(viewForAxis(axis_)));
    const double winH = renderWindowHeight(renderer);
    if (renderer == nullptr || winH <= 0.0) {
        return 0;
    }
    // The cursor's world position on the focal plane; each line's world point is
    // the cursor world with that line's represented-axis component pinned to the
    // slice plane (parallel projection -> the nearest point on the line).
    double cursorWorld[3] = {0.0, 0.0, 0.0};
    if (!displayToWorld(pos, cursorWorld)) {
        return 0;
    }
    const double dpr = vtkWidget_->devicePixelRatioF();
    const double cursorDispX = pos.x() * dpr;
    const double cursorDispY = winH - 1.0 - pos.y() * dpr;
    const double tol = kCrosshairHitTolerance * dpr;

    int mask = 0;
    for (int line = 0; line < 2; ++line) {
        const int lineAxis = XQRenderScene::crosshairLineAxis(axis_, line);
        if (lineAxis < 0) {
            continue;
        }
        double lineWorld[3] = {cursorWorld[0], cursorWorld[1], cursorWorld[2]};
        lineWorld[lineAxis] = scene_->sliceWorldCoord(lineAxis);
        renderer->SetWorldPoint(lineWorld[0], lineWorld[1], lineWorld[2], 1.0);
        renderer->WorldToDisplay();
        double disp[3] = {0.0, 0.0, 0.0};
        renderer->GetDisplayPoint(disp);
        const double dx = disp[0] - cursorDispX;
        const double dy = disp[1] - cursorDispY;
        if (std::sqrt(dx * dx + dy * dy) <= tol) {
            mask |= (1 << line);
        }
    }
    // Only the intersection (both lines within tolerance) is grabbable: a single
    // line no longer catches the cursor, so a drag always moves the centre point
    // in both axes at once (SV-style). Anything short of the crosshair centre is a
    // miss.
    return mask == 3 ? 3 : 0;
}

void XQSliceViewWidget::dragTo(const QPoint& pos)
{
    if (scene_ == nullptr || draggingLines_ == 0) {
        return;
    }
    double world[3] = {0.0, 0.0, 0.0};
    if (!displayToWorld(pos, world)) {
        return;
    }
    bool changed = false;
    for (int line = 0; line < 2; ++line) {
        if ((draggingLines_ & (1 << line)) == 0) {
            continue;
        }
        const int lineAxis = XQRenderScene::crosshairLineAxis(axis_, line);
        if (lineAxis < 0) {
            continue;
        }
        const int idx = scene_->worldToSliceIndex(lineAxis, world[lineAxis]);
        if (idx < 0 || idx == scene_->sliceIndex(lineAxis)) {
            continue;
        }
        scene_->setSliceIndex(lineAxis, idx);
        emit sliceChanged(lineAxis, idx);
        changed = true;
    }
    if (changed) {
        // The dragged line's own axis view is a *different* window; this window's
        // image does not move, but its crosshair did (setSliceIndex updated it),
        // so repaint here. The other views are repainted by the main window's
        // sliceChanged slot.
        renderNow();
    }
}

void XQSliceViewWidget::applyHitCursor(int hitMask)
{
    // Only the crosshair centre is grabbable (hitLine returns 3 or 0). Over it,
    // show a small cross that marks the drag point without obscuring it (not the
    // bulky SizeAllCursor); anywhere else, restore the default cursor.
    if (hitMask == 3) {
        vtkWidget_->setCursor(Qt::CrossCursor);
    } else {
        vtkWidget_->unsetCursor();
    }
}

} // namespace xq
