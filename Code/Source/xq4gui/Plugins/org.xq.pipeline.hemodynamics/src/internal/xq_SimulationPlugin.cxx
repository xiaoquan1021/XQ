#include "xq_SimulationPlugin.h"
#include "xq_HemodynamicsView.h"
#include "xq_SimulationPreferencePage.h"
#include "xq_SimJobCreateAction.h"
#include "xq_SolverExportAction.h"
#include "xq_ResultImportAction.h"

#include <berryIApplication.h>

void xq_SimulationPlugin::start(ctkPluginContext* context)
{
  BERRY_REGISTER_EXTENSION_CLASS(xq_HemodynamicsView, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_SimulationPreferencePage, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_SimJobCreateAction, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_SolverExportAction, context)
  BERRY_REGISTER_EXTENSION_CLASS(xq_ResultImportAction, context)
}

void xq_SimulationPlugin::stop(ctkPluginContext* context)
{
  Q_UNUSED(context)
}
