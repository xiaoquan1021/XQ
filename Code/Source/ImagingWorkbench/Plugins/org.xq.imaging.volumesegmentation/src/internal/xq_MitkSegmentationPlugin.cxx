#include "xq_MitkSegmentationPlugin.h"
#include "xq_MitkSegmentationView.h"

void xq_MitkSegmentationPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_MitkSegmentationView, context)
}

void xq_MitkSegmentationPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
