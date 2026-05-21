#include "xq_ApplicationPluginActivator.h"
#include "xq_Application.h"
#include "xq_DefaultPerspective.h"
#include "xq_ViewerPerspective.h"
#include "xq_VisualizationPerspective.h"
#include "xq_WelcomePart.h"
#include "xq_AboutDialog.h"

ctkPluginContext* xq_ApplicationPluginActivator::m_Context = nullptr;
xq_ApplicationPluginActivator* xq_ApplicationPluginActivator::m_Instance = nullptr;

xq_ApplicationPluginActivator::xq_ApplicationPluginActivator()
{
    m_Instance = this;
}

xq_ApplicationPluginActivator::~xq_ApplicationPluginActivator()
{
}

xq_ApplicationPluginActivator* xq_ApplicationPluginActivator::GetDefault()
{
    return m_Instance;
}

void xq_ApplicationPluginActivator::start(ctkPluginContext* context)
{
    berry::AbstractUICTKPlugin::start(context);
    m_Context = context;

    BERRY_REGISTER_EXTENSION_CLASS(xq_Application, context)
    BERRY_REGISTER_EXTENSION_CLASS(xq_DefaultPerspective, context)
    BERRY_REGISTER_EXTENSION_CLASS(xq_ViewerPerspective, context)
    BERRY_REGISTER_EXTENSION_CLASS(xq_VisualizationPerspective, context)
    BERRY_REGISTER_EXTENSION_CLASS(xq_WelcomePart, context)
    BERRY_REGISTER_EXTENSION_CLASS(xq_AboutDialog, context)
}

void xq_ApplicationPluginActivator::stop(ctkPluginContext* context)
{
    berry::AbstractUICTKPlugin::stop(context);
    m_Context = nullptr;
}

ctkPluginContext* xq_ApplicationPluginActivator::getContext()
{
    return m_Context;
}
