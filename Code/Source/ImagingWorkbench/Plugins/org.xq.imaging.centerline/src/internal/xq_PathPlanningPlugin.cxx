#include "xq_PathPlanningPlugin.h"
#include "xq_VesselPlanningView.h"
#include "xq_PathPreferencePage.h"
#include "xq_PathCreateAction.h"

#include <berryIQtStyleManager.h>

xq_PathPlanningPlugin::xq_PathPlanningPlugin()
{
}

xq_PathPlanningPlugin::~xq_PathPlanningPlugin()
{
}

void xq_PathPlanningPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_VesselPlanningView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_PathPreferencePage, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_PathCreateAction, context)
}

void xq_PathPlanningPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
