#include "xq_SegmentationPlugin.h"
#include "xq_LumenContouringView.h"
#include "xq_SegmentationPreferencePage.h"
#include "xq_ContourGroupCreateAction.h"

#include <berryIApplication.h>

void xq_SegmentationPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_LumenContouringView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_SegmentationPreferencePage, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_ContourGroupCreateAction, context)
}

void xq_SegmentationPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
