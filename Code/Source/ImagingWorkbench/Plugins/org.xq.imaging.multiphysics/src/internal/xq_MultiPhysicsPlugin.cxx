#include "xq_MultiPhysicsPlugin.h"
#include "xq_MultiPhysicsJobCreateAction.h"
#include "xq_MultiPhysicsView.h"

xq_MultiPhysicsPlugin::xq_MultiPhysicsPlugin() = default;
xq_MultiPhysicsPlugin::~xq_MultiPhysicsPlugin() = default;

void xq_MultiPhysicsPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_MultiPhysicsView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_MultiPhysicsJobCreateAction, context)
}

void xq_MultiPhysicsPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
