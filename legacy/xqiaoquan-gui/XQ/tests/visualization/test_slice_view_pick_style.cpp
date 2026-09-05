// Pick-interaction takeover invariant for XQSliceViewWidget (defect A). Entering
// seed/path picking must engage the empty interactor style so a click has one
// deterministic path (killing the native-GL press-order race that made seed
// markers appear only sometimes); leaving restores the window/level style.
//
// Headless (offscreen QPA, no GL context / no interactor), pickStyleActive()
// follows the pick-mode intent flag, so the takeover invariant is still testable
// under ctest. IRON RULE (memory: no-sideeffect-in-assert): side-effecting calls
// run first, their results stored, THEN asserted -- a /DNDEBUG build cannot delete
// them into a false green.

#include <visualization/XQRenderScene.h>
#include <visualization/XQSliceViewWidget.h>

#include <QApplication>

#include <cstdio>
#include <memory>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

// The three slice axes (XQRenderScene mapping): 0 = Sagittal, 1 = Coronal,
// 2 = Axial. The takeover is per-view, so every axis is exercised.
void test_pick_style_takeover(int axis)
{
    xq::XQRenderScene scene;
    xq::XQSliceViewWidget view(&scene, axis);

    const bool initial = view.pickStyleActive();
    check(!initial, "A: fresh view -> pick style not engaged");

    view.setSeedPickingEnabled(true);
    const bool engaged = view.pickStyleActive();
    check(engaged, "A: setSeedPickingEnabled(true) -> pick style engaged");
    check(view.seedPickingEnabled(), "A: seedPickingEnabled() true while engaged");

    view.setSeedPickingEnabled(false);
    const bool restored = view.pickStyleActive();
    check(!restored, "A: setSeedPickingEnabled(false) -> pick style released");
    check(!view.seedPickingEnabled(), "A: seedPickingEnabled() false after release");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    test_pick_style_takeover(0);
    test_pick_style_takeover(1);
    test_pick_style_takeover(2);

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all slice-view pick-style checks passed\n");
    return 0;
}
