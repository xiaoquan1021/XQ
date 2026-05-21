#include "xq_DefaultPerspective.h"
#include <berryIViewLayout.h>
#include <berryIPlaceholderFolderLayout.h>

// ---------------------------------------------------------------------------
// Layout geometry — expressed as data so the wiring logic stays generic.
// ---------------------------------------------------------------------------

namespace
{

// View identifiers centralised in one place.
constexpr const char* kDataManager      = "org.xq.views.datamanager";
constexpr const char* kImageNavigator   = "org.mitk.views.imagenavigator";
constexpr const char* kLogConsole       = "org.blueberry.views.logview";

// Deferred tool-view identifiers (shown on demand). The XQ Project Manager
// view is intentionally registered here as a placeholder only: it is no
// longer shown in the default layout, but still reachable from
// Window → Show View when needed.
constexpr const char* kDeferredViews[] = {
    "org.xq.views.projectmanager",
    "org.xq.views.imageprocessing",
    "org.xq.views.pathplanning",
    "org.xq.views.segmentation",
    "org.xq.views.mitksegmentation",
    "org.xq.views.modeling",
    "org.xq.views.meshing",
    "org.xq.views.simulation",
    "org.xq.views.romsimulation",
    "org.xq.views.multiphysics",
};

// Left-column layout expressed declaratively.
// Sidebar takes 76px on the left (handled by QDockWidget, not here).
// These ratios apply to the remaining space after the sidebar.
//
// The Logging view is intentionally NOT part of the default layout — it is
// opened on demand via the "Logging" button on the ribbon and docks to the
// right of the editor area (see kLogSidebarPlaceholderId below).
const XqPanelRegion kRegions[] = {
    // ── left column (20 % of total width) ──
    // Sidebar wide enough to show Image Navigator sliders without scrollbar.
    { kDataManager,    berry::IPageLayout::LEFT,   0.20f, nullptr,         false },
    // Image Navigator sits directly beneath Data Manager (50/50 split).
    { kImageNavigator, berry::IPageLayout::BOTTOM, 0.50f, kDataManager,    true  },
};
const std::size_t kRegionCount = sizeof(kRegions) / sizeof(kRegions[0]);

// Placeholder folder ID for the on-demand logging dock on the right side
// of the editor area. Matching in xq_WorkbenchWindowAdvisor's toggle code.
constexpr const char* kLogSidebarFolderId = "xq.rightside.log";

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

xq_DefaultPerspective::xq_DefaultPerspective() = default;

// ---------------------------------------------------------------------------
// berry::IPerspectiveFactory
// ---------------------------------------------------------------------------

void xq_DefaultPerspective::CreateInitialLayout(berry::IPageLayout::Pointer layout)
{
    const QString editorArea = layout->GetEditorArea();

    ApplyRegions(layout, kRegions, kRegionCount, editorArea);
    RegisterToolSlots(layout, editorArea);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void xq_DefaultPerspective::ApplyRegions(berry::IPageLayout::Pointer layout,
                                          const XqPanelRegion* regions,
                                          std::size_t count,
                                          const QString& editorArea)
{
    for (std::size_t idx = 0; idx < count; ++idx)
    {
        const auto& r = regions[idx];
        const QString anchor = r.refId ? QString(r.refId) : editorArea;

        layout->AddView(r.viewId, r.placement, r.ratio, anchor);

        berry::IViewLayout::Pointer vl = layout->GetViewLayout(r.viewId);
        if (vl)
        {
            vl->SetCloseable(r.closeable);
        }
    }
}

void xq_DefaultPerspective::RegisterToolSlots(berry::IPageLayout::Pointer layout,
                                               const QString& editorArea)
{
    // Right-side panel — hosts both pipeline tool views and the log
    // console as peer tabs.  Editor keeps the left 45 %; the right panel
    // gets the remaining 55 %, wide enough to show tool controls without
    // horizontal scrollbars.
    berry::IPlaceholderFolderLayout::Pointer rightDock =
        layout->CreatePlaceholderFolder(kLogSidebarFolderId,
                                        berry::IPageLayout::RIGHT,
                                        0.55f, editorArea);

    for (const char* id : kDeferredViews)
    {
        rightDock->AddPlaceholder(id);
    }

    rightDock->AddPlaceholder(kLogConsole);
}
