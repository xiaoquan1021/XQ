#include "xq_AppWorkbenchAdvisor.h"
#include "xq_ApplicationPluginActivator.h"
#include "xq_WorkbenchWindowAdvisor.h"
#include "../../../../../Application/xq_StartupSafety.h"

#include <mitkCoreServices.h>
#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkLogMacros.h>

#include <QApplication>
#include <QScreen>

namespace
{

constexpr const char* kPreferencesNode = "/org.xq.preferences";
constexpr const char* kRestoreWorkspaceKey = "general.restoreWorkspace";

} // namespace

// ---------------------------------------------------------------------------
// Startup — two-phase theme resolution then plugin dependency check
// ---------------------------------------------------------------------------

void xq_AppWorkbenchAdvisor::Initialize(
    berry::IWorkbenchConfigurer::Pointer configurer)
{
    berry::QtWorkbenchAdvisor::Initialize(configurer);
    configurer->SetSaveAndRestore(ShouldRestoreWorkbench());

    ValidatePluginDependencies();

    if (auto* svc = ResolveStyleService())
        ConfigureAppearance(svc);
}

// ---------------------------------------------------------------------------
// Window advisor — display-adaptive geometry, icon assignment
// ---------------------------------------------------------------------------

berry::WorkbenchWindowAdvisor*
xq_AppWorkbenchAdvisor::CreateWorkbenchWindowAdvisor(
    berry::IWorkbenchWindowConfigurer::Pointer winCfg)
{
    AdaptToDisplayMetrics(winCfg);

    auto* advisor = new xq_WorkbenchWindowAdvisor(this, winCfg);
    advisor->SetWindowIcon(kAppIcon);
    return advisor;
}

// ---------------------------------------------------------------------------
// Perspective identifier
// ---------------------------------------------------------------------------

QString xq_AppWorkbenchAdvisor::GetInitialWindowPerspectiveId()
{
    return QLatin1String(kPerspectiveId);
}

// ---------------------------------------------------------------------------
// Style service resolution — returns nullptr on any failure
// ---------------------------------------------------------------------------

berry::IQtStyleManager*
xq_AppWorkbenchAdvisor::ResolveStyleService() const
{
    auto* ctx = xq_ApplicationPluginActivator::getContext();
    if (!ctx) return nullptr;

    auto ref = ctx->getServiceReference<berry::IQtStyleManager>();
    if (!ref) return nullptr;

    return ctx->getService<berry::IQtStyleManager>(ref);
}

// ---------------------------------------------------------------------------
// Appearance — register and activate the XQ stylesheet
// ---------------------------------------------------------------------------

void xq_AppWorkbenchAdvisor::ConfigureAppearance(
    berry::IQtStyleManager* styleMgr)
{
    if (!styleMgr || m_ThemeApplied) return;

    const QString resource = QLatin1String(kThemeResource);
    styleMgr->AddStyle(resource, QLatin1String(kThemeLabel));
    styleMgr->SetStyle(resource);
    m_ThemeApplied = true;
}

bool xq_AppWorkbenchAdvisor::ShouldRestoreWorkbench() const
{
    if (BackupUnsafeRestoredPipelineViewState())
    {
        MITK_WARN << "Disabling startup workbench restore because the persisted state restores a pipeline view before the workbench is fully ready. The unsafe workbench.xml has been backed up so later clean sessions can save a healthy state.";
        return false;
    }

    auto* prefsService = mitk::CoreServices::GetPreferencesService();
    if (!prefsService)
        return false;

    auto* prefs = prefsService->GetSystemPreferences()->Node(kPreferencesNode);
    if (!prefs)
        return false;

    if (!prefs->GetBool(kRestoreWorkspaceKey, false))
        return false;

    return true;
}

bool xq_AppWorkbenchAdvisor::BackupUnsafeRestoredPipelineViewState() const
{
    const bool backedUp = xq::startup::BackupUnsafeWorkbenchState();
    if (backedUp)
        MITK_WARN << "Backed up unsafe workbench restore state before deciding whether startup restore is allowed.";
    return backedUp;
}

// ---------------------------------------------------------------------------
// Display metrics — choose initial window fraction by pixel density
// ---------------------------------------------------------------------------

void xq_AppWorkbenchAdvisor::AdaptToDisplayMetrics(
    berry::IWorkbenchWindowConfigurer::Pointer winCfg)
{
    const QScreen* screen = QApplication::primaryScreen();
    if (!screen) return;

    const qreal   density   = screen->devicePixelRatio();
    const QRect   available = screen->availableGeometry();

    // HiDPI displays get a larger default window to compensate for scaling.
    constexpr double kHiDpiFraction   = 0.92;
    constexpr double kNormalFraction  = 0.90;
    constexpr qreal  kDensityThreshold = 1.5;
    const double frac = (density > kDensityThreshold) ? kHiDpiFraction
                                                      : kNormalFraction;

    winCfg->SetInitialSize(QPoint(
        static_cast<int>(available.width()  * frac),
        static_cast<int>(available.height() * frac)));
}

// ---------------------------------------------------------------------------
// Dependency validation — log missing services but continue gracefully
// ---------------------------------------------------------------------------

bool xq_AppWorkbenchAdvisor::ValidatePluginDependencies() const
{
    auto* ctx = xq_ApplicationPluginActivator::getContext();
    return ctx != nullptr;
}
