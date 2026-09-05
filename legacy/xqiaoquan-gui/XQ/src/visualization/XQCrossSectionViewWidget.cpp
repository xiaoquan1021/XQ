#include "visualization/XQCrossSectionViewWidget.h"

#include "visualization/XQRenderScene.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLatin1String>
#include <QMouseEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyle.h>
#include <vtkInteractorStyleImage.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <cstddef>

namespace xq {
namespace {

constexpr int kMinimumCellSize = 160;

// True on the offscreen platform (headless ctest): no usable GL context, so the
// render window is left unmounted (the widget stays a structurally valid
// placeholder). Real desktop platforms mount it normally. Mirrors
// XQSliceViewWidget.
bool isOffscreenPlatform()
{
    return QGuiApplication::platformName() == QLatin1String("offscreen");
}

} // namespace

// VTK pipeline for the section view, built once. The reslice output feeds a
// vtkImageSlice; window/level is applied through the shared image property. Two
// resident line actors (built once, only their polydata is refreshed) draw the
// committed contours and the in-progress drawing preview on the z = 0 section
// plane, so section-local (u, v) mm maps straight to world (u, v, 0).
class XQCrossSectionViewWidget::Impl {
public:
    Impl()
    {
        renderer_->SetBackground(0.0, 0.0, 0.0);
        renderer_->GetActiveCamera()->ParallelProjectionOn();

        mapper_->SetInputData(nullptr);
        slice_->SetMapper(mapper_);
        slice_->SetProperty(property_);
        slice_->VisibilityOff();
        renderer_->AddViewProp(slice_);

        // Committed contours (already in the group): a calm cyan. Preview (draft
        // being drawn): a brighter yellow so the user sees what they are placing.
        committedMapper_->SetInputData(committedData_);
        committedActor_->SetMapper(committedMapper_);
        committedActor_->GetProperty()->SetColor(0.20, 0.85, 0.95);
        committedActor_->GetProperty()->SetLineWidth(1.5);
        committedActor_->GetProperty()->SetLighting(false);
        committedActor_->VisibilityOff();
        renderer_->AddActor(committedActor_);

        previewMapper_->SetInputData(previewData_);
        previewActor_->SetMapper(previewMapper_);
        previewActor_->GetProperty()->SetColor(0.98, 0.85, 0.20);
        previewActor_->GetProperty()->SetLineWidth(2.0);
        previewActor_->GetProperty()->SetPointSize(6.0);
        previewActor_->GetProperty()->SetLighting(false);
        previewActor_->VisibilityOff();
        renderer_->AddActor(previewActor_);

        // Threshold live preview (candidate loop while dragging the threshold
        // slider, not yet committed): a distinct orange dashed-look line, drawn
        // over the committed overlay so scrubbing shows the lumen grow/shrink.
        thresholdPreviewMapper_->SetInputData(thresholdPreviewData_);
        thresholdPreviewActor_->SetMapper(thresholdPreviewMapper_);
        thresholdPreviewActor_->GetProperty()->SetColor(0.98, 0.55, 0.10);
        thresholdPreviewActor_->GetProperty()->SetLineWidth(2.0);
        thresholdPreviewActor_->GetProperty()->SetLighting(false);
        thresholdPreviewActor_->VisibilityOff();
        renderer_->AddActor(thresholdPreviewActor_);

        // Styles the interactor swaps between: image (default pan/zoom/window
        // level) and an empty style engaged while drawing so VTK never steals the
        // press ahead of the eventFilter.
        imageStyle_ = vtkSmartPointer<vtkInteractorStyleImage>::New();
        drawStyle_ = vtkSmartPointer<vtkInteractorStyle>::New();
    }

    XQCrossSectionResampler resampler_;
    vtkNew<vtkRenderer> renderer_;
    vtkNew<vtkImageSliceMapper> mapper_;
    vtkNew<vtkImageSlice> slice_;
    vtkNew<vtkImageProperty> property_;
    bool cameraInitialized_ = false;

    // Committed / preview contour overlays (line loops on the section plane).
    vtkNew<vtkPolyData> committedData_;
    vtkNew<vtkPolyDataMapper> committedMapper_;
    vtkNew<vtkActor> committedActor_;
    vtkNew<vtkPolyData> previewData_;
    vtkNew<vtkPolyDataMapper> previewMapper_;
    vtkNew<vtkActor> previewActor_;
    vtkNew<vtkPolyData> thresholdPreviewData_;
    vtkNew<vtkPolyDataMapper> thresholdPreviewMapper_;
    vtkNew<vtkActor> thresholdPreviewActor_;

    vtkSmartPointer<vtkInteractorStyleImage> imageStyle_;
    vtkSmartPointer<vtkInteractorStyle> drawStyle_;
    bool drawStyleEngaged_ = false;
};

XQCrossSectionViewWidget::XQCrossSectionViewWidget(XQRenderScene* scene, QWidget* parent)
    : QWidget(parent)
    , scene_(scene)
    , vtkWidget_(new QVTKOpenGLNativeWidget(this))
    , impl_(new Impl())
{
    setMinimumSize(kMinimumCellSize, kMinimumCellSize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vtkWidget_->setMinimumSize(kMinimumCellSize, kMinimumCellSize);
    vtkWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vtkWidget_);

    // Mount our own render window carrying the section renderer. Skipped under
    // the offscreen platform (no GL context -> QVTK paintGL would crash).
    if (!isOffscreenPlatform()) {
        vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
        vtkWidget_->setRenderWindow(renderWindow.Get());
        renderWindow->AddRenderer(impl_->renderer_);

        // Default view-only interaction (pan/zoom/window-level). Drawing swaps in
        // the empty style so the eventFilter owns the press (setDrawMethod).
        if (renderWindow->GetInteractor() != nullptr) {
            renderWindow->GetInteractor()->SetInteractorStyle(impl_->imageStyle_);
        }
    }

    // Mouse events must reach us before the interactor (drawing control-point
    // placement). Mouse tracking makes button-free MouseMove flow so the polygon
    // rubber-band follows the cursor between clicks.
    vtkWidget_->setMouseTracking(true);
    vtkWidget_->installEventFilter(this);

    // Centered empty-state guidance banner (no volume / no path). The object
    // name is a stable test / retranslate anchor.
    emptyState_ = new QLabel(this);
    emptyState_->setObjectName(QStringLiteral("xqCrossSectionEmptyState"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setWordWrap(true);
    emptyState_->setStyleSheet(QStringLiteral(
        "QLabel { color: #E5E7EB; background: rgba(17, 24, 39, 0.82); "
        "border: none; border-radius: 6px; padding: 10px 16px; }"));
    emptyState_->setText(QCoreApplication::translate(
        "XQStageWidgets",
        "Select a centerline path, then drag the slider above to position the "
        "cross-section."));
    emptyState_->hide();

    refreshImage();
}

XQCrossSectionViewWidget::~XQCrossSectionViewWidget()
{
    delete impl_;
}

void XQCrossSectionViewWidget::refreshImage()
{
    void* image = scene_ != nullptr ? scene_->vtkImageDataHandle() : nullptr;
    impl_->resampler_.setImage(image);
    if (image == nullptr) {
        impl_->slice_->VisibilityOff();
        impl_->mapper_->SetInputData(nullptr);
        setEmptyStateVisible(true);
    } else {
        syncWindowLevel();
    }
    renderNow();
}

void XQCrossSectionViewWidget::setSampleFrame(const XQCrossSectionPose& pose)
{
    impl_->resampler_.setPose(pose);
    void* output = impl_->resampler_.outputImageHandle();
    if (output == nullptr) {
        impl_->slice_->VisibilityOff();
        impl_->mapper_->SetInputData(nullptr);
        setEmptyStateVisible(true);
        renderNow();
        return;
    }

    setEmptyStateVisible(false);
    vtkImageData* section = static_cast<vtkImageData*>(output);
    impl_->mapper_->SetInputData(section);
    impl_->slice_->VisibilityOn();
    syncWindowLevel();

    // Frame the section once; subsequent poses keep the same centered camera so
    // scrubbing does not jump. The section is always centered on pose.origin in
    // its own reslice coordinates, so a fixed camera stays valid.
    if (!impl_->cameraInitialized_) {
        impl_->renderer_->ResetCamera();
        impl_->cameraInitialized_ = true;
    }
    renderNow();
}

void XQCrossSectionViewWidget::syncWindowLevel()
{
    if (scene_ == nullptr) {
        return;
    }
    double window = 0.0;
    double level = 0.0;
    scene_->windowLevel(&window, &level);
    impl_->property_->SetColorWindow(window);
    impl_->property_->SetColorLevel(level);
}

void XQCrossSectionViewWidget::renderNow()
{
    if (!vtkWidget_->isValid()) {
        // No GL context yet (before first expose, or an offscreen QPA with no GL
        // as in headless ctest). Entering QVTK's paintGL without a context
        // crashes; do not queue a repaint. QVTK drives the first paint once the
        // widget is exposed.
        return;
    }
    if (vtkWidget_->renderWindow() != nullptr) {
        vtkWidget_->renderWindow()->Render();
    }
    vtkWidget_->update();
}

XQCrossSectionFrame XQCrossSectionViewWidget::lastFrame() const
{
    return impl_->resampler_.lastFrame();
}

void XQCrossSectionViewWidget::setEmptyStateVisible(bool visible)
{
    if (emptyState_ == nullptr) {
        return;
    }
    if (visible) {
        emptyState_->adjustSize();
        positionEmptyState();
        emptyState_->show();
        emptyState_->raise();
    } else {
        emptyState_->hide();
    }
}

void XQCrossSectionViewWidget::positionEmptyState()
{
    if (emptyState_ == nullptr) {
        return;
    }
    const int maxWidth = width() - 32;
    if (maxWidth > 0 && emptyState_->width() > maxWidth) {
        emptyState_->setFixedWidth(maxWidth);
        emptyState_->adjustSize();
    }
    const int x = (width() - emptyState_->width()) / 2;
    const int y = (height() - emptyState_->height()) / 2;
    emptyState_->move(x > 0 ? x : 0, y > 0 ? y : 0);
}

void XQCrossSectionViewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionEmptyState();
}

// --- Drawing layer ---------------------------------------------------------

namespace {

// Circle preview needs a smooth ring even from just two control points; sample
// it at this many segments (matches the app's ContourExtractionService default).
constexpr int kCirclePreviewSegments = 36;

// Fills `data` with one polyline per (u, v) loop, placed on the z = 0 section
// plane (world (u, v, 0)). `closed` wraps each loop back to its first point.
// A loop of a single point becomes a VTK vertex (so a lone circle center shows).
void buildLoopPolyData(vtkPolyData* data, const QVector<QVector<QPointF>>& loops,
                       bool closed)
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkCellArray> verts;

    for (int l = 0; l < loops.size(); ++l) {
        const QVector<QPointF>& loop = loops[l];
        if (loop.isEmpty()) {
            continue;
        }
        const vtkIdType base = points->GetNumberOfPoints();
        for (int i = 0; i < loop.size(); ++i) {
            points->InsertNextPoint(loop[i].x(), loop[i].y(), 0.0);
        }
        if (loop.size() == 1) {
            const vtkIdType id = base;
            verts->InsertNextCell(1, &id);
            continue;
        }
        const int segments = closed ? loop.size() : loop.size() - 1;
        for (int i = 0; i < segments; ++i) {
            const vtkIdType a = base + i;
            const vtkIdType b = base + ((i + 1) % loop.size());
            vtkIdType seg[2] = {a, b};
            lines->InsertNextCell(2, seg);
        }
    }

    data->SetPoints(points);
    data->SetLines(lines);
    data->SetVerts(verts);
    data->Modified();
}

} // namespace

DrawMethod XQCrossSectionViewWidget::drawMethod() const
{
    return drawMethod_;
}

void XQCrossSectionViewWidget::setDrawMethod(DrawMethod method)
{
    drawMethod_ = method;
    // Any pending draft is abandoned when the method changes (including on exit).
    draftPoints_.clear();
    circleDragging_ = false;

    engageDrawStyle(method != DrawMethod::None);

    // Clear the live preview; committed contours stay visible.
    impl_->previewActor_->VisibilityOff();
    if (vtkWidget_ != nullptr) {
        vtkWidget_->setCursor(method == DrawMethod::None ? Qt::ArrowCursor
                                                         : Qt::CrossCursor);
    }
    renderNow();
}

void XQCrossSectionViewWidget::engageDrawStyle(bool engage)
{
    impl_->drawStyleEngaged_ = engage;
    vtkRenderWindow* window = vtkWidget_ != nullptr ? vtkWidget_->renderWindow() : nullptr;
    vtkRenderWindowInteractor* interactor =
        window != nullptr ? window->GetInteractor() : nullptr;
    if (interactor != nullptr) {
        interactor->SetInteractorStyle(engage ? impl_->drawStyle_.Get()
                                              : impl_->imageStyle_.Get());
    }
}

bool XQCrossSectionViewWidget::drawStyleActive() const
{
    vtkRenderWindow* window = vtkWidget_ != nullptr ? vtkWidget_->renderWindow() : nullptr;
    vtkRenderWindowInteractor* interactor =
        window != nullptr ? window->GetInteractor() : nullptr;
    if (interactor != nullptr) {
        return interactor->GetInteractorStyle() == impl_->drawStyle_.Get();
    }
    return impl_->drawStyleEngaged_;
}

bool XQCrossSectionViewWidget::sectionPixels(std::vector<double>& gray, int& width,
                                             int& height, double& pixelSizeMm) const
{
    // Read the resampler's last resliced 2D section. Null until a pose has been
    // reslicing a real volume; outputs stay untouched then.
    void* output = impl_->resampler_.outputImageHandle();
    if (output == nullptr) {
        return false;
    }
    vtkImageData* section = static_cast<vtkImageData*>(output);
    int dims[3] = {0, 0, 0};
    section->GetDimensions(dims);
    if (dims[0] <= 0 || dims[1] <= 0) {
        return false;
    }

    width = dims[0];
    height = dims[1];
    pixelSizeMm = impl_->resampler_.outputSpec().pixelSizeMm;

    // Point-by-point read via GetScalarComponentAsDouble so the scalar type
    // (reslice may emit double or the source's short) is handled uniformly.
    // Row-major (idx = y*W + x); 200x200 has no perf concern. VTK vtkImageData
    // is (x fast, y slow), matching our idx.
    gray.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                0.0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                 + static_cast<std::size_t>(x)] =
                section->GetScalarComponentAsDouble(x, y, 0, 0);
        }
    }
    return true;
}

void XQCrossSectionViewWidget::setDisplayedContours(
    const QVector<QVector<QPointF>>& contours)
{
    buildLoopPolyData(impl_->committedData_, contours, /*closed=*/true);
    impl_->committedActor_->SetVisibility(contours.isEmpty() ? 0 : 1);
    renderNow();
}

void XQCrossSectionViewWidget::setPreviewContour(
    const QVector<QVector<QPointF>>& contours)
{
    buildLoopPolyData(impl_->thresholdPreviewData_, contours, /*closed=*/true);
    impl_->thresholdPreviewActor_->SetVisibility(contours.isEmpty() ? 0 : 1);
    renderNow();
}

bool XQCrossSectionViewWidget::displayToSection(const QPoint& pos, double* u, double* v) const
{
    if (isOffscreenPlatform() || vtkWidget_ == nullptr || !vtkWidget_->isValid()) {
        return false;
    }
    vtkRenderWindow* window = vtkWidget_->renderWindow();
    if (window == nullptr) {
        return false;
    }
    const int* size = window->GetSize();
    if (size == nullptr || size[1] <= 0) {
        return false;
    }
    // Qt logical pixels (top-left origin) -> VTK physical display pixels
    // (bottom-left origin): scale by device pixel ratio, flip y.
    const double dpr = vtkWidget_->devicePixelRatioF();
    const double dispX = pos.x() * dpr;
    const double dispY = static_cast<double>(size[1]) - 1.0 - pos.y() * dpr;

    vtkRenderer* renderer = impl_->renderer_;
    renderer->SetDisplayPoint(dispX, dispY, 0.0);
    renderer->DisplayToWorld();
    double w[4] = {0.0, 0.0, 0.0, 1.0};
    renderer->GetWorldPoint(w);
    double wx = w[0];
    double wy = w[1];
    if (w[3] != 0.0) {
        wx = w[0] / w[3];
        wy = w[1] / w[3];
    }
    // The section image sits in world coords with its center at (0, 0) on the
    // z = 0 plane (SetOutputOrigin(-half, -half)), so world (x, y) IS the
    // section-local (u, v) mm. No offset correction is needed.
    *u = wx;
    *v = wy;
    return true;
}

void XQCrossSectionViewWidget::updatePreview(double cursorU, double cursorV, bool hasCursor)
{
    QVector<QVector<QPointF>> loops;

    if (drawMethod_ == DrawMethod::Circle) {
        if (!draftPoints_.isEmpty()) {
            const QPointF center = draftPoints_[0];
            QPointF boundary = center;
            if (draftPoints_.size() >= 2) {
                boundary = draftPoints_[1];
            } else if (hasCursor) {
                boundary = QPointF(cursorU, cursorV);
            }
            const double radius =
                std::hypot(boundary.x() - center.x(), boundary.y() - center.y());
            if (radius > 0.0) {
                QVector<QPointF> ring;
                ring.reserve(kCirclePreviewSegments);
                const double twoPi = 2.0 * 3.14159265358979323846;
                for (int i = 0; i < kCirclePreviewSegments; ++i) {
                    const double a =
                        static_cast<double>(i) * twoPi / kCirclePreviewSegments;
                    ring.push_back(QPointF(center.x() + radius * std::cos(a),
                                           center.y() + radius * std::sin(a)));
                }
                loops.push_back(ring);
            } else {
                // Just the center placed: show it as a dot.
                loops.push_back(QVector<QPointF>{center});
            }
        }
    } else if (drawMethod_ == DrawMethod::Polygon) {
        if (!draftPoints_.isEmpty()) {
            QVector<QPointF> chain = draftPoints_;
            if (hasCursor) {
                chain.push_back(QPointF(cursorU, cursorV));
            }
            loops.push_back(chain);
        }
    }

    // Polygon draft is an open chain (not yet closed); circle preview is a ring.
    const bool closed = (drawMethod_ == DrawMethod::Circle);
    buildLoopPolyData(impl_->previewData_, loops, closed);
    impl_->previewActor_->SetVisibility(loops.isEmpty() ? 0 : 1);
    renderNow();
}

void XQCrossSectionViewWidget::cancelDraft()
{
    draftPoints_.clear();
    circleDragging_ = false;
    impl_->previewActor_->VisibilityOff();
    renderNow();
}

void XQCrossSectionViewWidget::finishContour()
{
    if (draftPoints_.isEmpty()) {
        return;
    }
    const QVector<QPointF> points = draftPoints_;
    const DrawMethod method = drawMethod_;
    draftPoints_.clear();
    circleDragging_ = false;
    impl_->previewActor_->VisibilityOff();
    renderNow();
    emit contourDrawn(method, points);
}

bool XQCrossSectionViewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != vtkWidget_ || drawMethod_ == DrawMethod::None) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        if (drawMethod_ == DrawMethod::SeedPick) {
            // Seed pick is a one-shot: report the clicked (u, v) and place no
            // contour (no draft, no finishContour, no contourDrawn). Consume the
            // press either way -- the empty draw style already took over the
            // interactor, so we must not fall through to the VTK interactor.
            double u = 0.0;
            double v = 0.0;
            if (displayToSection(mouse->pos(), &u, &v)) {
                emit seedPicked(u, v);
            }
            return true;
        }
        double u = 0.0;
        double v = 0.0;
        if (!displayToSection(mouse->pos(), &u, &v)) {
            return true; // consume: no valid map, but drawing owns the press
        }
        if (drawMethod_ == DrawMethod::Circle) {
            // Press places the center; the drag/release sets the boundary.
            draftPoints_.clear();
            draftPoints_.push_back(QPointF(u, v));
            circleDragging_ = true;
            updatePreview(u, v, true);
        } else { // Polygon
            draftPoints_.push_back(QPointF(u, v));
            updatePreview(u, v, false);
        }
        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        double u = 0.0;
        double v = 0.0;
        if (!displayToSection(mouse->pos(), &u, &v)) {
            return true;
        }
        if (drawMethod_ == DrawMethod::Circle && circleDragging_) {
            updatePreview(u, v, true);
            return true;
        }
        if (drawMethod_ == DrawMethod::Polygon && !draftPoints_.isEmpty()) {
            // Rubber-band the open chain to the cursor between clicks.
            updatePreview(u, v, true);
            return true;
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        if (drawMethod_ == DrawMethod::Circle && circleDragging_) {
            double u = 0.0;
            double v = 0.0;
            if (displayToSection(mouse->pos(), &u, &v)) {
                const QPointF center = draftPoints_[0];
                if (std::hypot(u - center.x(), v - center.y()) > 0.0) {
                    draftPoints_.push_back(QPointF(u, v));
                    finishContour();
                    return true;
                }
            }
            // Zero-radius (no drag): keep the center, wait for a real boundary.
            circleDragging_ = false;
        }
        return true;
    }
    case QEvent::MouseButtonDblClick: {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        // Double-click closes a polygon (needs >= 3 vertices). The extra press
        // that precedes the double-click already added the last vertex, so no
        // point is added here.
        if (drawMethod_ == DrawMethod::Polygon && draftPoints_.size() >= 3) {
            finishContour();
        }
        return true;
    }
    case QEvent::KeyPress: {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            // Abandon the current draft (keeps committed contours).
            cancelDraft();
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && drawMethod_ == DrawMethod::Polygon && draftPoints_.size() >= 3) {
            finishContour();
            return true;
        }
        return false;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace xq
