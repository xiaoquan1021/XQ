#include "xq_ApplicationContext.h"

#include "xq_ProjectService.h"

#include <mitkStandaloneDataStorage.h>

namespace xq::core
{

ApplicationContext::ApplicationContext(mitk::DataStorage::Pointer dataStorage,
                                       QObject* parent)
    : QObject(parent)
    , m_DataStorage(dataStorage)
    , m_ProjectService(new ProjectService(this))
{
}

ApplicationContext* ApplicationContext::CreateDefault(QObject* parent)
{
    return new ApplicationContext(mitk::StandaloneDataStorage::New(), parent);
}

mitk::DataStorage::Pointer ApplicationContext::DataStorage() const
{
    return m_DataStorage;
}

mitk::DataNode::Pointer ApplicationContext::ActiveNode() const
{
    return m_ActiveNode;
}

QStringList ApplicationContext::Diagnostics() const
{
    return m_Diagnostics;
}

ProjectService* ApplicationContext::Projects() const
{
    return m_ProjectService;
}

void ApplicationContext::SetActiveNode(mitk::DataNode::Pointer node)
{
    if (m_ActiveNode.GetPointer() == node.GetPointer())
        return;

    m_ActiveNode = node;
    emit ActiveNodeChanged();
    emit SelectionChanged(m_ActiveNode);
}

void ApplicationContext::ClearActiveNode()
{
    SetActiveNode(nullptr);
}

void ApplicationContext::PostDiagnostic(const QString& message)
{
    if (message.trimmed().isEmpty())
        return;

    m_Diagnostics.append(message);
    emit DiagnosticPosted(message);
}

} // namespace xq::core
