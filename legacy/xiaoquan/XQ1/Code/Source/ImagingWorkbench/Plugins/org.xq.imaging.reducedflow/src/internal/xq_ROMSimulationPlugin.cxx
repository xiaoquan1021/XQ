#include "xq_ROMSimulationPlugin.h"
#include "xq_ROMJobCreateAction.h"
#include "xq_ROMSimulationView.h"

xq_ROMSimulationPlugin::xq_ROMSimulationPlugin() = default;
xq_ROMSimulationPlugin::~xq_ROMSimulationPlugin() = default;

void xq_ROMSimulationPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_ROMSimulationView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_ROMJobCreateAction, context)
}

void xq_ROMSimulationPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
