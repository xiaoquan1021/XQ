#include "xq_Application.h"

#include <berryPlatformUI.h>
#include "xq_AppWorkbenchAdvisor.h"

xq_Application::xq_Application()
{
}

QVariant xq_Application::Start(berry::IApplicationContext* /*context*/)
{
    QScopedPointer<berry::Display> display(berry::PlatformUI::CreateDisplay());
    QScopedPointer<xq_AppWorkbenchAdvisor> wbAdvisor(new xq_AppWorkbenchAdvisor());

    int code = berry::PlatformUI::CreateAndRunWorkbench(display.data(), wbAdvisor.data());

    return code == berry::PlatformUI::RETURN_RESTART
               ? EXIT_RESTART
               : EXIT_OK;
}

void xq_Application::Stop()
{
}
