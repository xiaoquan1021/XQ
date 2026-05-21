#include "xq_PythonDataNodesPluginActivator.h"

ctkPluginContext* xq_PythonDataNodesPluginActivator::m_Context = nullptr;
xq_PythonDataNodesPluginActivator* xq_PythonDataNodesPluginActivator::m_Inst = nullptr;

xq_PythonDataNodesPluginActivator::xq_PythonDataNodesPluginActivator()
{
  m_Inst = this;
}

xq_PythonDataNodesPluginActivator::~xq_PythonDataNodesPluginActivator()
{
}

void xq_PythonDataNodesPluginActivator::start(ctkPluginContext* context)
{
  berry::AbstractUICTKPlugin::start(context);
  m_Context = context;
}

void xq_PythonDataNodesPluginActivator::stop(ctkPluginContext* context)
{
  m_Context = nullptr;
  berry::AbstractUICTKPlugin::stop(context);
}

xq_PythonDataNodesPluginActivator* xq_PythonDataNodesPluginActivator::GetDefault()
{
  return m_Inst;
}

ctkPluginContext* xq_PythonDataNodesPluginActivator::GetContext()
{
  return m_Context;
}
