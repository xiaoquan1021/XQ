#include <app/XQMainWindow.h>
#include <app/XQTaskRunner.h>

#include <core/NodeId.h>
#include <core/XQContourGroupPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQPayload.h>
#include <core/XQPathPayload.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQVesselProfilePayload.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>
#include <io/project/CTGRContourReader.h>
#include <ui/XQSceneModel.h>
#include <ui/controllers/PathController.h>
#include <ui/controllers/VesselProfileController.h>
#include <ui/panels/XQStageWidgets.h>
#include <visualization/XQMprWidget.h>
#include <visualization/XQVolumeViewWidget.h>

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFrame>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPoint>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QRegularExpression>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QTabWidget>
#include <QTreeView>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <memory>

#ifndef XQ_TEST_VTI_PATH
#error "XQ_TEST_VTI_PATH must be defined"
#endif

#ifndef XQ_SVPROJECT_DIR
#error "XQ_SVPROJECT_DIR must be defined"
#endif

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

std::size_t count_nodes(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) {
        ++count;
    });
    return count;
}

std::map<xq::XQDomainType, std::size_t> count_domains(const xq::XQScene& scene)
{
    std::map<xq::XQDomainType, std::size_t> counts;
    scene.visit_nodes([&counts](const xq::XQDataNode& node) {
        ++counts[node.domainType()];
    });
    return counts;
}

bool close_enough(int a, int b, int tolerance = 4)
{
    return std::abs(a - b) <= tolerance;
}

xq::XQContour make_profile_contour(
    const xq::XQPath& path,
    double arcLength,
    double halfWidth)
{
    xq::PathFrame pathFrame = {};
    path.frameAtArcLength(arcLength, &pathFrame);
    xq::XQContour contour;
    contour.contourId = xq::ContourId::invalid();
    contour.pathArcLength = arcLength;
    contour.frame.origin = pathFrame.position;
    contour.frame.normal = pathFrame.tangent;
    contour.frame.xAxis = pathFrame.normal;
    contour.frame.yAxis = pathFrame.binormal;
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

xq::VesselProfileV1 make_smoke_profile(const xq::NodeId& pathId)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "1.2.840.main-window.flow-smoke";
    profile.sourcePathNode = pathId;
    profile.externalEvidenceId = "main-window-flow-smoke-profile";
    profile.externalEvidenceFingerprint = "sha256:main-window-flow-smoke-profile";
    profile.derivationStamp.algorithmId = "xq.main-window.flow-smoke-test";
    profile.derivationStamp.algorithmVersion = "1";
    profile.derivationStamp.parameterSummary = "fixed-valid-profile";
    xq::DerivationInputStamp pathStamp;
    pathStamp.nodeId = pathId;
    profile.derivationStamp.inputs.push_back(pathStamp);

    const double arcs[] = {0.0, 9.0, 27.5, 50.0};
    for (std::size_t i = 0; i < 4; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(5100 + i);
        sample.arcLengthMm = arcs[i];
        sample.positionMm = {arcs[i], 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 100.0 + 10.0 * i;
        sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
        sample.quality = xq::VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = xq::NodeId::invalid();
        profile.samples.push_back(sample);
    }
    return profile;
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();

    QApplication app(argc, argv);

    // Contour workbench edits are semantic copy-on-write commands. Drive the
    // exact shared append tail with invalid contour ids (the real GUI path), so
    // the test also proves automatic ids stay unique and stable through undo.
    {
        const xq::NodeId pathId(9101);
        const xq::NodeId contourGroupId(9102);
        const xq::NodeId existingProfileId(9103);
        const xq::NodeId preparedProfileId(9104);
        const xq::NodeId otherContourGroupId(9105);
        const xq::ContourId preexistingNestedContourId(9900);

        xq::XQProject project;
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open contour semantic-edit project");
        }
        xq::XQPath path;
        path.setId(pathId);
        path.setControlPoints({{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 30.0}}});
        path.resample(1.0);

        xq::XQDataNode pathNode(
            pathId,
            xq::XQDomainType::Path,
            "profile path",
            std::make_shared<xq::XQPathPayload>(path));
        pathNode.setContentRevision(7);
        xq::XQContourGroup group;
        group.setId(contourGroupId);
        group.setSourcePathNode(pathId);
        xq::XQDataNode contourNode(
            contourGroupId,
            xq::XQDomainType::ContourGroup,
            "profile contours",
            std::make_shared<xq::XQContourGroupPayload>(group));
        contourNode.setContentRevision(11);
        xq::XQDataNode existingProfile(
            existingProfileId,
            xq::XQDomainType::VesselProfile,
            "existing profile",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::VesselProfile, "existing-profile"));
        xq::XQContourGroup otherGroup;
        otherGroup.setId(otherContourGroupId);
        otherGroup.setSourcePathNode(pathId);
        xq::XQContour otherContour = make_profile_contour(path, 10.0, 0.5);
        otherContour.contourId = preexistingNestedContourId;
        otherGroup.addContour(otherContour);
        xq::XQDataNode otherContourNode(
            otherContourGroupId,
            xq::XQDomainType::ContourGroup,
            "other contours",
            std::make_shared<xq::XQContourGroupPayload>(otherGroup));
        if (project.scene().insert(pathNode) != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(contourNode)
                != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(existingProfile)
                != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(otherContourNode)
                != xq::XQScene::InsertResult::Inserted
            || project.scene().link_derived(contourGroupId, existingProfileId)
                != xq::XQScene::RelationResult::Linked) {
            return fail("seed contour semantic-edit project");
        }

        xq::XQCommandStack contourStack;
        xq::XQMainWindow contourWindow;
        contourWindow.attachWorkflow(&project.scene(), &contourStack);

        xq::ContourId appendedIds[3];
        const double arcs[3] = {5.0, 15.0, 25.0};
        for (int i = 0; i < 3; ++i) {
            appendedIds[i] = contourWindow.appendContourToGroupForTest(
                contourGroupId,
                make_profile_contour(path, arcs[i], 1.0 + i));
            const xq::XQDataNode* live = project.scene().find(contourGroupId);
            const std::shared_ptr<xq::XQContourGroupPayload> livePayload =
                live == nullptr
                ? std::shared_ptr<xq::XQContourGroupPayload>()
                : std::dynamic_pointer_cast<xq::XQContourGroupPayload>(
                    live->payload());
            if (!appendedIds[i].is_valid()
                || appendedIds[i].value() <= preexistingNestedContourId.value()
                || project.scene().find(appendedIds[i]) != nullptr
                || live == nullptr || livePayload == nullptr
                || live->contentRevision() != static_cast<unsigned long long>(12 + i)
                || livePayload->group().contours().size()
                    != static_cast<std::size_t>(i + 1)
                || livePayload->group().contours().back().contourId
                    != appendedIds[i]) {
                return fail("automatic contour append assigns one distinct semantic id");
            }
        }
        if (appendedIds[0] == appendedIds[1]
            || appendedIds[0] == appendedIds[2]
            || appendedIds[1] == appendedIds[2]) {
            return fail("automatic contour ids are distinct");
        }

        contourWindow.undo();
        const auto payloadAfterUndo = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (payloadAfterUndo == nullptr
            || payloadAfterUndo->group().contours().size() != 2
            || project.scene().find(contourGroupId)->contentRevision() != 13) {
            return fail("contour append undo restores count and revision");
        }
        contourWindow.redo();
        const auto payloadAfterRedo = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (payloadAfterRedo == nullptr
            || payloadAfterRedo->group().contours().size() != 3
            || payloadAfterRedo->group().contours().back().contourId
                != appendedIds[2]
            || project.scene().find(contourGroupId)->contentRevision() != 14) {
            return fail("contour append redo preserves allocated id and revision");
        }
        if (!project.scene().restore_stale_snapshot({})) {
            return fail("clear contour-edit stale snapshot before race probe");
        }

        xq::VesselProfileController controller(&project, &contourStack);
        xq::VesselProfileController::ContourIntent intent;
        intent.output.newProfileId = preparedProfileId;
        intent.output.name = "prepared profile";
        intent.pathNode = pathId;
        intent.contourGroupNode = contourGroupId;
        intent.frameOfReferenceId = "1.2.840.main-window.profile";
        xq::VesselProfileController::PreparedCommand prepared =
            controller.prepareFromContours(intent);
        if (!prepared.ok()) {
            return fail("three GUI contours assemble a valid prepared profile");
        }
        const std::shared_ptr<xq::XQPayload> beforeEditIdentity =
            project.scene().find(contourGroupId)->payload();
        const xq::ContourId fourthId = contourWindow.appendContourToGroupForTest(
            contourGroupId,
            make_profile_contour(path, 28.0, 4.0));
        const auto payloadAfterFourth = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (!fourthId.is_valid() || fourthId == appendedIds[0]
            || fourthId == appendedIds[1] || fourthId == appendedIds[2]
            || payloadAfterFourth == nullptr
            || payloadAfterFourth->group().contours().size() != 4
            || payloadAfterFourth->group().contours().back().contourId != fourthId
            || project.scene().find(contourGroupId)->contentRevision() != 15
            || project.scene().find(contourGroupId)->payload() == beforeEditIdentity
            || !project.scene().is_stale(existingProfileId)) {
            return fail("fourth contour is a copy-on-write semantic edit");
        }
        if (controller.commitPrepared(std::move(prepared))
                != xq::VesselProfileController::Status::SourceChanged
            || project.scene().find(preparedProfileId) != nullptr) {
            return fail("prepared profile rejects a contour edit race");
        }

        contourWindow.undo();
        const auto payloadAfterRaceUndo = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (payloadAfterRaceUndo == nullptr
            || payloadAfterRaceUndo->group().contours().size() != 3
            || project.scene().find(contourGroupId)->contentRevision() != 14
            || project.scene().is_stale(existingProfileId)) {
            return fail("race edit undo restores exact contour and stale state");
        }
        contourWindow.redo();
        const auto payloadAfterRaceRedo = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (payloadAfterRaceRedo == nullptr
            || payloadAfterRaceRedo->group().contours().size() != 4
            || payloadAfterRaceRedo->group().contours().back().contourId != fourthId
            || project.scene().find(contourGroupId)->contentRevision() != 15
            || !project.scene().is_stale(existingProfileId)) {
            return fail("race edit redo restores id, revision, and stale state");
        }
    }

    // A successful workbench mutation must clear redo before an undone contour
    // id can be reused as a Scene node id. Otherwise redoing the old contour
    // would create one id in both the node and nested-contour namespaces.
    {
        const xq::NodeId pathId(9201);
        const xq::NodeId contourGroupId(9202);
        xq::XQProject project;
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open contour redo-reservation project");
        }
        xq::XQPath path;
        path.setId(pathId);
        path.setControlPoints({{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 10.0}}});
        path.resample(1.0);
        xq::XQContourGroup group;
        group.setId(contourGroupId);
        group.setSourcePathNode(pathId);
        xq::XQDataNode pathNode(
            pathId,
            xq::XQDomainType::Path,
            "redo path",
            std::make_shared<xq::XQPathPayload>(path));
        pathNode.setScaleSlot(xq::ScaleSlot::Organ);
        if (project.scene().insert(std::move(pathNode))
                != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(xq::XQDataNode(
                contourGroupId,
                xq::XQDomainType::ContourGroup,
                "redo contours",
                std::make_shared<xq::XQContourGroupPayload>(group)))
                != xq::XQScene::InsertResult::Inserted) {
            return fail("seed contour redo-reservation project");
        }

        xq::XQCommandStack stack;
        xq::XQMainWindow window;
        window.attachWorkflow(&project.scene(), &stack);
        const xq::ContourId undoneContourId = window.appendContourToGroupForTest(
            contourGroupId,
            make_profile_contour(path, 5.0, 1.0));
        window.undo();
        if (!undoneContourId.is_valid() || stack.redo_count() != 1) {
            return fail("prepare undone contour id for workbench reuse");
        }

        QComboBox* pathCombo =
            window.findChild<QComboBox*>(QStringLiteral("xqContourPathCombo"));
        QPushButton* createButton = window.findChild<QPushButton*>(
            QStringLiteral("xqContourGroupCreateBtn"));
        if (pathCombo == nullptr || createButton == nullptr) {
            return fail("find contour workbench creation controls");
        }
        static_cast<xq::NodeComboBox*>(pathCombo)->repopulate();
        const int pathIndex = pathCombo->findData(
            QVariant::fromValue<qulonglong>(pathId.value()));
        if (pathIndex < 0) {
            return fail("select path for redo-reservation contour group");
        }
        pathCombo->setCurrentIndex(pathIndex);
        createButton->click();

        const xq::XQDataNode* reusedNode =
            project.scene().find(undoneContourId);
        bool pathRelation = false;
        project.scene().visit_derived_relations(
            [&pathRelation, pathId, undoneContourId](
                const xq::NodeId& source,
                const xq::NodeId& derived) {
                pathRelation = pathRelation
                    || (source == pathId && derived == undoneContourId);
            });
        const auto originalGroup = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (reusedNode == nullptr
            || reusedNode->domainType() != xq::XQDomainType::ContourGroup
            || reusedNode->scaleSlot() != xq::ScaleSlot::Organ
            || !pathRelation
            || originalGroup == nullptr || !originalGroup->group().contours().empty()
            || stack.redo_count() != 0) {
            return fail(
                "new contour group links its Path and clears conflicting contour redo");
        }
        window.redo();
        const auto groupAfterNoOpRedo = std::dynamic_pointer_cast<
            xq::XQContourGroupPayload>(
                project.scene().find(contourGroupId)->payload());
        if (groupAfterNoOpRedo == nullptr
            || !groupAfterNoOpRedo->group().contours().empty()) {
            return fail("cleared contour redo remains a no-op");
        }
    }

    // Runtime Flow-off keeps the six-stage shell stable, exposes the same
    // Path/Modules shell page as Flow-on, and continues to show historical
    // FlowResult data without restoring a solver entry.
    {
        xq::XQProject project;
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open runtime Flow-off project");
        }

        xq::XQFlowResult historicalFlow;
        historicalFlow.setTimes({0.0, 0.1});
        xq::FlowSegment segment;
        segment.segmentId = 0;
        segment.arcLengthStart = 0.0;
        segment.arcLengthEnd = 1.0;
        historicalFlow.addSegment(segment);
        historicalFlow.setSeries({{1.0, 1.0}}, {{100.0, 100.0}}, {{1.0, 1.0}});
        historicalFlow.setConverged(true);
        historicalFlow.setMaxCfl(0.2);
        if (project.scene().insert(xq::XQDataNode(
                xq::NodeId(90), xq::XQDomainType::FlowResult,
                "Historical Flow Result",
                std::make_shared<xq::XQFlowResultPayload>(std::move(historicalFlow))))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("seed historical FlowResult for runtime Flow-off");
        }
        const xq::NodeId modulePathId(91);
        const xq::NodeId moduleProfileId(92);
        xq::XQDataNode modulePath(
            modulePathId, xq::XQDomainType::Path, "Module path",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, std::string()));
        xq::XQDataNode moduleProfile(
            moduleProfileId, xq::XQDomainType::VesselProfile, "Module profile",
            std::make_shared<xq::XQVesselProfilePayload>(
                make_smoke_profile(modulePathId)));
        if (project.scene().insert(std::move(modulePath))
                != xq::XQScene::InsertResult::Inserted
            || project.scene().insert(std::move(moduleProfile))
                != xq::XQScene::InsertResult::Inserted
            || project.scene().link_derived(modulePathId, moduleProfileId)
                != xq::XQScene::RelationResult::Linked) {
            return fail("seed Path module profile for runtime Flow-off");
        }

        xq::XQCommandStack stack;
        xq::XQMainWindow window(xq::WorkflowCapabilities::withoutFlow());
        window.attachWorkflow(&project, &stack);
        if (window.hasFlowCapability() || window.flowController() != nullptr
            || window.flowSmokeController() != nullptr
            || window.vesselProfileController() == nullptr
            || window.pathModuleController() == nullptr
            || window.pathController() == nullptr || window.aiController() == nullptr) {
            return fail("runtime Flow-off removes only Flow execution controllers");
        }

        QWidget* page =
            window.findChild<QWidget*>(QStringLiteral("xqStagePage_Modules"));
        QWidget* oldFlowPage =
            window.findChild<QWidget*>(QStringLiteral("xqStagePage_Flow"));
        QPushButton* oldFlowRun =
            window.findChild<QPushButton*>(QStringLiteral("xqFlowSmokeRun"));
        QComboBox* oldFlowSource =
            window.findChild<QComboBox*>(QStringLiteral("xqFlowSourceCombo"));
        QLabel* oldFlowProtocol =
            window.findChild<QLabel*>(QStringLiteral("xqFlowSmokeProtocol"));
        QPushButton* preparePathInput =
            window.findChild<QPushButton*>(QStringLiteral("xqPathInputBuild"));
        QAction* openDicom =
            window.findChild<QAction*>(QStringLiteral("xqOpenDicomAction"));
        QComboBox* profilePath =
            window.findChild<QComboBox*>(QStringLiteral("xqPathInputPathCombo"));
        QComboBox* profileContours =
            window.findChild<QComboBox*>(QStringLiteral("xqPathInputContourCombo"));
        QLabel* status =
            window.findChild<QLabel*>(QStringLiteral("xqStageStatus_Modules"));
        QComboBox* pathModuleSource =
            window.findChild<QComboBox*>(QStringLiteral("xqPathModuleSourceCombo"));
        QComboBox* pathModuleCombo =
            window.findChild<QComboBox*>(QStringLiteral("xqPathModuleCombo"));
        QPushButton* pathModuleRun =
            window.findChild<QPushButton*>(QStringLiteral("xqPathModuleRun"));
        QLabel* pathSource =
            window.findChild<QLabel*>(QStringLiteral("xqPathModuleSource"));
        QLabel* pathGeometry =
            window.findChild<QLabel*>(QStringLiteral("xqPathModuleGeometry"));
        QPlainTextEdit* pathDump =
            window.findChild<QPlainTextEdit*>(QStringLiteral("xqPathContractDump"));
        QStackedWidget* panel =
            window.findChild<QStackedWidget*>(QStringLiteral("xqStagePanel"));
        if (page == nullptr || oldFlowPage != nullptr || oldFlowRun != nullptr
            || oldFlowSource != nullptr || oldFlowProtocol != nullptr
            || preparePathInput == nullptr
            || openDicom == nullptr || !openDicom->isEnabled()
            || profilePath == nullptr || profileContours == nullptr
            || pathModuleSource == nullptr || pathModuleCombo == nullptr
            || pathModuleRun == nullptr || pathSource == nullptr
            || pathGeometry == nullptr || pathDump == nullptr
            || status == nullptr || panel == nullptr
            || panel->count() != 6
            || !profilePath->isEnabled()
            || !profileContours->isEnabled()
            || !pathModuleRun->isEnabled() || !pathModuleSource->isEnabled()
            || pathModuleCombo->count() != 2
            || page->property("xqCapabilityState").toString()
                != QStringLiteral("available")
            || status->text().isEmpty()) {
            return fail("runtime Flow-off shell exposes Modules without old Flow entry");
        }

        static_cast<xq::NodeComboBox*>(pathModuleSource)->repopulate();
        const int moduleProfileIndex = pathModuleSource->findData(
            QVariant::fromValue<qulonglong>(moduleProfileId.value()));
        const int validateIndex = pathModuleCombo->findData(
            QStringLiteral("path-validate"));
        if (moduleProfileIndex < 0 || validateIndex < 0) {
            return fail("runtime Flow-off lists Path inputs and built-in modules");
        }
        pathModuleSource->setCurrentIndex(moduleProfileIndex);
        pathModuleCombo->setCurrentIndex(validateIndex);
        pathModuleRun->click();
        if (!status->text().contains(QStringLiteral("Path module completed"))
            || pathSource->text() != QStringLiteral("Gold Path file")
            || !pathGeometry->text().contains(QStringLiteral("stations"))
            || !pathDump->toPlainText().contains(QStringLiteral("vessel_path_v1"))
            || !pathDump->toPlainText().contains(QStringLiteral("source=gold_file"))) {
            return fail("runtime Flow-off runs PathValidate and exposes source/dump");
        }

        QTreeView* tree =
            window.findChild<QTreeView*>(QStringLiteral("xqSceneTreeView"));
        QAbstractItemModel* model = tree != nullptr ? tree->model() : nullptr;
        bool historicalVisible = false;
        if (model != nullptr) {
            for (int group = 0; group < model->rowCount(QModelIndex()); ++group) {
                const QModelIndex groupIndex = model->index(group, 0, QModelIndex());
                for (int node = 0; node < model->rowCount(groupIndex); ++node) {
                    const QModelIndex nodeIndex = model->index(node, 0, groupIndex);
                    historicalVisible = historicalVisible
                        || model->data(nodeIndex, Qt::DisplayRole).toString()
                            == QStringLiteral("Historical Flow Result");
                }
            }
        }
        if (!historicalVisible) {
            return fail("runtime Flow-off keeps historical FlowResult visible");
        }
    }

#if XQ_ENABLE_FLOW
    // Flow-enabled builds retain the backend controller for later core/module
    // use, but the Shell-A GUI must not restore the superseded smoke entry.
    {
        xq::XQProject project;
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open Flow-on shell project");
        }

        xq::XQCommandStack stack;
        xq::XQMainWindow window;
        window.attachWorkflow(&project, &stack);
        if (window.flowSmokeController() == nullptr
            || window.findChild<QWidget*>(QStringLiteral("xqStagePage_Modules")) == nullptr
            || window.findChild<QWidget*>(QStringLiteral("xqStagePage_Flow")) != nullptr
            || window.findChild<QPushButton*>(QStringLiteral("xqFlowSmokeRun")) != nullptr
            || window.findChild<QComboBox*>(QStringLiteral("xqFlowSourceCombo")) != nullptr
            || window.findChild<QLabel*>(QStringLiteral("xqFlowSmokeProtocol")) != nullptr) {
            return fail("Flow-on keeps backend controller but no Shell-A Flow entry");
        }
    }
#endif

    xq::XQScene scene;
    if (scene.insert(xq::XQDataNode(xq::NodeId(1), "volume", "Synthetic Volume"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert volume node");
    }
    if (scene.insert(xq::XQDataNode(xq::NodeId(2), "mesh", "Derived Mesh"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert mesh node");
    }

    xq::XQMainWindow window;
    window.setScene(&scene);

    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    if (tree == nullptr) {
        return fail("main window exposes scene tree view");
    }

    QAbstractItemModel* model = tree->model();
    if (model == nullptr) {
        return fail("scene tree view has a model");
    }
    // Tree equivalent of the old flat "rowCount == node count": the root rows are
    // the non-empty type groups, and the child rows across all groups sum to the
    // scene node count. (Both fixture nodes are legacy/Unknown, so they share the
    // Ungrouped root -> one group row, two child rows.)
    {
        int expectedGroups = 0;
        {
            bool seen[8] = {false, false, false, false, false, false, false, false};
            scene.visit_nodes([&](const xq::XQDataNode& node) {
                const int g = static_cast<int>(
                    xq::XQScene::groupForDomain(node.domainType()));
                if (!seen[g]) {
                    seen[g] = true;
                    ++expectedGroups;
                }
            });
        }
        if (model->rowCount(QModelIndex()) != expectedGroups) {
            return fail("tree model root rowCount matches non-empty group count");
        }
        int childSum = 0;
        for (int g = 0; g < model->rowCount(QModelIndex()); ++g) {
            childSum += model->rowCount(model->index(g, 0, QModelIndex()));
        }
        if (childSum != static_cast<int>(count_nodes(scene))) {
            return fail("tree model child rows sum to the scene node count");
        }
    }

    // --- workflow integration: attach a mutable scene + command stack ---
    xq::XQScene workflowScene;
    xq::XQCommandStack stack;
    const xq::NodeId imageId(1);
    workflowScene.insert(xq::XQDataNode(imageId, xq::XQDomainType::Image, "img",
                                        std::make_shared<xq::XQSourcePayload>(
                                            xq::XQDomainType::Image, "Images/x.vti")));

    xq::XQMainWindow workflowWindow;
    workflowWindow.attachWorkflow(&workflowScene, &stack);
    workflowWindow.resize(1440, 920);
    workflowWindow.show();
    QApplication::processEvents();

    if (workflowWindow.findChild<QStackedWidget*>("xqStagePanel") == nullptr) {
        return fail("attachWorkflow builds the stage panel");
    }
    QDockWidget* stageDock = workflowWindow.findChild<QDockWidget*>("xqStageDock");
    if (stageDock == nullptr || stageDock->isVisible()) {
        return fail("workflow stage dock is available but hidden in the default MITK-like layout");
    }
    QDockWidget* imageNavigatorDock =
        workflowWindow.findChild<QDockWidget*>("xqImageNavigatorDock");
    if (imageNavigatorDock == nullptr || !imageNavigatorDock->isVisible()) {
        return fail("image navigator is visible below the data manager by default");
    }
    QDockWidget* dataManagerDock =
        workflowWindow.findChild<QDockWidget*>("xqDataManagerDock");
    if (dataManagerDock == nullptr || !dataManagerDock->isVisible()) {
        return fail("data manager is visible above the image navigator by default");
    }
    if (!close_enough(dataManagerDock->width(), imageNavigatorDock->width(), 2)
        || !close_enough(dataManagerDock->x(), imageNavigatorDock->x(), 2)) {
        return fail("left data and image navigator docks share one aligned column");
    }
    QTabWidget* workspaceTabs = workflowWindow.findChild<QTabWidget*>("xqWorkspaceTabs");
    if (workspaceTabs == nullptr || workspaceTabs->count() != 1
        || workspaceTabs->tabText(0).isEmpty()) {
        return fail("central workspace exposes the Standard Display tab");
    }
    if (workflowWindow.pathController() == nullptr
        || workflowWindow.aiController() == nullptr) {
        return fail("attachWorkflow wires the stage controllers");
    }

    // Status-bar memory is real (R4): matches "<zh>内存</zh> N MB" with N > 0,
    // never the old hard-coded "Mem: -- MB" placeholder.
    QLabel* memLabel = workflowWindow.findChild<QLabel*>("xqStatusMem");
    if (memLabel == nullptr) {
        return fail("status bar exposes the memory label");
    }
    const QString memText = memLabel->text();
    const QRegularExpression memPattern(
        QStringLiteral("\\d+ MB"));
    if (!memPattern.match(memText).hasMatch() || memText.contains(QStringLiteral("--"))) {
        return fail("status bar shows a real working-set figure");
    }

    xq::XQMprWidget* mpr = workflowWindow.findChild<xq::XQMprWidget*>("xqMprView");
    if (mpr == nullptr) {
        return fail("workflow window exposes MPR view");
    }
    const QRect mprWindowRect(mpr->mapTo(&workflowWindow, QPoint(0, 0)),
                              mpr->size());
    if (!mprWindowRect.intersects(workflowWindow.rect())) {
        return fail("MPR canvas is visible in the central workspace");
    }
    QFrame* axialFrame = mpr->findChild<QFrame*>("xqMprAxialFrame");
    QFrame* sagittalFrame = mpr->findChild<QFrame*>("xqMprSagittalFrame");
    QFrame* coronalFrame = mpr->findChild<QFrame*>("xqMprCoronalFrame");
    QFrame* volumeFrame = mpr->findChild<QFrame*>("xqMprVolumeFrame");
    if (axialFrame == nullptr || sagittalFrame == nullptr || coronalFrame == nullptr
        || volumeFrame == nullptr) {
        return fail("MPR exposes all four 2x2 frames");
    }
    const int minCellWidth = std::min(
        std::min(axialFrame->width(), sagittalFrame->width()),
        std::min(coronalFrame->width(), volumeFrame->width()));
    const int maxCellWidth = std::max(
        std::max(axialFrame->width(), sagittalFrame->width()),
        std::max(coronalFrame->width(), volumeFrame->width()));
    const int minCellHeight = std::min(
        std::min(axialFrame->height(), sagittalFrame->height()),
        std::min(coronalFrame->height(), volumeFrame->height()));
    const int maxCellHeight = std::max(
        std::max(axialFrame->height(), sagittalFrame->height()),
        std::max(coronalFrame->height(), volumeFrame->height()));
    if (!close_enough(minCellWidth, maxCellWidth)
        || !close_enough(minCellHeight, maxCellHeight)) {
        return fail("MPR 2x2 cells stay equal-sized in the product layout");
    }
    // B3: the opacity slider is a live presentation control again (07-03 B2
    // deleted it; this batch restores it). Only the time slider stays a deleted
    // dead placeholder. The opacity slider + colour button exist but start
    // disabled (nothing renderable is selected yet).
    if (workflowWindow.findChild<QSlider*>("xqNavTimeSlider") != nullptr) {
        return fail("dead placeholder time slider stays deleted");
    }
    QSlider* opacitySlider = workflowWindow.findChild<QSlider*>("xqOpacitySlider");
    QPushButton* colorButton = workflowWindow.findChild<QPushButton*>("xqColorButton");
    if (opacitySlider == nullptr || colorButton == nullptr) {
        return fail("data manager exposes the opacity slider + colour button");
    }
    if (opacitySlider->isEnabled() || colorButton->isEnabled()) {
        return fail("presentation controls start disabled with nothing selected");
    }
    QAction* pathAction = workflowWindow.findChild<QAction*>("xqToolbarPathAction");
    QAction* imageAction = workflowWindow.findChild<QAction*>("xqToolbarImageAction");
    QAction* segmentationAction =
        workflowWindow.findChild<QAction*>("xqToolbarSeg2dAction");
    QAction* modulesAction =
        workflowWindow.findChild<QAction*>("xqToolbarModulesAction");
    if (pathAction == nullptr || imageAction == nullptr
        || segmentationAction == nullptr || modulesAction == nullptr) {
        return fail("toolbar workflow actions expose stable object names");
    }
    modulesAction->trigger();
    QApplication::processEvents();
    QStackedWidget* workflowStages =
        workflowWindow.findChild<QStackedWidget*>("xqStagePanel");
    if (workflowStages == nullptr || workflowStages->currentIndex() != 4
        || workflowStages->currentWidget() == nullptr
        || workflowStages->currentWidget()->objectName()
            != QStringLiteral("xqStagePage_Modules")) {
        return fail("Modules toolbar action opens the replacement fifth page");
    }
    pathAction->trigger();
    QApplication::processEvents();
    if (!stageDock->isVisible()) {
        return fail("workflow stage dock opens when a stage toolbar action is triggered");
    }
    imageAction->trigger();
    QApplication::processEvents();
    if (stageDock->isVisible()) {
        return fail("Image toolbar action returns to the unobstructed Standard Display workspace");
    }
    if (!workflowWindow.loadImageFromPath(QString::fromUtf8(XQ_TEST_VTI_PATH))) {
        return fail("workflow window queues a real image before MPR seed picking");
    }
    // B4b: image decoding now rides the background parse queue (a large .vti no
    // longer freezes the GUI thread), so the navigator only enables once the
    // decode commits. Pump the loop until the axial slice spin enables (bounded
    // ~10s) -- the same wait pattern the .mdl/.ctgr pre-parse assertions use.
    QSpinBox* axialSpin = workflowWindow.findChild<QSpinBox*>("xqNavAxialSpin");
    if (axialSpin == nullptr) {
        return fail("image navigator exposes the axial slice spin box");
    }
    {
        QElapsedTimer imageTimer;
        imageTimer.start();
        while (!axialSpin->isEnabled() && imageTimer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }
    if (!axialSpin->isEnabled()
        || axialSpin->maximum() <= axialSpin->minimum()) {
        return fail("image navigator slice spin boxes enable only after a real image loads");
    }
    // B2b: the window/level numeric boxes enable with a real image and carry a
    // seeded (non-zero-width) window.
    QDoubleSpinBox* windowSpin =
        workflowWindow.findChild<QDoubleSpinBox*>("xqNavWindowSpin");
    QDoubleSpinBox* levelSpin =
        workflowWindow.findChild<QDoubleSpinBox*>("xqNavLevelSpin");
    if (windowSpin == nullptr || levelSpin == nullptr) {
        return fail("image navigator exposes window/level spin boxes");
    }
    if (!windowSpin->isEnabled() || !levelSpin->isEnabled()) {
        return fail("window/level spins enable after a real image loads");
    }
    if (windowSpin->value() <= 0.0) {
        return fail("window spin seeds a positive window width");
    }

    // B4: the navigator Loc.(mm) row exposes three world-coordinate spins, enabled
    // once a real image loads, two-way bound to the slice sliders.
    QDoubleSpinBox* locX = workflowWindow.findChild<QDoubleSpinBox*>("xqNavLocX");
    QDoubleSpinBox* locY = workflowWindow.findChild<QDoubleSpinBox*>("xqNavLocY");
    QDoubleSpinBox* locZ = workflowWindow.findChild<QDoubleSpinBox*>("xqNavLocZ");
    if (locX == nullptr || locY == nullptr || locZ == nullptr) {
        return fail("image navigator exposes the three Loc.(mm) spins");
    }
    if (!locX->isEnabled() || !locY->isEnabled() || !locZ->isEnabled()) {
        return fail("Loc.(mm) spins enable after a real image loads");
    }
    // Slice -> Loc: moving the Axial slice spin (axis 2) changes the Z world
    // read-out (spacing is non-zero for the real 0007 image).
    const int axialIdxBefore = axialSpin->value();
    const double locZBefore = locZ->value();
    const int axialTarget =
        (axialIdxBefore == axialSpin->maximum()) ? axialSpin->minimum()
                                                 : axialSpin->maximum();
    axialSpin->setValue(axialTarget);
    QApplication::processEvents();
    const double locZAfter = locZ->value();
    if (std::abs(locZAfter - locZBefore) < 1.0e-4) {
        return fail("moving the Axial slice spin updates the Loc.(mm) Z read-out");
    }
    // Loc -> slice: setting the Z world value back drives the Axial slice spin
    // back to its original index (world round-trips to voxel index).
    locZ->setValue(locZBefore);
    QApplication::processEvents();
    if (axialSpin->value() != axialIdxBefore) {
        return fail("editing the Loc.(mm) Z spin drives the Axial slice spin");
    }

    // B4: the status bar shows the world Position as "Position: <x, y, z> mm" once
    // a slice moves (idle read-out). Nudge the axial spin to publish it.
    axialSpin->setValue(axialTarget);
    QApplication::processEvents();
    QLabel* posLabel = workflowWindow.findChild<QLabel*>("xqStatusPosition");
    if (posLabel == nullptr) {
        return fail("status bar exposes the position label");
    }
    const QString posText = posLabel->text();
    if (!posText.contains(QStringLiteral("mm"))) {
        return fail("status Position read-out is expressed in mm");
    }
    // Three signed/decimal numbers inside the "<...>" bracket.
    const QRegularExpression posPattern(
        QStringLiteral("<\\s*-?\\d+\\.\\d+,\\s*-?\\d+\\.\\d+,\\s*-?\\d+\\.\\d+\\s*>"));
    if (!posPattern.match(posText).hasMatch()) {
        return fail("status Position read-out carries three decimal coordinates");
    }
    // Restore the axial slice to its starting index so downstream assertions see
    // the pre-B4 navigator state.
    axialSpin->setValue(axialIdxBefore);
    QApplication::processEvents();

    // The Axial slice cell keeps the "xqMprAxial" object name; it is now a slice
    // view widget (QVTKOpenGLNativeWidget host), not an offscreen QLabel.
    QWidget* axial = mpr->findChild<QWidget*>("xqMprAxial");
    if (axial == nullptr) {
        return fail("MPR exposes axial slice view");
    }
    // B2b: seed picking is live (click -> displayToWorld -> worldToVoxelIndex ->
    // voxelPicked -> seedPicked). The voxel maths is covered headlessly by
    // test_render_scene (test 18); the click end of the chain needs a real GL
    // context (offscreen mounts no renderer), so the behaviour assertion here is
    // the API surface: pick mode toggles + marker clear stay safe, and no
    // spurious seedPicked fires without a real click.
    bool pickedSeed = false;
    QObject::connect(mpr, &xq::XQMprWidget::seedPicked, mpr,
                     [&pickedSeed](int, int, int) { pickedSeed = true; });
    workflowWindow.show();
    app.processEvents();
    mpr->setSeedPickingEnabled(true);
    mpr->clearSeedMarker();
    mpr->setSeedPickingEnabled(false);
    // No click was synthesized -> nothing may fire (a spurious emission would
    // mean pick mode leaks events). The real click chain is a live-machine item.
    if (pickedSeed) {
        return fail("seedPicked must not fire without a real click");
    }

    // AC1 programmatic check: load the real 0007 .vti through the same path as
    // File -> Open Image, but without popping QFileDialog.
    xq::XQScene imageScene;
    xq::XQCommandStack imageStack;
    xq::XQMainWindow imageWindow;
    imageWindow.attachWorkflow(&imageScene, &imageStack);
    if (!imageWindow.loadImageFromPath(QString::fromUtf8(XQ_TEST_VTI_PATH))) {
        return fail("main window queues the real .vti image");
    }
    // B4b: the decode + node seed now land on the background-queue commit; pump
    // the loop until the Image node appears (bounded ~10s).
    {
        auto imageNodeCount = [&imageScene]() -> std::size_t {
            const std::map<xq::XQDomainType, std::size_t> domains =
                count_domains(imageScene);
            const auto it = domains.find(xq::XQDomainType::Image);
            return (it == domains.end()) ? 0U : it->second;
        };
        QElapsedTimer seedTimer;
        seedTimer.start();
        while (imageNodeCount() != 1U && seedTimer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        if (imageNodeCount() != 1U) {
            return fail("open image seeds one Image node into the scene");
        }
    }
    xq::XQMprWidget* imageMpr = imageWindow.findChild<xq::XQMprWidget*>("xqMprView");
    if (imageMpr == nullptr) {
        return fail("image load window exposes MPR view");
    }
    if (imageMpr->sliceCount(0) != 100
        || imageMpr->sliceCount(1) != 512
        || imageMpr->sliceCount(2) != 512) {
        return fail("MPR slice counts match the real 0007 image dimensions");
    }

    // AC2 programmatic check: load the real SimVascular project and verify the
    // live scene receives the expected typed node families.
    xq::XQScene projectScene;
    xq::XQCommandStack projectStack;
    xq::XQMainWindow projectWindow;
    projectWindow.attachWorkflow(&projectScene, &projectStack);
    // B4c: count background parse turns. Same-kind jobs are merged into one runner
    // task, so the whole project (1 image + 1 model + 5 contour groups) should
    // start at most 3 parse tasks (image batch + model batch + contour batch), not
    // one per file. Connected before the load so no taskStarted is missed.
    int parseTaskCount = 0;
    const QString contourTaskLabel =
        QCoreApplication::translate("xq::XQMainWindow",
                                    "Parsing contours (%1)...")
            .arg(5);
    const std::string semanticEditPath =
        "Segmentations/semantic-edit-during-materialize.ctgr";
    bool contourRaceEditApplied = false;
    bool contourRaceTaskFinished = false;
    bool contourRaceSetupFailed = false;
    xq::NodeId contourRaceNodeId = xq::NodeId::invalid();
    std::string contourRaceOriginalPath;
    xq::ContentRevision contourRaceOriginalRevision = 0;
    bool contourRaceOriginalStale = false;
    std::size_t contourRaceUndoCountAfterEdit = 0;
    QObject::connect(&projectWindow.taskRunner(), &xq::XQTaskRunner::taskStarted,
                     &projectWindow, [&](const QString& label) {
                         ++parseTaskCount;
                         if (label != contourTaskLabel || contourRaceEditApplied
                             || contourRaceSetupFailed) {
                             return;
                         }

                         projectScene.visit_nodes([&](const xq::XQDataNode& node) {
                             if (!contourRaceNodeId.is_valid()
                                 && node.domainType()
                                        == xq::XQDomainType::ContourGroup
                                 && std::dynamic_pointer_cast<xq::XQSourcePayload>(
                                        node.payload())
                                        != nullptr) {
                                 contourRaceNodeId = node.id();
                             }
                         });
                         xq::XQDataNode* target =
                             projectScene.find(contourRaceNodeId);
                         const std::shared_ptr<xq::XQSourcePayload> source =
                             target != nullptr
                             ? std::dynamic_pointer_cast<xq::XQSourcePayload>(
                                   target->payload())
                             : nullptr;
                         if (target == nullptr || source == nullptr) {
                             contourRaceSetupFailed = true;
                             return;
                         }

                         contourRaceOriginalPath = source->sourcePath();
                         contourRaceOriginalRevision = target->contentRevision();
                         contourRaceOriginalStale =
                             projectScene.is_stale(contourRaceNodeId);
                         if (!projectStack.push(
                                 std::make_unique<
                                     xq::SemanticReplacePayloadCommand>(
                                     &projectScene,
                                     contourRaceNodeId,
                                     xq::XQDomainType::ContourGroup,
                                     std::make_shared<xq::XQSourcePayload>(
                                         xq::XQDomainType::ContourGroup,
                                         semanticEditPath),
                                     "Test edit during materialization"))) {
                             contourRaceSetupFailed = true;
                             return;
                         }
                         contourRaceEditApplied = true;
                         contourRaceUndoCountAfterEdit =
                             projectStack.undo_count();
                     });
    QObject::connect(&projectWindow.taskRunner(), &xq::XQTaskRunner::taskFinished,
                     &projectWindow, [&](const QString& label) {
                         if (label == contourTaskLabel) {
                             contourRaceTaskFinished = true;
                         }
                     });
    if (!projectWindow.loadSvProjectFromDirectory(QString::fromUtf8(XQ_SVPROJECT_DIR))) {
        return fail("main window loads the real SimVascular project");
    }
    const std::size_t materializationUndoBaseline = projectStack.undo_count();
    std::map<xq::NodeId, xq::ContentRevision> materializationRevisions;
    std::map<xq::NodeId, bool> materializationStale;
    projectScene.visit_nodes([&](const xq::XQDataNode& node) {
        if (node.domainType() == xq::XQDomainType::SurfaceModel
            || node.domainType() == xq::XQDomainType::ContourGroup) {
            materializationRevisions[node.id()] = node.contentRevision();
            materializationStale[node.id()] = projectScene.is_stale(node.id());
        }
    });
    const std::map<xq::XQDomainType, std::size_t> projectDomains =
        count_domains(projectScene);
    if (projectDomains.find(xq::XQDomainType::Image) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::Image) != 1U) {
        return fail("SV project seeds one image node");
    }
    if (projectDomains.find(xq::XQDomainType::Path) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::Path) != 5U) {
        return fail("SV project seeds path nodes");
    }
    if (projectDomains.find(xq::XQDomainType::ContourGroup) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::ContourGroup) != 5U) {
        return fail("SV project seeds contour group nodes");
    }
    if (projectDomains.find(xq::XQDomainType::Mesh) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::Mesh) != 1U) {
        return fail("SV project seeds one mesh node");
    }
    if (projectDomains.find(xq::XQDomainType::SimulationCase) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::SimulationCase) != 1U) {
        return fail("SV project seeds one simulation case node");
    }
    xq::XQMprWidget* projectMpr = projectWindow.findChild<xq::XQMprWidget*>("xqMprView");
    if (projectMpr == nullptr) {
        return fail("project load window exposes MPR view");
    }
    // B4b: the SV project's image is now decoded + uploaded on the background-queue
    // commit (front-of-queue so the underlay lands first). Pump the loop until the
    // MPR carries the volume (slice counts non-zero) before asserting them.
    {
        QElapsedTimer projImageTimer;
        projImageTimer.start();
        while (projectMpr->sliceCount(2) <= 0 && projImageTimer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }
    if (projectMpr->sliceCount(0) != 100
        || projectMpr->sliceCount(1) != 512
        || projectMpr->sliceCount(2) != 512) {
        return fail("SV project displays its real image in MPR");
    }

    // B2a: the SV project's surface-model (.mdl) node is parsed in the background
    // after load (no user action) and its payload is swapped from the unresolved
    // XQSourcePayload to a real XQSurfaceModelPayload. Discrete invariant: pump
    // the event loop until that swap lands (bounded ~10s), then assert the parsed
    // triangle geometry is present. The project must carry exactly one model node.
    if (projectDomains.find(xq::XQDomainType::SurfaceModel) == projectDomains.end()
        || projectDomains.at(xq::XQDomainType::SurfaceModel) != 1U) {
        return fail("SV project seeds one surface-model node");
    }
    // Reads the first surface-model node's payload as an XQSurfaceModelPayload, or
    // null while it is still the unresolved source payload / being parsed.
    auto resolvedModelPayload = [&projectScene]()
        -> std::shared_ptr<xq::XQSurfaceModelPayload> {
        std::shared_ptr<xq::XQSurfaceModelPayload> found;
        projectScene.visit_nodes([&found](const xq::XQDataNode& node) {
            if (found != nullptr
                || node.domainType() != xq::XQDomainType::SurfaceModel) {
                return;
            }
            found = std::dynamic_pointer_cast<xq::XQSurfaceModelPayload>(node.payload());
        });
        return found;
    };
    QElapsedTimer modelTimer;
    modelTimer.start();
    while (resolvedModelPayload() == nullptr && modelTimer.elapsed() < 10000) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const std::shared_ptr<xq::XQSurfaceModelPayload> modelPayload = resolvedModelPayload();
    if (modelPayload == nullptr) {
        return fail("SV project surface-model node is pre-parsed to a model payload");
    }
    if (!modelPayload->model().hasTriangleGeometry()) {
        return fail("pre-parsed surface model carries triangle geometry");
    }
    // B2c data anchor: the real 0007_H_AO_H/Models/0090_0001.vtp is 84542 points /
    // 169080 triangles (verified against the source file). Asserting the exact
    // counts locks in "the whole model was read" -- a truncated/partial parse (the
    // "model shows incomplete" symptom) would trip this instead of silently
    // rendering fewer triangles.
    const auto triangleGeometry = modelPayload->model().triangleGeometry();
    if (triangleGeometry == nullptr) {
        return fail("pre-parsed surface model triangle geometry handle is non-null");
    }
    if (triangleGeometry->pointCount() != 84542) {
        return fail("pre-parsed surface model has 84542 points (full read)");
    }
    if (triangleGeometry->triangleCount() != 169080) {
        return fail("pre-parsed surface model has 169080 triangles (full read)");
    }

    // A queued contour parse must not land over a semantic edit that happens
    // after the worker snapshot was captured. taskStarted is emitted before the
    // worker hop, so the hook above replaces one unresolved source with another
    // same-domain source path at the exact race boundary. The old type-only guard
    // would overwrite it; revision + payload identity + source path must reject it.
    {
        QElapsedTimer raceTimer;
        raceTimer.start();
        while (!contourRaceTaskFinished && raceTimer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }
    if (contourRaceSetupFailed || !contourRaceEditApplied
        || !contourRaceTaskFinished || !contourRaceNodeId.is_valid()) {
        return fail("contour materialization race hook executes and finishes");
    }
    xq::XQDataNode* raceNode = projectScene.find(contourRaceNodeId);
    std::shared_ptr<xq::XQSourcePayload> editedSource =
        raceNode != nullptr
        ? std::dynamic_pointer_cast<xq::XQSourcePayload>(raceNode->payload())
        : nullptr;
    if (editedSource == nullptr || editedSource->sourcePath() != semanticEditPath) {
        return fail("old contour worker does not overwrite a semantic source edit");
    }
    if (raceNode->contentRevision() != contourRaceOriginalRevision + 1) {
        return fail("semantic edit revision survives rejected materialization");
    }
    if (projectScene.is_stale(contourRaceNodeId) != contourRaceOriginalStale) {
        return fail("rejected materialization does not change stale state");
    }
    if (projectStack.undo_count() != contourRaceUndoCountAfterEdit) {
        return fail("materialization does not add an undo entry");
    }

    // Restore the fixture's original source state, then materialize it directly
    // so the pre-existing five-contour integration assertions below retain their
    // full coverage. This mirrors the production non-semantic setPayload commit.
    if (!projectStack.undo()) {
        return fail("undo restores the contour source after the race probe");
    }
    raceNode = projectScene.find(contourRaceNodeId);
    const std::shared_ptr<xq::XQSourcePayload> restoredSource =
        raceNode != nullptr
        ? std::dynamic_pointer_cast<xq::XQSourcePayload>(raceNode->payload())
        : nullptr;
    if (restoredSource == nullptr
        || restoredSource->sourcePath() != contourRaceOriginalPath
        || raceNode->contentRevision() != contourRaceOriginalRevision
        || projectScene.is_stale(contourRaceNodeId) != contourRaceOriginalStale
        || projectStack.undo_count() != materializationUndoBaseline) {
        return fail("semantic undo restores source revision stale and undo depth");
    }

    QString restoredPath =
        QString::fromStdString(contourRaceOriginalPath);
    if (QFileInfo(restoredPath).isRelative()) {
        restoredPath =
            QDir(QString::fromUtf8(XQ_SVPROJECT_DIR)).filePath(restoredPath);
    }
    xq::CTGRReadResult restoredRead;
    if (xq::CTGRContourReader::read(restoredPath.toStdString(), &restoredRead)
        != xq::CTGRContourReader::Status::Ok) {
        return fail("race probe re-reads the restored contour source");
    }
    raceNode->setPayload(
        xq::XQDomainType::ContourGroup,
        std::make_shared<xq::XQContourGroupPayload>(
            std::move(restoredRead.group)));
    if (raceNode->contentRevision() != contourRaceOriginalRevision
        || projectScene.is_stale(contourRaceNodeId) != contourRaceOriginalStale
        || projectStack.undo_count() != materializationUndoBaseline) {
        return fail("direct materialization preserves revision stale and undo depth");
    }

    // B3: the SV project's contour-group (.ctgr) nodes are parsed in the
    // background too (no user action), swapping the unresolved XQSourcePayload for
    // a real XQContourGroupPayload whose group carries contours. Pump the loop
    // until all five ContourGroup nodes resolve (bounded ~10s).
    auto allContoursResolved = [&projectScene]() -> bool {
        std::size_t resolved = 0;
        bool anyUnresolvedEmpty = false;
        projectScene.visit_nodes([&](const xq::XQDataNode& node) {
            if (node.domainType() != xq::XQDomainType::ContourGroup) {
                return;
            }
            auto payload =
                std::dynamic_pointer_cast<xq::XQContourGroupPayload>(node.payload());
            if (payload == nullptr) {
                return;
            }
            if (payload->group().contours().empty()) {
                anyUnresolvedEmpty = true;
            }
            ++resolved;
        });
        return resolved == 5U && !anyUnresolvedEmpty;
    };
    QElapsedTimer contourTimer;
    contourTimer.start();
    while (!allContoursResolved() && contourTimer.elapsed() < 10000) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!allContoursResolved()) {
        return fail("all five contour-group nodes pre-parse to non-empty payloads");
    }
    for (const auto& entry : materializationRevisions) {
        const xq::XQDataNode* materialized = projectScene.find(entry.first);
        if (materialized == nullptr
            || materialized->contentRevision() != entry.second
            || projectScene.is_stale(entry.first)
                   != materializationStale.at(entry.first)) {
            return fail("model and contour materialization preserve revision and stale");
        }
    }
    if (projectStack.undo_count() != materializationUndoBaseline) {
        return fail("model and contour materialization preserve undo depth");
    }

    // B4c batch-merge invariant: all of the above resolved (image + model + 5
    // contour groups) yet the runner started at most 3 parse tasks -- one per
    // same-kind batch, not one per file (the pre-B4c code started 7). A count of 0
    // would mean the taskStarted wiring never fired (i.e. a false green), so the
    // lower bound guards that too.
    if (parseTaskCount < 1 || parseTaskCount > 3) {
        return fail("SV project pre-parse merges same-kind jobs (<=3 parse tasks)");
    }

    // B3c: the scene tree is a single-column, header-hidden two-level tree (type
    // groups -> nodes) with the visibility checkbox folded into the name column.
    // The dropped Type/Status columns freed the width they stole; domain type and
    // staleness now live in the icon, amber tint, and tooltip.
    QTreeView* projectTree = projectWindow.findChild<QTreeView*>("xqSceneTreeView");
    if (projectTree == nullptr) {
        return fail("project window exposes the scene tree view");
    }
    QAbstractItemModel* projectModel = projectTree->model();
    if (projectModel == nullptr) {
        return fail("project scene tree has a model");
    }
    if (projectModel->columnCount(QModelIndex()) != xq::XQSceneModel::ColumnCount) {
        return fail("project scene model exposes only the name column");
    }
    // The name column (0) stretches so full names never truncate.
    if (projectTree->header()->sectionResizeMode(xq::XQSceneModel::NameColumn)
        != QHeaderView::Stretch) {
        return fail("project scene tree stretches the name column");
    }
    // The single-column tree hides its (now meaningless) header, MITK-style.
    if (!projectTree->isHeaderHidden()) {
        return fail("project scene tree hides its header");
    }

    // Two-level locator: find the name-column index of the first node whose
    // domain matches, walking group rows then their children. The dedicated type
    // column is gone, so the type is read off the name column's tooltip token
    // "[<type>]" (proxy-forwarded, no cast needed) -- proving the type stays
    // reachable after the column was dropped.
    const auto findNodeByDomain =
        [&](xq::XQDomainType domain) -> QModelIndex {
        const QString token = QStringLiteral("[")
            + QString::fromLatin1(xq::domainTypeToString(domain))
            + QStringLiteral("]");
        for (int g = 0; g < projectModel->rowCount(QModelIndex()); ++g) {
            const QModelIndex groupIdx =
                projectModel->index(g, xq::XQSceneModel::NameColumn, QModelIndex());
            for (int n = 0; n < projectModel->rowCount(groupIdx); ++n) {
                const QModelIndex nameIdx =
                    projectModel->index(n, xq::XQSceneModel::NameColumn, groupIdx);
                if (projectModel->data(nameIdx, Qt::ToolTipRole)
                        .toString()
                        .contains(token)) {
                    return nameIdx;
                }
            }
        }
        return QModelIndex();
    };

    // The surface-model node row: checkable, and Checked by default
    // (modeling-first).
    const QModelIndex surfaceNameIdx =
        findNodeByDomain(xq::XQDomainType::SurfaceModel);
    if (!surfaceNameIdx.isValid()) {
        return fail("project scene tree lists the surface-model node");
    }
    if ((projectModel->flags(surfaceNameIdx) & Qt::ItemIsUserCheckable) == 0) {
        return fail("project scene model node name column is user-checkable");
    }
    if (projectModel->data(surfaceNameIdx, Qt::CheckStateRole).toInt() != Qt::Checked) {
        return fail("SurfaceModel node defaults to Checked (modeling-first)");
    }
    // A contour-group node defaults to Unchecked (only image + model start on).
    const QModelIndex contourNameIdx =
        findNodeByDomain(xq::XQDomainType::ContourGroup);
    if (!contourNameIdx.isValid()) {
        return fail("project scene tree lists a contour-group node");
    }
    if (projectModel->data(contourNameIdx, Qt::CheckStateRole).toInt() != Qt::Unchecked) {
        return fail("ContourGroup node defaults to Unchecked (modeling-first)");
    }

    // B3: selecting a renderable node (the resolved surface model) enables the
    // presentation controls.
    QSlider* projOpacity = projectWindow.findChild<QSlider*>("xqOpacitySlider");
    QPushButton* projColor = projectWindow.findChild<QPushButton*>("xqColorButton");
    if (projOpacity == nullptr || projColor == nullptr) {
        return fail("project window exposes the presentation controls");
    }
    projectTree->setCurrentIndex(surfaceNameIdx);
    QApplication::processEvents();
    if (!projOpacity->isEnabled() || !projColor->isEnabled()) {
        return fail("selecting a renderable node enables the presentation controls");
    }

    // --- B6a: the Modeling page's contour-group source is a scene-node dropdown
    // (was a raw NodeId spin box). It must enumerate exactly the scene's
    // ContourGroup nodes, tag each item with a valid NodeId + a name-bearing label,
    // so the user picks a node instead of typing an internal id.
    std::size_t contourNodeCount = 0;
    xq::NodeId firstContourId = xq::NodeId::invalid();
    std::string firstContourName;
    xq::NodeId lastContourId = xq::NodeId::invalid();
    std::string lastContourName;
    projectScene.visit_nodes([&](const xq::XQDataNode& node) {
        if (node.domainType() == xq::XQDomainType::ContourGroup) {
            ++contourNodeCount;
            if (!firstContourId.is_valid()) {
                firstContourId = node.id();
                firstContourName = node.display_name();
            }
            lastContourId = node.id();
            lastContourName = node.display_name();
        }
    });
    if (contourNodeCount == 0 || !firstContourId.is_valid()) {
        return fail("SV project exposes contour-group nodes for the modeling picker");
    }
    // The preselect assertions below need a node that is NOT the combo's first
    // item: after repopulate() Qt auto-selects index 0, so asserting on the first
    // contour group cannot distinguish a real preselect from that default (a
    // fake-green found by tampering setCurrentIndex away).
    if (contourNodeCount < 2 || lastContourId == firstContourId) {
        return fail("SV project provides >=2 contour groups (preselect falsifiability)");
    }
    xq::NodeComboBox* modelingCombo =
        projectWindow.findChild<xq::NodeComboBox*>("xqModelingSourceCombo");
    if (modelingCombo == nullptr) {
        return fail("modeling page exposes its scene-node source dropdown");
    }
    // The panel was built at attach time (empty scene), so refresh from the now-
    // loaded scene the same way opening the dropdown (showPopup) would.
    modelingCombo->repopulate();
    if (static_cast<std::size_t>(modelingCombo->count()) != contourNodeCount) {
        return fail("modeling source dropdown lists every contour-group node");
    }
    bool comboHasValidIds = true;
    bool comboHasFirstName = false;
    for (int i = 0; i < modelingCombo->count(); ++i) {
        const QVariant data = modelingCombo->itemData(i);
        if (!data.isValid()
            || !xq::NodeId(static_cast<xq::NodeId::ValueType>(data.value<qulonglong>()))
                    .is_valid()) {
            comboHasValidIds = false;
        }
        if (modelingCombo->itemText(i).contains(
                QString::fromStdString(firstContourName))) {
            comboHasFirstName = true;
        }
    }
    if (!comboHasValidIds) {
        return fail("each modeling dropdown item carries a valid NodeId");
    }
    if (!comboHasFirstName) {
        return fail("modeling dropdown items show the node display name");
    }

    QStackedWidget* projStagePanel =
        projectWindow.findChild<QStackedWidget*>("xqStagePanel");
    if (projStagePanel == nullptr) {
        return fail("project window exposes the stage panel");
    }

    // B6a selection follow: selecting a contour group in the tree pre-fills the
    // Modeling dropdown WITHOUT switching pages (selecting is not a request to
    // model). Park the panel on page 0 first so a stale index can't mask a no-op,
    // and park the combo on the LAST item so the follow must actively move the
    // selection (repopulate() preserves the current pick and Qt defaults to index
    // 0, so a parked index 0 could not falsify a skipped setCurrentIndex).
    projStagePanel->setCurrentIndex(0);
    modelingCombo->setCurrentIndex(modelingCombo->count() - 1);
    if (modelingCombo->currentData().value<qulonglong>()
        == static_cast<qulonglong>(firstContourId.value())) {
        return fail("combo parked on a non-first item before selection follow");
    }
    projectTree->setCurrentIndex(contourNameIdx);
    QApplication::processEvents();
    if (projStagePanel->currentIndex() != 0) {
        return fail("selecting a contour group does not switch the stage page");
    }
    const QVariant preselected = modelingCombo->currentData();
    if (!preselected.isValid()) {
        return fail("selecting a contour group pre-fills the modeling dropdown");
    }
    const xq::NodeId selectedContourId(
        static_cast<xq::NodeId::ValueType>(preselected.value<qulonglong>()));
    bool selectedIsContour = false;
    projectScene.visit_nodes([&](const xq::XQDataNode& node) {
        if (node.id() == selectedContourId
            && node.domainType() == xq::XQDomainType::ContourGroup) {
            selectedIsContour = true;
        }
    });
    if (!selectedIsContour) {
        return fail("selection follow pre-fills the dropdown with the selected contour");
    }
    if (selectedContourId == lastContourId) {
        return fail("selection follow actually moved the parked combo selection");
    }

    // B6a right-click "Model from <node>": jumps to the Modeling page (index 2) and
    // preselects that contour group in the dropdown. Driven through the test entry
    // point because the right-click QMenu's exec() cannot be pumped offscreen; it
    // shares its implementation with the real action. Target the LAST contour
    // group while the combo sits on the selection-follow pick, so the preselect
    // assertion is falsifiable (see parking note above).
    projStagePanel->setCurrentIndex(0);
    projectWindow.activateStageFromNodeForTest(lastContourId,
                                               xq::XQDomainType::ContourGroup);
    QApplication::processEvents();
    if (projStagePanel->currentIndex() != 2) {
        return fail("Model-from action raises the Modeling stage page");
    }
    if (modelingCombo->currentData().value<qulonglong>()
        != static_cast<qulonglong>(lastContourId.value())) {
        return fail("Model-from action preselects the right-clicked contour group");
    }

    // B2d: the View menu carries a checkable "3D slice planes" toggle (default on)
    // so the user can clear the three grey planes that otherwise occlude the
    // vessel in the 3D view. The window exposes no renderScene_ probe and a
    // test-only getter is disallowed by the brief, so this asserts the action's
    // own state; the setImagePlanesVisible3d kernel logic is covered by
    // test_render_scene (test 16). trigger() toggles the checkable action.
    QAction* planes3dAction = projectWindow.findChild<QAction*>("xqAction3dPlanes");
    if (planes3dAction == nullptr) {
        return fail("View menu exposes the 3D slice-planes action");
    }
    if (!planes3dAction->isCheckable()) {
        return fail("3D slice-planes action is checkable");
    }
    if (!planes3dAction->isChecked()) {
        return fail("3D slice-planes action is checked by default");
    }
    planes3dAction->trigger();
    if (planes3dAction->isChecked()) {
        return fail("triggering the 3D slice-planes action unchecks it");
    }
    planes3dAction->trigger();
    if (!planes3dAction->isChecked()) {
        return fail("triggering the 3D slice-planes action again re-checks it");
    }

    // Drive a stage controller, then exercise the window's undo/redo path.
    // NodeId(2) is taken: loadImageFromPath seeded an Image node (maxId+1) into
    // workflowScene above, and the strict command stack rejects duplicate ids
    // instead of silently tolerating them. Use a far-away id for the new path.
    xq::PathController::AddPathIntent intent;
    intent.newPathId = xq::NodeId(2000);
    intent.name = "centerline";
    intent.sourceImageNode = imageId;
    intent.controlPoints = {{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}, {{1.0, 1.0, 0.0}}};
    intent.spacing = 0.5;
    if (workflowWindow.pathController()->addPath(intent)
        != xq::PathController::Status::Ok) {
        return fail("path controller adds a path through the workflow");
    }
    if (workflowScene.find(xq::NodeId(2000)) == nullptr) {
        return fail("workflow path node is in the scene");
    }

    workflowWindow.undo();
    if (workflowScene.find(xq::NodeId(2000)) != nullptr) {
        return fail("window undo reverts the path command");
    }
    workflowWindow.redo();
    if (workflowScene.find(xq::NodeId(2000)) == nullptr) {
        return fail("window redo restores the path command");
    }

    // --- fixed automatic vessel-segmentation product entry ---
    // The three-dimensional stage exposes exactly the ROI-v2 workflow. The old
    // threshold, region-grow, estimate and seed controls must not remain hidden
    // or disabled on this page. The controller-level test executes the injected
    // pipeline; this shell test protects the user-facing replacement contract.
    QStackedWidget* stagePanel =
        workflowWindow.findChild<QStackedWidget*>("xqStagePanel");
    if (stagePanel == nullptr) {
        return fail("stage panel exists for automatic segmentation");
    }
    QWidget* segPage = workflowWindow.findChild<QWidget*>("xqStagePage_Segmentation");
    if (segPage == nullptr) {
        return fail("segmentation page exists");
    }
    QPushButton* segRun =
        segPage->findChild<QPushButton*>(QStringLiteral("xqSegAutomaticRun"));
    QLineEdit* liverRoi =
        segPage->findChild<QLineEdit*>(QStringLiteral("xqSegLiverRoiPath"));
    QLineEdit* coarseRoi =
        segPage->findChild<QLineEdit*>(QStringLiteral("xqSegCoarseVesselRoiPath"));
    QPushButton* liverBrowse =
        segPage->findChild<QPushButton*>(QStringLiteral("xqSegLiverRoiBrowse"));
    QPushButton* coarseBrowse =
        segPage->findChild<QPushButton*>(QStringLiteral("xqSegCoarseVesselRoiBrowse"));
    QPushButton* openContourWorkbench = segPage->findChild<QPushButton*>(
        QStringLiteral("xqSegOpenContourWorkbench"));
    QLabel* segStatus =
        segPage->findChild<QLabel*>(QStringLiteral("xqStageStatus_Segmentation"));
    if (segRun == nullptr || liverRoi == nullptr || coarseRoi == nullptr
        || liverBrowse == nullptr || coarseBrowse == nullptr
        || openContourWorkbench == nullptr || segStatus == nullptr
        || segRun->property("class").toString() != QStringLiteral("primary")
        || !liverBrowse->isEnabled() || !coarseBrowse->isEnabled()
        || !openContourWorkbench->isEnabled()) {
        return fail("segmentation page exposes the single ROI-v2 action and file pickers");
    }
    if (segPage->findChild<QDoubleSpinBox*>(QStringLiteral("xqSegLowerSpin")) != nullptr
        || segPage->findChild<QDoubleSpinBox*>(QStringLiteral("xqSegUpperSpin")) != nullptr
        || segPage->findChild<QPushButton*>(QStringLiteral("xqSegEstimateBtn")) != nullptr
        || !segPage->findChildren<QRadioButton*>().isEmpty()) {
        return fail("old 3-D threshold and region-grow controls are absent");
    }

    QStackedWidget* centralStack =
        workflowWindow.findChild<QStackedWidget*>(QStringLiteral("xqCentralStack"));
    QWidget* contourPanel = workflowWindow.findChild<QWidget*>(
        QStringLiteral("xqCrossSectionSegPanel"));
    QWidget* contourWorkbench = workflowWindow.findChild<QWidget*>(
        QStringLiteral("xqCrossSectionWorkbench"));
    QWidget* contourThreshold = workflowWindow.findChild<QWidget*>(
        QStringLiteral("xqContourMethodThreshold"));
    if (centralStack == nullptr || contourPanel == nullptr
        || contourWorkbench == nullptr || contourThreshold == nullptr
        || segPage->isAncestorOf(contourThreshold)) {
        return fail("two-dimensional contour workbench remains a separate UI path");
    }

    segmentationAction->trigger();
    QApplication::processEvents();
    if (!stageDock->isVisible() || stageDock->widget() != stagePanel
        || stagePanel->currentIndex() != 1 || !segPage->isVisible()
        || centralStack->currentWidget() != mpr) {
        return fail("Seg toolbar opens the visible ROI-v2 page against the MPR");
    }

    openContourWorkbench->click();
    QApplication::processEvents();
    if (stageDock->widget() != contourPanel || !contourPanel->isVisible()
        || centralStack->currentWidget() != contourWorkbench) {
        return fail("ROI-v2 page opens the separate two-dimensional contour workbench");
    }

    segmentationAction->trigger();
    QApplication::processEvents();
    if (stageDock->widget() != stagePanel || !segPage->isVisible()
        || centralStack->currentWidget() != mpr) {
        return fail("Seg toolbar returns from contours to the ROI-v2 product entry");
    }

    const QString initialSegStatus = segStatus->text();
    const std::size_t nodesBeforeRejectedRun = count_nodes(workflowScene);
    segRun->click();
    QApplication::processEvents();
    if (workflowWindow.taskRunner().busy()
        || count_nodes(workflowScene) != nodesBeforeRejectedRun
        || segStatus->text().isEmpty() || segStatus->text() == initialSegStatus) {
        return fail("missing ROI paths reject before background work or scene mutation");
    }
    liverRoi->setText(QStringLiteral("liver.nii.gz"));
    coarseRoi->setText(QStringLiteral("portal_vein_and_splenic_vein.nii.gz"));
    if (liverRoi->text().isEmpty() || coarseRoi->text().isEmpty()) {
        return fail("ROI file selections remain available to the automatic action");
    }

    // --- B4b: incremental render-sync (uses projectWindow, whose SV project has
    // fully pre-parsed above). The resolved surface model + the project's path
    // nodes are resident render actors. An unrelated command must NOT evict or
    // rebuild an untouched node, and removing a node must drop its actor. Kept at
    // the end because the removal probe undoes the whole project (destructive).
    xq::NodeId projectSurfaceId = xq::NodeId::invalid();
    xq::NodeId projectPathId = xq::NodeId::invalid();
    projectScene.visit_nodes([&](const xq::XQDataNode& node) {
        if (node.domainType() == xq::XQDomainType::SurfaceModel
            && !projectSurfaceId.is_valid()) {
            projectSurfaceId = node.id();
        }
        if (node.domainType() == xq::XQDomainType::Path && !projectPathId.is_valid()) {
            projectPathId = node.id();
        }
    });
    if (!projectSurfaceId.is_valid() || !projectPathId.is_valid()) {
        return fail("SV project exposes a surface-model and a path node id");
    }
    // The surface + path actors are assembled by the incremental sync. A broken
    // diff that never upserts (always "unchanged") would leave these absent, so
    // this pair of assertions is the incremental-path anchor.
    if (!projectWindow.renderSceneHasNode(projectSurfaceId)) {
        return fail("resolved surface model is a resident render actor");
    }
    if (!projectWindow.renderSceneHasNode(projectPathId)) {
        return fail("project path node is a resident render actor");
    }
    const long long surfacePointsBefore =
        projectWindow.renderSceneUploadedPointCount(projectSurfaceId);
    if (surfacePointsBefore <= 0) {
        return fail("resident surface model reports a positive uploaded point count");
    }

    // Unrelated command (undo the SV project's top-of-stack AddNode, then redo it):
    // each fires a full render-sync. The untouched surface model must stay resident
    // with an unchanged uploaded point count -- i.e. the incremental diff skipped
    // its rebuild rather than re-running its decimation.
    projectWindow.undo();
    QApplication::processEvents();
    projectWindow.redo();
    QApplication::processEvents();
    if (!projectWindow.renderSceneHasNode(projectSurfaceId)) {
        return fail("surface model survives an unrelated undo/redo (incremental sync)");
    }
    if (projectWindow.renderSceneUploadedPointCount(projectSurfaceId)
        != surfacePointsBefore) {
        return fail("an unrelated undo/redo does not rebuild the surface model");
    }

    // Node removal: undo the entire project (each undo fires a sync). Once every
    // node has left the scene, the leave-scene sweep must have removed the surface
    // actor from the render side -- exercising the incremental removeNode branch.
    {
        QElapsedTimer undoTimer;
        undoTimer.start();
        while (count_nodes(projectScene) > 0 && undoTimer.elapsed() < 10000) {
            projectWindow.undo();
            QApplication::processEvents();
        }
        if (count_nodes(projectScene) != 0) {
            return fail("undoing the whole project empties the scene");
        }
        if (projectWindow.renderSceneHasNode(projectSurfaceId)) {
            return fail("a removed node leaves no render actor (incremental sweep)");
        }
    }

    return 0;
}
