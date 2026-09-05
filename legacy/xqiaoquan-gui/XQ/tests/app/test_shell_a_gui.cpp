#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "app/XQAppStartup.h"
#include "app/XQMainWindow.h"
#include "app/XQTaskRunner.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQPathPayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQSceneCommands.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/VesselProfileController.h"
#include "ui/panels/XQStageWidgets.h"
#include "visualization/XQVolumeViewWidget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QLabel>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QTreeView>
#include <QVariant>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef XQ_SHELL_A_DICOM_ROOT
#error "XQ_SHELL_A_DICOM_ROOT must be defined"
#endif

namespace {

namespace fs = std::filesystem;

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            return fail(#condition, __LINE__);                              \
        }                                                                    \
    } while (false)

bool waitForTask(xq::XQMainWindow* window, int timeoutMs = 15000)
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

const xq::XQDataNode* firstNodeOfDomain(const xq::XQProject& project,
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

std::size_t domainCount(const xq::XQProject& project, xq::XQDomainType domain)
{
    std::size_t count = 0;
    project.scene().visit_nodes([&count, domain](const xq::XQDataNode& node) {
        if (node.domainType() == domain) {
            ++count;
        }
    });
    return count;
}

bool buildPathPoints(const xq::XQImageVolume& image,
                     std::vector<xq::PathControlPoint>* points)
{
    if (points == nullptr || !image.hasGeometry()) {
        return false;
    }
    const xq::ImageGeometry& geometry = image.geometry();
    int axis = 0;
    double extent = -1.0;
    for (int candidate = 0; candidate < 3; ++candidate) {
        const double candidateExtent =
            static_cast<double>(geometry.dimensions[candidate] - 1)
            * std::abs(geometry.spacing[candidate]);
        if (candidateExtent > extent) {
            extent = candidateExtent;
            axis = candidate;
        }
    }
    if (!(std::abs(geometry.spacing[axis]) > 0.0)) {
        return false;
    }
    double startVoxel[3] = {
        (geometry.dimensions[0] - 1) * 0.5,
        (geometry.dimensions[1] - 1) * 0.5,
        (geometry.dimensions[2] - 1) * 0.5,
    };
    startVoxel[axis] = 0.0;
    double endVoxel[3] = {
        startVoxel[0], startVoxel[1], startVoxel[2]};
    endVoxel[axis] = 60.0 / std::abs(geometry.spacing[axis]);
    double startWorld[3] = {};
    double endWorld[3] = {};
    if (image.voxelToWorld(startVoxel, startWorld)
            != xq::XQImageVolume::TransformStatus::Ok
        || image.voxelToWorld(endVoxel, endWorld)
            != xq::XQImageVolume::TransformStatus::Ok) {
        return false;
    }
    points->clear();
    points->push_back({{startWorld[0], startWorld[1], startWorld[2]}});
    points->push_back({{endWorld[0], endWorld[1], endWorld[2]}});
    return std::abs(xq::distance(
        points->front().position, points->back().position) - 60.0) < 1.0e-8;
}

xq::XQContour contourAt(const xq::XQPath& path,
                        xq::ContourId id,
                        double arcLength,
                        double halfWidth)
{
    xq::PathFrame frame = {};
    path.frameAtArcLength(arcLength, &frame);
    xq::XQContour contour;
    contour.contourId = id;
    contour.pathArcLength = arcLength;
    contour.frame.origin = frame.position;
    contour.frame.normal = frame.tangent;
    contour.frame.xAxis = frame.normal;
    contour.frame.yAxis = frame.binormal;
    contour.type = xq::ContourType::SplinePolygon;
    contour.closed = true;
    contour.points = {
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, -halfWidth, -halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, halfWidth, -halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, halfWidth, halfWidth),
        xq::XQContourGroup::unprojectFromFrame(
            contour.frame, -halfWidth, halfWidth),
    };
    return contour;
}

QModelIndex findIndexByText(QAbstractItemModel* model,
                            const QModelIndex& parent,
                            const QString& text)
{
    if (model == nullptr) {
        return QModelIndex();
    }
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if (model->data(index, Qt::DisplayRole).toString() == text) {
            return index;
        }
        const QModelIndex nested = findIndexByText(model, index, text);
        if (nested.isValid()) {
            return nested;
        }
    }
    return QModelIndex();
}

struct TempProject {
    fs::path path;
    fs::path assets;

    TempProject()
    {
        path = fs::temp_directory_path() / "xq-shell-a-gui.xqproj";
        assets = path.parent_path() / "xq-shell-a-gui.assets";
        std::error_code error;
        fs::remove(path, error);
        fs::remove_all(assets, error);
    }

    ~TempProject()
    {
        std::error_code error;
        fs::remove(path, error);
        fs::remove_all(assets, error);
    }
};

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();
    QApplication app(argc, argv);

    const fs::path dicomDirectory =
        fs::path(XQ_SHELL_A_DICOM_ROOT) / "regular-oblique";
    xq::GdcmItkDicomSeriesReader discoveryReader;
    const xq::DicomSeriesDiscoveryResult discovery =
        discoveryReader.discover(dicomDirectory.string());
    CHECK(discovery.ok());
    CHECK(discovery.series.size() == 1);

    xq::XQAppStartupState state;
    CHECK(xq::initializeAppStartup({}, &state) == xq::XQAppStartupStatus::Ok);
    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    xq::attachMainWindowWorkflow(&window, &state);
    CHECK(window.vesselProfileController() != nullptr);
    CHECK(window.pathModuleController() != nullptr);
    QAction* openDicom = window.findChild<QAction*>(
        QStringLiteral("xqOpenDicomAction"));
    CHECK(openDicom != nullptr && openDicom->isEnabled());

    CHECK(window.loadDicomSeriesFromDirectory(
        QString::fromStdString(dicomDirectory.string()),
        QString::fromStdString(
            discovery.series.front().identity.seriesInstanceUid)));
    CHECK(window.taskRunner().busy());
    CHECK(waitForTask(&window));

    const xq::XQDataNode* imageNode =
        firstNodeOfDomain(state.project, xq::XQDomainType::Image);
    CHECK(imageNode != nullptr);
    CHECK(imageNode->hasAssetId());
    CHECK(imageNode->scaleSlot() == xq::ScaleSlot::Organ);
    const auto imagePayload = std::dynamic_pointer_cast<
        xq::XQImageVolumePayload>(imageNode->payload());
    CHECK(imagePayload != nullptr);
    CHECK(imagePayload->volume().hasDicomIdentity());
    QSlider* axial = window.findChild<QSlider*>(
        QStringLiteral("xqNavAxialSlider"));
    CHECK(axial != nullptr && axial->isEnabled());

    std::vector<xq::PathControlPoint> pathPoints;
    CHECK(buildPathPoints(imagePayload->volume(), &pathPoints));
    const xq::NodeId pathId(imageNode->id().value() + 1);
    xq::PathController::AddPathIntent pathIntent;
    pathIntent.newPathId = pathId;
    pathIntent.name = "GUI Shell A path";
    pathIntent.sourceImageNode = imageNode->id();
    pathIntent.controlPoints = pathPoints;
    pathIntent.spacing = 1.0;
    CHECK(window.pathController()->addPath(pathIntent)
          == xq::PathController::Status::Ok);
    const xq::XQDataNode* pathNode = state.project.scene().find(pathId);
    CHECK(pathNode != nullptr);
    CHECK(pathNode->scaleSlot() == xq::ScaleSlot::Organ);
    const auto pathPayload = std::dynamic_pointer_cast<xq::XQPathPayload>(
        pathNode->payload());
    CHECK(pathPayload != nullptr);

    const xq::NodeId contourId(pathId.value() + 1);
    xq::XQContourGroup group;
    group.setId(contourId);
    group.setSourcePathNode(pathId);
    group.addContour(contourAt(
        pathPayload->path(), xq::ContourId(65001), 5.0, 5.0));
    group.addContour(contourAt(
        pathPayload->path(), xq::ContourId(65002), 30.0, 6.0));
    group.addContour(contourAt(
        pathPayload->path(), xq::ContourId(65003), 55.0, 7.0));
    xq::XQDataNode contourNode(
        contourId, xq::XQDomainType::ContourGroup,
        "GUI Shell A contours",
        std::make_shared<xq::XQContourGroupPayload>(std::move(group)));
    contourNode.setScaleSlot(xq::ScaleSlot::Organ);
    CHECK(state.commandStack.push(
        std::make_unique<xq::AddNodeWithSourceRelationCommand>(
            &state.project.scene(), std::move(contourNode), pathId,
            "Add GUI Shell A contours")));

    // Re-attach refreshes the scene tree and the six stage pages against the
    // newly committed Path/Contour inputs; the decoded DICOM residency remains
    // owned by the window.
    window.attachWorkflow(&state.project, &state.commandStack);
    QComboBox* profilePath = window.findChild<QComboBox*>(
        QStringLiteral("xqPathInputPathCombo"));
    QComboBox* profileContour = window.findChild<QComboBox*>(
        QStringLiteral("xqPathInputContourCombo"));
    QPushButton* preparePathInput = window.findChild<QPushButton*>(
        QStringLiteral("xqPathInputBuild"));
    CHECK(profilePath != nullptr && profileContour != nullptr
          && preparePathInput != nullptr && preparePathInput->isEnabled());
    static_cast<xq::NodeComboBox*>(profilePath)->repopulate();
    static_cast<xq::NodeComboBox*>(profileContour)->repopulate();
    const int pathIndex = profilePath->findData(
        QVariant::fromValue<qulonglong>(pathId.value()));
    const int contourIndex = profileContour->findData(
        QVariant::fromValue<qulonglong>(contourId.value()));
    CHECK(pathIndex >= 0 && contourIndex >= 0);
    profilePath->setCurrentIndex(pathIndex);
    profileContour->setCurrentIndex(contourIndex);
    preparePathInput->click();
    CHECK(window.taskRunner().busy());
    CHECK(waitForTask(&window));
    CHECK(domainCount(state.project, xq::XQDomainType::VesselProfile) == 1);

    const xq::XQDataNode* profileNode =
        firstNodeOfDomain(state.project, xq::XQDomainType::VesselProfile);
    CHECK(profileNode != nullptr);
    xq::NodeId profileId = profileNode->id();
    CHECK(profileNode->scaleSlot() == xq::ScaleSlot::Organ);
    const auto vesselProfile = std::dynamic_pointer_cast<
        xq::XQVesselProfilePayload>(profileNode->payload());
    CHECK(vesselProfile != nullptr);
    CHECK(xq::VesselProfileValidator::validate(vesselProfile->profile()).ok());
    CHECK(vesselProfile->profile().frameOfReferenceId
          == imagePayload->volume().dicomIdentity().frameOfReferenceUid);
    CHECK(vesselProfile->profile().samples.size() == 3);

    QWidget* modulesPage = window.findChild<QWidget*>(
        QStringLiteral("xqStagePage_Modules"));
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
    QLabel* pathGeometry = window.findChild<QLabel*>(
        QStringLiteral("xqPathModuleGeometry"));
    QPlainTextEdit* pathDump = window.findChild<QPlainTextEdit*>(
        QStringLiteral("xqPathContractDump"));
    QPushButton* copyPathDump = window.findChild<QPushButton*>(
        QStringLiteral("xqPathDumpCopy"));
    CHECK(modulesPage != nullptr && moduleSource != nullptr
          && moduleCombo != nullptr && runModule != nullptr
          && moduleStatus != nullptr && pathSource != nullptr
          && pathGeometry != nullptr && pathDump != nullptr
          && copyPathDump != nullptr && !copyPathDump->isEnabled());
    CHECK(window.findChild<QWidget*>(QStringLiteral("xqStagePage_Flow")) == nullptr);
    CHECK(window.findChild<QPushButton*>(QStringLiteral("xqFlowSmokeRun")) == nullptr);
    CHECK(window.findChild<QComboBox*>(QStringLiteral("xqFlowSourceCombo")) == nullptr);
    CHECK(window.findChild<QLabel*>(QStringLiteral("xqFlowSmokeProtocol")) == nullptr);

    static_cast<xq::NodeComboBox*>(moduleSource)->repopulate();
    const int profileIndex = moduleSource->findData(
        QVariant::fromValue<qulonglong>(profileId.value()));
    const int validateIndex = moduleCombo->findData(
        QStringLiteral("path-validate"));
    CHECK(profileIndex >= 0 && validateIndex >= 0);
    moduleSource->setCurrentIndex(profileIndex);
    moduleCombo->setCurrentIndex(validateIndex);
    CHECK(runModule->isEnabled());
    runModule->click();
    CHECK(moduleStatus->text().contains(QStringLiteral("Path module completed")));
    CHECK(pathSource->text() == QStringLiteral("Semi-automatic"));
    CHECK(pathGeometry->text().contains(QStringLiteral("stations")));
    CHECK(pathDump->toPlainText().contains(QStringLiteral("vessel_path_v1")));
    CHECK(pathDump->toPlainText().contains(QStringLiteral("source=semi_automatic")));
    CHECK(copyPathDump->isEnabled());
    copyPathDump->click();
    CHECK(QApplication::clipboard()->text() == pathDump->toPlainText());

    const int noopIndex = moduleCombo->findData(QStringLiteral("noop"));
    CHECK(noopIndex >= 0);
    moduleCombo->setCurrentIndex(noopIndex);
    CHECK(pathDump->toPlainText().isEmpty());
    CHECK(!copyPathDump->isEnabled());
    CHECK(moduleStatus->text() == QStringLiteral("Idle."));
    moduleCombo->setCurrentIndex(validateIndex);
    runModule->click();
    CHECK(pathDump->toPlainText().contains(QStringLiteral("vessel_path_v1")));

    preparePathInput->click();
    CHECK(window.taskRunner().busy());
    CHECK(waitForTask(&window));
    CHECK(domainCount(state.project, xq::XQDomainType::VesselProfile) == 2);
    const qulonglong preparedProfileValue =
        moduleSource->currentData().value<qulonglong>();
    CHECK(preparedProfileValue != profileId.value());
    profileId = xq::NodeId(preparedProfileValue);
    CHECK(pathDump->toPlainText().isEmpty());
    CHECK(!copyPathDump->isEnabled());
    CHECK(runModule->isEnabled());
    runModule->click();
    CHECK(pathDump->toPlainText().contains(QStringLiteral("vessel_path_v1")));
    CHECK(domainCount(state.project, xq::XQDomainType::SimulationCase) == 0);
    CHECK(domainCount(state.project, xq::XQDomainType::FlowResult) == 0);

    TempProject files;
    CHECK(window.saveWorkspaceFile(
        QString::fromStdString(files.path.string())));
    CHECK(window.openWorkspaceFromPath(
        QString::fromStdString(files.path.string())));

    QTreeView* tree = window.findChild<QTreeView*>(
        QStringLiteral("xqSceneTreeView"));
    CHECK(tree != nullptr && tree->model() != nullptr
          && tree->selectionModel() != nullptr);
    const QModelIndex reopenedImageIndex = findIndexByText(
        tree->model(), QModelIndex(),
        QString::fromStdString(imageNode->display_name()));
    CHECK(reopenedImageIndex.isValid());
    tree->selectionModel()->setCurrentIndex(
        reopenedImageIndex,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    CHECK(window.taskRunner().busy());
    CHECK(waitForTask(&window));
    CHECK(axial->isEnabled());

    const QModelIndex reopenedProfileIndex = findIndexByText(
        tree->model(), QModelIndex(), QStringLiteral("Path input"));
    CHECK(reopenedProfileIndex.isValid());
    CHECK(domainCount(state.project, xq::XQDomainType::SimulationCase) == 0);
    CHECK(domainCount(state.project, xq::XQDomainType::FlowResult) == 0);

    QComboBox* reopenedModuleSource = window.findChild<QComboBox*>(
        QStringLiteral("xqPathModuleSourceCombo"));
    QComboBox* reopenedModuleCombo = window.findChild<QComboBox*>(
        QStringLiteral("xqPathModuleCombo"));
    QPushButton* reopenedRunModule = window.findChild<QPushButton*>(
        QStringLiteral("xqPathModuleRun"));
    QLabel* reopenedModuleStatus = window.findChild<QLabel*>(
        QStringLiteral("xqStageStatus_Modules"));
    QPlainTextEdit* reopenedDump = window.findChild<QPlainTextEdit*>(
        QStringLiteral("xqPathContractDump"));
    CHECK(reopenedModuleSource != nullptr && reopenedModuleCombo != nullptr
          && reopenedRunModule != nullptr && reopenedModuleStatus != nullptr
          && reopenedDump != nullptr);
    static_cast<xq::NodeComboBox*>(reopenedModuleSource)->repopulate();
    const int reopenedProfileComboIndex = reopenedModuleSource->findData(
        QVariant::fromValue<qulonglong>(profileId.value()));
    const int reopenedValidateIndex = reopenedModuleCombo->findData(
        QStringLiteral("path-validate"));
    CHECK(reopenedProfileComboIndex >= 0 && reopenedValidateIndex >= 0);
    reopenedModuleSource->setCurrentIndex(reopenedProfileComboIndex);
    reopenedModuleCombo->setCurrentIndex(reopenedValidateIndex);
    CHECK(reopenedRunModule->isEnabled());
    reopenedRunModule->click();
    if (!reopenedModuleStatus->text().contains(
            QStringLiteral("Path module completed"))) {
        std::fprintf(stderr, "Reopened module status: %s\n",
                     reopenedModuleStatus->text().toUtf8().constData());
        return fail("reopened Path module completes", __LINE__);
    }
    CHECK(reopenedDump->toPlainText().contains(QStringLiteral("vessel_path_v1")));
    CHECK(window.saveWorkspaceFile(
        QString::fromStdString(files.path.string())));

    std::printf(
        "OK: GUI DICOM selection/import, Path input preparation, Path module, "
        "save/reopen, and lazy voxel display\n");
    return 0;
}
