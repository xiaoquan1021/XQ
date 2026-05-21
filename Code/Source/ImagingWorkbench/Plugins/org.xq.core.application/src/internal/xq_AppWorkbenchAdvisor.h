#ifndef XQ_APP_WORKBENCH_ADVISOR_H
#define XQ_APP_WORKBENCH_ADVISOR_H

#include <berryQtWorkbenchAdvisor.h>
#include <berryIQtStyleManager.h>
#include <QString>

/// Workbench lifecycle advisor for XQ.
///
/// Differs from conventional berry advisors by separating theme resolution
/// from theme application: ResolveStyleService() returns a validated handle
/// that ConfigureAppearance() consumes, keeping each step independently
/// testable.  Display metrics are gathered once and cached for the session.
class xq_AppWorkbenchAdvisor : public berry::QtWorkbenchAdvisor
{
public:
    void Initialize(berry::IWorkbenchConfigurer::Pointer configurer) override;

    berry::WorkbenchWindowAdvisor* CreateWorkbenchWindowAdvisor(
        berry::IWorkbenchWindowConfigurer::Pointer configurer) override;

    QString GetInitialWindowPerspectiveId() override;

private:
    /// Two-phase theme setup: resolve, then configure.
    berry::IQtStyleManager* ResolveStyleService() const;
    void ConfigureAppearance(berry::IQtStyleManager* styleMgr);
    bool ShouldRestoreWorkbench() const;
    bool BackupUnsafeRestoredPipelineViewState() const;

    /// Gather display DPI/geometry and size the initial window.
    void AdaptToDisplayMetrics(
        berry::IWorkbenchWindowConfigurer::Pointer winCfg);

    /// Verify that required plugin dependencies are loaded.
    bool ValidatePluginDependencies() const;

    static constexpr const char* kPerspectiveId =
        "org.xq.application.defaultperspective";
    static constexpr const char* kThemeResource = ":/xq/xq.qss";
    static constexpr const char* kThemeLabel    = "XQ Arctic Light";
    static constexpr const char* kAppIcon       = ":/xq/icon.png";

    bool m_ThemeApplied = false;
};

#endif // XQ_APP_WORKBENCH_ADVISOR_H
