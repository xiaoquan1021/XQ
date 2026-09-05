#include "app/XQMainWindow.h"
#include "app/XQTaskRunner.h"
#include "core/XQDataNode.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"
#include "ui/panels/XQStageWidgets.h"
#include "visualization/XQVolumeViewWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVariant>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                       \
    do {                                        \
        if (!(expression)) {                    \
            return fail(#expression, __LINE__); \
        }                                       \
    } while (false)

bool waitForTask(xq::XQMainWindow* window, int timeoutMs = 30000)
{
    if (window == nullptr) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (window->taskRunner().busy() && timer.elapsed() < timeoutMs) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    return !window->taskRunner().busy();
}

xq::ImageGeometry geometry()
{
    xq::ImageGeometry value{};
    value.dimensions[0] = 25;
    value.dimensions[1] = 21;
    value.dimensions[2] = 23;
    value.spacing[0] = 0.7;
    value.spacing[1] = 1.1;
    value.spacing[2] = 1.6;
    value.origin[0] = 12.5;
    value.origin[1] = -8.25;
    value.origin[2] = 3.75;
    const double angle = 0.31;
    value.direction[0][0] = std::cos(angle);
    value.direction[0][1] = -std::sin(angle);
    value.direction[0][2] = 0.0;
    value.direction[1][0] = std::sin(angle);
    value.direction[1][1] = std::cos(angle);
    value.direction[1][2] = 0.0;
    value.direction[2][0] = 0.0;
    value.direction[2][1] = 0.0;
    value.direction[2][2] = 1.0;
    value.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return value;
}

std::size_t flat(int x, int y, int z, const int dimensions[3])
{
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(dimensions[0])
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(dimensions[1])
                   * static_cast<std::size_t>(z));
}

xq::XQSegmentationMask tubeMask()
{
    const xq::ImageGeometry valueGeometry = geometry();
    xq::XQSegmentationMask mask(valueGeometry.dimensions);
    mask.setGeometry(valueGeometry);
    mask.setSourceImageNode(xq::NodeId(100));
    const double centerX =
        0.5 * static_cast<double>(valueGeometry.dimensions[0] - 1);
    const double centerY =
        0.5 * static_cast<double>(valueGeometry.dimensions[1] - 1);
    for (int z = 3; z + 3 < valueGeometry.dimensions[2]; ++z) {
        for (int y = 0; y < valueGeometry.dimensions[1]; ++y) {
            for (int x = 0; x < valueGeometry.dimensions[0]; ++x) {
                const double dx =
                    (static_cast<double>(x) - centerX)
                    * valueGeometry.spacing[0];
                const double dy =
                    (static_cast<double>(y) - centerY)
                    * valueGeometry.spacing[1];
                if (dx * dx + dy * dy <= 3.2 * 3.2) {
                    mask.setLabelAt(
                        flat(x, y, z, valueGeometry.dimensions), 1);
                }
            }
        }
    }
    return mask;
}

const xq::XQDataNode* firstNodeOfDomain(
    const xq::XQProject& project,
    xq::XQDomainType domain)
{
    const xq::XQDataNode* found = nullptr;
    project.scene().visit_nodes([&found, domain](const xq::XQDataNode& node) {
        if (found == nullptr && node.domainType() == domain) {
            found = &node;
        }
    });
    return found;
}

std::size_t relationCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();
    QApplication app(argc, argv);

    xq::XQProject project;
    xq::XQCommandStack stack;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);

    xq::XQImageVolume image;
    image.setGeometry(geometry());
    image.setScalarType(xq::ScalarType::Int16);
    image.setComponentCount(1);
    xq::DicomSeriesIdentity dicom;
    dicom.studyInstanceUid = "1.2.840.centerline-b.gui.study";
    dicom.seriesInstanceUid = "1.2.840.centerline-b.gui.series";
    dicom.frameOfReferenceUid = "1.2.840.centerline-b.gui.frame";
    image.setDicomIdentity(dicom);
    xq::XQDataNode imageNode(
        xq::NodeId(100),
        xq::XQDomainType::Image,
        "Centerline B source image",
        std::make_shared<xq::XQImageVolumePayload>(image));
    imageNode.setScaleSlot(xq::ScaleSlot::Organ);
    CHECK(project.scene().insert(std::move(imageNode))
          == xq::XQScene::InsertResult::Inserted);

    xq::XQDataNode maskNode(
        xq::NodeId(101),
        xq::XQDomainType::SegmentationMask,
        "Centerline B tube mask",
        std::make_shared<xq::XQSegmentationMaskPayload>(tubeMask()));
    CHECK(project.scene().insert(std::move(maskNode))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().link_derived(xq::NodeId(100), xq::NodeId(101))
          == xq::XQScene::RelationResult::Linked);

    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    window.attachWorkflow(&project, &stack);
    CHECK(window.centerlineBController() != nullptr);

    QComboBox* maskCombo = window.findChild<QComboBox*>(
        QStringLiteral("xqCenterlineBMaskCombo"));
    QPushButton* build = window.findChild<QPushButton*>(
        QStringLiteral("xqCenterlineBBuild"));
    QComboBox* moduleSource = window.findChild<QComboBox*>(
        QStringLiteral("xqPathModuleSourceCombo"));
    QComboBox* moduleCombo = window.findChild<QComboBox*>(
        QStringLiteral("xqPathModuleCombo"));
    QPushButton* runModule = window.findChild<QPushButton*>(
        QStringLiteral("xqPathModuleRun"));
    QLabel* moduleStatus = window.findChild<QLabel*>(
        QStringLiteral("xqStageStatus_Modules"));
    QLabel* pathSource = window.findChild<QLabel*>(
        QStringLiteral("xqPathModuleSource"));
    QPlainTextEdit* pathDump = window.findChild<QPlainTextEdit*>(
        QStringLiteral("xqPathContractDump"));
    CHECK(maskCombo != nullptr && build != nullptr
          && moduleSource != nullptr && moduleCombo != nullptr
          && runModule != nullptr && moduleStatus != nullptr
          && pathSource != nullptr && pathDump != nullptr);
    static_cast<xq::NodeComboBox*>(maskCombo)->repopulate();
    CHECK(maskCombo->count() == 1);
    CHECK(maskCombo->currentData().value<qulonglong>() == 101);
    CHECK(build->isEnabled());

    build->click();
    CHECK(window.taskRunner().busy());
    CHECK(waitForTask(&window));
    CHECK(moduleStatus->text().contains(QStringLiteral("Centerline B")));

    const xq::XQDataNode* pathNode =
        firstNodeOfDomain(project, xq::XQDomainType::Path);
    const xq::XQDataNode* profileNode =
        firstNodeOfDomain(project, xq::XQDomainType::VesselProfile);
    CHECK(pathNode != nullptr && profileNode != nullptr);
    CHECK(pathNode->id() == xq::NodeId(2000));
    CHECK(profileNode->id() == xq::NodeId(2001));
    CHECK(pathNode->scaleSlot() == xq::ScaleSlot::Organ);
    CHECK(profileNode->scaleSlot() == xq::ScaleSlot::Organ);
    CHECK(pathNode->hasAssetId() && profileNode->hasAssetId());
    CHECK(project.assetRegistry().hasRelation(
        pathNode->assetId(), profileNode->assetId()));
    CHECK(relationCount(project.scene()) == 4);
    const std::shared_ptr<xq::XQPathPayload> pathPayload =
        std::dynamic_pointer_cast<xq::XQPathPayload>(pathNode->payload());
    const std::shared_ptr<xq::XQVesselProfilePayload> profilePayload =
        std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(
            profileNode->payload());
    CHECK(pathPayload != nullptr && profilePayload != nullptr);
    CHECK(pathPayload->path().sourceImageNode() == xq::NodeId(100));
    CHECK(xq::VesselProfileValidator::validate(
        profilePayload->profile()).ok());

    const qulonglong selectedProfile =
        moduleSource->currentData().value<qulonglong>();
    CHECK(selectedProfile == profileNode->id().value());
    const int validateIndex = moduleCombo->findData(
        QStringLiteral("path-validate"));
    CHECK(validateIndex >= 0);
    moduleCombo->setCurrentIndex(validateIndex);
    CHECK(runModule->isEnabled());
    runModule->click();
    CHECK(moduleStatus->text().contains(
        QStringLiteral("Path module completed")));
    CHECK(pathSource->text() == QStringLiteral("Automatic centerline B"));
    CHECK(pathDump->toPlainText().contains(
        QStringLiteral("source=automatic_centerline_b")));

    window.undo();
    CHECK(project.scene().find(xq::NodeId(2000)) == nullptr);
    CHECK(project.scene().find(xq::NodeId(2001)) == nullptr);
    CHECK(project.assetRegistry().assetCount() == 0);
    window.redo();
    CHECK(project.scene().find(xq::NodeId(2000)) != nullptr);
    CHECK(project.scene().find(xq::NodeId(2001)) != nullptr);
    CHECK(project.assetRegistry().assetCount() == 2);

    CHECK(project.scene().mark_source_changed(xq::NodeId(100)) == 3);
    CHECK(project.scene().is_stale(xq::NodeId(101)));
    CHECK(project.scene().is_stale(xq::NodeId(2000)));
    CHECK(project.scene().is_stale(xq::NodeId(2001)));

    std::printf(
        "Centerline B GUI real-ITK build, selection, PathValidate, and undo/redo passed\n");
    return 0;
}
