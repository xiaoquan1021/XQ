#include <app/XQMainWindow.h>

#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQScene.h>
#include <core/command/XQCommandStack.h>
#include <visualization/XQMprWidget.h>
#include <visualization/XQVolumeViewWidget.h>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include <cstdio>
#include <map>

#ifndef XQ_TEST_VTI_PATH
#error "XQ_TEST_VTI_PATH must be defined"
#endif

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

std::size_t count_domain(const xq::XQScene& scene, xq::XQDomainType domain)
{
    std::size_t count = 0;
    scene.visit_nodes([&count, domain](const xq::XQDataNode& node) {
        if (node.domainType() == domain) {
            ++count;
        }
    });
    return count;
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();
    QApplication app(argc, argv);

    xq::XQScene scene;
    xq::XQCommandStack stack;
    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    window.attachWorkflow(&scene, &stack);
    window.resize(1440, 920);
    window.show();
    QApplication::processEvents();

    // B6b: step-by-step guidance on the Path page. Before any image is open the
    // hint must point at step 1 (open an image). Snapshot each transition so the
    // later checks assert the text actually CHANGED, not a fixed string.
    QLabel* pathHint = window.findChild<QLabel*>("xqStageHint_Path");
    CHECK(pathHint != nullptr);
    const QString hintNoImage = pathHint->text();
    CHECK(hintNoImage.contains(QStringLiteral("Step 1")));

    // Real image: gives the draft picks a source image node + voxel frame.
    // B4b: image decoding is now queued on the background parse runner, which
    // engages the busy matrix (stage panel disabled) until the decode commits.
    // Pump the loop until the image lands (axial slice spin enabled) so the later
    // runButton-enabled assertion is not tripped by a transient busy state.
    const bool imageLoaded =
        window.loadImageFromPath(QString::fromUtf8(XQ_TEST_VTI_PATH));
    CHECK(imageLoaded);
    {
        QSpinBox* axialSpin = window.findChild<QSpinBox*>("xqNavAxialSpin");
        CHECK(axialSpin != nullptr);
        QElapsedTimer imageTimer;
        imageTimer.start();
        while (!axialSpin->isEnabled() && imageTimer.elapsed() < 10000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        CHECK(axialSpin->isEnabled());
    }

    QPushButton* runButton = window.findChild<QPushButton*>("xqPathRunButton");
    QListWidget* pointList = window.findChild<QListWidget*>("xqPathPointList");
    QDoubleSpinBox* spacing = window.findChild<QDoubleSpinBox*>("xqPathSpacingSpin");
    QLineEdit* nameEdit = window.findChild<QLineEdit*>("xqPathNameEdit");
    CHECK(runButton != nullptr);
    CHECK(pointList != nullptr);
    CHECK(spacing != nullptr);
    CHECK(nameEdit != nullptr);
    CHECK(!runButton->isEnabled()); // no draft yet

    // Guidance advanced past step 1 once the image is loaded (useVolume nudges
    // the Path page). Assert it CHANGED and now shows step 2.
    const QString hintWithImage = pathHint->text();
    CHECK(hintWithImage != hintNoImage);
    CHECK(hintWithImage.contains(QStringLiteral("Step 2")));

    // Three picks through the test hook (same path as an MPR click).
    window.appendPathDraftPointForTest({0.0, 0.0, 0.0});
    window.appendPathDraftPointForTest({10.0, 0.0, 0.0});
    QApplication::processEvents();
    CHECK(pointList->count() == 2);
    CHECK(runButton->isEnabled()); // >= 2 points gate

    // With >= 2 points the guidance moves to step 3 (name + generate). Assert it
    // changed off the step-2 text.
    const QString hintTwoPoints = pathHint->text();
    CHECK(hintTwoPoints != hintWithImage);
    CHECK(hintTwoPoints.contains(QStringLiteral("Step 3")));

    window.appendPathDraftPointForTest({10.0, 10.0, 0.0});
    QApplication::processEvents();
    CHECK(pointList->count() == 3);

    spacing->setValue(0.5);
    nameEdit->setText(QStringLiteral("centerline-test"));

    const std::size_t pathsBefore = count_domain(scene, xq::XQDomainType::Path);
    runButton->click();

    // The run goes through the async runner; pump until the Path node lands.
    QElapsedTimer timer;
    timer.start();
    while (count_domain(scene, xq::XQDomainType::Path) == pathsBefore
           && timer.elapsed() < 30000) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore + 1);

    // Draft cleared + list emptied + run disabled again.
    QApplication::processEvents();
    CHECK(pointList->count() == 0);
    CHECK(!runButton->isEnabled());

    // undo/redo symmetry through the window path.
    window.undo();
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore);
    window.redo();
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore + 1);

    // Rejected input surfaces a message, adds nothing: spacing 0 is clamped by
    // the spin box (min 0.01), so drive rejection with a 1-point draft instead.
    window.appendPathDraftPointForTest({0.0, 0.0, 1.0});
    QApplication::processEvents();
    CHECK(!runButton->isEnabled()); // 1 point: gate holds, honest disable

    // --- B6b: picking-mode overlay + Ctrl+A + Esc ---------------------------
    QPushButton* pickToggle = window.findChild<QPushButton*>("xqPathPickButton");
    QLabel* modeHint = window.findChild<QLabel*>("xqSliceModeHint");
    CHECK(pickToggle != nullptr);
    CHECK(modeHint != nullptr);

    // Enter path picking: the slice mode-hint overlay shows with non-empty text.
    CHECK(modeHint->isHidden()); // hidden before picking starts
    pickToggle->setChecked(true);
    QApplication::processEvents();
    CHECK(pickToggle->isChecked());
    CHECK(!modeHint->isHidden());
    CHECK(!modeHint->text().isEmpty());

    // Ctrl+A path: adds a draft point at the current crosshair voxel. Record the
    // count first, then assert it grew by exactly one (run the same code the
    // shortcut runs; offscreen ctest cannot route a window shortcut reliably).
    const int draftBeforeCtrlA = pointList->count();
    const std::size_t afterCtrlA = window.addPathDraftAtCrosshairForTest();
    QApplication::processEvents();
    CHECK(static_cast<int>(afterCtrlA) == draftBeforeCtrlA + 1);
    CHECK(pointList->count() == draftBeforeCtrlA + 1);

    // Esc leaves picking: the toggle resets and the overlay hides (pickMode back
    // to None). Snapshot the checked state first so the reset is falsifiable.
    CHECK(pickToggle->isChecked());
    window.exitPickingForTest();
    QApplication::processEvents();
    CHECK(!pickToggle->isChecked());
    CHECK(modeHint->isHidden());

    std::printf("OK: path stage end-to-end\n");
    return 0;
}
