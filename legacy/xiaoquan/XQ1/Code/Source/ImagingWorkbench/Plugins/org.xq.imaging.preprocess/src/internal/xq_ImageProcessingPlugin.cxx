#include "xq_ImageProcessingPlugin.h"
#include "xq_ImageProcessingView.h"

xq_ImageProcessingPlugin::xq_ImageProcessingPlugin() = default;
xq_ImageProcessingPlugin::~xq_ImageProcessingPlugin() = default;

void xq_ImageProcessingPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_ImageProcessingView, context)
}

void xq_ImageProcessingPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
