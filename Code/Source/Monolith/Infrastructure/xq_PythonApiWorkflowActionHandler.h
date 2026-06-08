#ifndef XQ_PYTHONAPIWORKFLOWACTIONHANDLER_H
#define XQ_PYTHONAPIWORKFLOWACTIONHANDLER_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
}

namespace xq::infrastructure
{

bool RegisterDynamicPythonApiWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    QString* message = nullptr);

} // namespace xq::infrastructure

#endif // XQ_PYTHONAPIWORKFLOWACTIONHANDLER_H
