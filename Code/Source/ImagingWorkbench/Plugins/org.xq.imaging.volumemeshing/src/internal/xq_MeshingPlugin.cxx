#include "xq_MeshingPlugin.h"
#include "xq_GridGenerationView.h"
#include "xq_MeshPreferencePage.h"
#include "xq_MeshCreateAction.h"

#include <berryIApplication.h>

void xq_GridingPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_GridGenerationView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_GridPreferencePage, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_MeshCreateAction, context)
}

void xq_GridingPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
