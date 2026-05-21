#include "xq_ModelingPlugin.h"
#include "xq_VascularModelingView.h"
#include "xq_ModelPreferencePage.h"
#include "xq_ModelCreateAction.h"

#include <berryIApplication.h>

void xq_ModelingPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_VascularModelingView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_ModelPreferencePage, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_ModelCreateAction, context)
}

void xq_ModelingPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
