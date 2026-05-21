#include "xq_WorkspacePluginActivator.h"
#include "xq_WorkspaceExplorer.h"
#include "xq_AddImageAction.h"
#include "xq_PipelineCheckAction.h"

xq_WorkspacePluginActivator::xq_WorkspacePluginActivator()
{
}

xq_WorkspacePluginActivator::~xq_WorkspacePluginActivator()
{
}

void xq_WorkspacePluginActivator::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_WorkspaceExplorer, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_AddImageAction, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_PipelineCheckAction, context)
}

void xq_WorkspacePluginActivator::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
