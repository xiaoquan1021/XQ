#include "visualization/XQMprWidget.h"

#include "visualization/XQRenderScene.h"
#include "visualization/XQSliceViewWidget.h"
#include "visualization/XQVolumeViewWidget.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace xq {
namespace {

constexpr int kMinimumCellSize = 96;

// Coloured 1px border on a frame, black background behind the mounted view. The
// id-scoped selector keeps the border off descendant widgets.
void styleFrame(QFrame* frame, const char* objectName, const char* borderColor)
{
    frame->setObjectName(QString::fromLatin1(objectName));
    frame->setMinimumSize(kMinimumCellSize, kMinimumCellSize);
    frame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Expanding,
                                     QSizePolicy::Frame));
    frame->setStyleSheet(
        QString("QFrame#%1 { border: 1px solid %2; background-color: #000000; }")
            .arg(QString::fromLatin1(objectName),
                 QString::fromLatin1(borderColor)));
}

// Small white axis-name overlay anchored to a frame's bottom-left corner (MITK
// style). `border: none` stops the frame's coloured-border rule leaking onto it
// (QLabel is-a QFrame).
QLabel* makeAxisLabel(QWidget* parent, const QString& text)
{
    QLabel* label = new QLabel(text, parent);
    label->setStyleSheet(
        "QLabel { color: #FFFFFF; background: rgba(31, 41, 55, 0.72); "
        "border: none; border-radius: 3px; padding: 1px 4px; "
        "font-size: 11px; }");
    return label;
}

void positionAxisName(QFrame* frame, QLabel* name)
{
    if (frame == nullptr || name == nullptr) {
        return;
    }
    name->adjustSize();
    constexpr int margin = 3;
    name->move(margin, frame->height() - name->height() - margin);
    name->raise();
}

} // namespace

XQMprWidget::XQMprWidget(XQRenderScene* scene, QWidget* parent)
    : QWidget(parent)
    , scene_(scene)
{
    setObjectName(QStringLiteral("xqMprView"));

    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid_ = grid;

    // Axial (top-left, red).
    axialFrame_ = new QFrame(this);
    styleFrame(axialFrame_, "xqMprAxialFrame", "#C43C3C");
    axial_ = new XQSliceViewWidget(scene_, 2, axialFrame_);
    axial_->setObjectName(QStringLiteral("xqMprAxial"));
    {
        QVBoxLayout* l = new QVBoxLayout(axialFrame_);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(axial_);
    }
    axialName_ = makeAxisLabel(axialFrame_, tr("Axial"));
    axialFrame_->installEventFilter(this);

    // Sagittal (top-right, green).
    sagittalFrame_ = new QFrame(this);
    styleFrame(sagittalFrame_, "xqMprSagittalFrame", "#3C9C4A");
    sagittal_ = new XQSliceViewWidget(scene_, 0, sagittalFrame_);
    sagittal_->setObjectName(QStringLiteral("xqMprSagittal"));
    {
        QVBoxLayout* l = new QVBoxLayout(sagittalFrame_);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(sagittal_);
    }
    sagittalName_ = makeAxisLabel(sagittalFrame_, tr("Sagittal"));
    sagittalFrame_->installEventFilter(this);

    // Coronal (bottom-left, blue).
    coronalFrame_ = new QFrame(this);
    styleFrame(coronalFrame_, "xqMprCoronalFrame", "#3C6CC4");
    coronal_ = new XQSliceViewWidget(scene_, 1, coronalFrame_);
    coronal_->setObjectName(QStringLiteral("xqMprCoronal"));
    {
        QVBoxLayout* l = new QVBoxLayout(coronalFrame_);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(coronal_);
    }
    coronalName_ = makeAxisLabel(coronalFrame_, tr("Coronal"));
    coronalFrame_->installEventFilter(this);

    // 3D (bottom-right, yellow).
    volumeFrame_ = new QFrame(this);
    styleFrame(volumeFrame_, "xqMprVolumeFrame", "#C4913C");
    volume_ = new XQVolumeViewWidget(scene_, volumeFrame_);
    volume_->setObjectName(QStringLiteral("xqRenderWidget"));
    {
        QVBoxLayout* l = new QVBoxLayout(volumeFrame_);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(volume_);
    }
    volumeName_ = makeAxisLabel(volumeFrame_, QStringLiteral("3D"));
    volumeFrame_->installEventFilter(this);

    grid->addWidget(axialFrame_, 0, 0);
    grid->addWidget(sagittalFrame_, 0, 1);
    grid->addWidget(coronalFrame_, 1, 0);
    grid->addWidget(volumeFrame_, 1, 1);

    // Forward each slice view's wheel-driven slice change up to the main window.
    connect(axial_, &XQSliceViewWidget::sliceChanged,
            this, &XQMprWidget::sliceChanged);
    connect(sagittal_, &XQSliceViewWidget::sliceChanged,
            this, &XQMprWidget::sliceChanged);
    connect(coronal_, &XQSliceViewWidget::sliceChanged,
            this, &XQMprWidget::sliceChanged);

    // Forward each slice view's seed pick (voxelPicked) up as seedPicked so the
    // main window's pick-mode router handles it.
    connect(axial_, &XQSliceViewWidget::voxelPicked,
            this, &XQMprWidget::seedPicked);
    connect(sagittal_, &XQSliceViewWidget::voxelPicked,
            this, &XQMprWidget::seedPicked);
    connect(coronal_, &XQSliceViewWidget::voxelPicked,
            this, &XQMprWidget::seedPicked);

    // Forward each slice view's out-of-bounds pick so the main window can hint.
    connect(axial_, &XQSliceViewWidget::pickOutOfBounds,
            this, &XQMprWidget::pickOutOfBounds);
    connect(sagittal_, &XQSliceViewWidget::pickOutOfBounds,
            this, &XQMprWidget::pickOutOfBounds);
    connect(coronal_, &XQSliceViewWidget::pickOutOfBounds,
            this, &XQMprWidget::pickOutOfBounds);

    // Forward each slice view's window/level-drag-finished signal.
    connect(axial_, &XQSliceViewWidget::windowLevelChanged,
            this, &XQMprWidget::windowLevelChanged);
    connect(sagittal_, &XQSliceViewWidget::windowLevelChanged,
            this, &XQMprWidget::windowLevelChanged);
    connect(coronal_, &XQSliceViewWidget::windowLevelChanged,
            this, &XQMprWidget::windowLevelChanged);

    retranslateAxisNames();
}

XQMprWidget::~XQMprWidget() = default;

int XQMprWidget::sliceCount(int axis) const
{
    return scene_ != nullptr ? scene_->sliceCount(axis) : 0;
}

int XQMprWidget::currentSlice(int axis) const
{
    if (scene_ == nullptr) {
        return 0;
    }
    const int index = scene_->sliceIndex(axis);
    return index < 0 ? 0 : index;
}

void XQMprWidget::setLayoutMode(LayoutMode mode)
{
    if (layoutMode_ == mode) {
        return;
    }
    layoutMode_ = mode;

    const bool quad = (mode == LayoutMode::Quad);
    // Single: show only Axial; hide the other three so the grid collapses their
    // rows/columns and lets Axial fill the widget.
    sagittalFrame_->setVisible(quad);
    coronalFrame_->setVisible(quad);
    volumeFrame_->setVisible(quad);

    grid_->removeWidget(axialFrame_);
    if (quad) {
        grid_->addWidget(axialFrame_, 0, 0);
    } else {
        grid_->addWidget(axialFrame_, 0, 0, 2, 2);
    }
}

XQMprWidget::LayoutMode XQMprWidget::layoutMode() const
{
    return layoutMode_;
}

void XQMprWidget::renderAll()
{
    if (axial_ != nullptr) {
        axial_->refreshInfoOverlay();
        axial_->renderNow();
    }
    if (sagittal_ != nullptr) {
        sagittal_->refreshInfoOverlay();
        sagittal_->renderNow();
    }
    if (coronal_ != nullptr) {
        coronal_->refreshInfoOverlay();
        coronal_->renderNow();
    }
    if (volume_ != nullptr) {
        volume_->renderNow();
    }
}

void XQMprWidget::setCrosshairVisible(bool visible)
{
    if (scene_ != nullptr) {
        scene_->setCrosshairVisible(visible);
    }
    renderAll();
}

void XQMprWidget::setSeedPickingEnabled(bool enabled)
{
    if (axial_ != nullptr) {
        axial_->setSeedPickingEnabled(enabled);
    }
    if (sagittal_ != nullptr) {
        sagittal_->setSeedPickingEnabled(enabled);
    }
    if (coronal_ != nullptr) {
        coronal_->setSeedPickingEnabled(enabled);
    }
}

void XQMprWidget::clearSeedMarker()
{
    if (axial_ != nullptr) {
        axial_->clearSeedMarker();
    }
    if (sagittal_ != nullptr) {
        sagittal_->clearSeedMarker();
    }
    if (coronal_ != nullptr) {
        coronal_->clearSeedMarker();
    }
}

void XQMprWidget::setModeHint(const QString& text)
{
    if (axial_ != nullptr) {
        axial_->setModeHint(text);
    }
    if (sagittal_ != nullptr) {
        sagittal_->setModeHint(text);
    }
    if (coronal_ != nullptr) {
        coronal_->setModeHint(text);
    }
}

void XQMprWidget::retranslateAxisNames()
{
    // The overlay names mirror the medical axis terms the Image Navigator uses.
    // "3D" is an abbreviation, left untranslated.
    if (axialName_ != nullptr) {
        axialName_->setText(tr("Axial"));
        axialName_->adjustSize();
    }
    if (sagittalName_ != nullptr) {
        sagittalName_->setText(tr("Sagittal"));
        sagittalName_->adjustSize();
    }
    if (coronalName_ != nullptr) {
        coronalName_->setText(tr("Coronal"));
        coronalName_->adjustSize();
    }
}

bool XQMprWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize) {
        if (watched == axialFrame_) {
            positionAxisName(axialFrame_, axialName_);
        } else if (watched == sagittalFrame_) {
            positionAxisName(sagittalFrame_, sagittalName_);
        } else if (watched == coronalFrame_) {
            positionAxisName(coronalFrame_, coronalName_);
        } else if (watched == volumeFrame_) {
            positionAxisName(volumeFrame_, volumeName_);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void XQMprWidget::changeEvent(QEvent* event)
{
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateAxisNames();
    }
    QWidget::changeEvent(event);
}

} // namespace xq
