#include "xq_DataExplorerPluginActivator.h"
#include "xq_DataExplorerView.h"

#include <berryIApplication.h>

void xq_DataExplorerPluginActivator::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_DataExplorerView, context)
}

void xq_DataExplorerPluginActivator::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
