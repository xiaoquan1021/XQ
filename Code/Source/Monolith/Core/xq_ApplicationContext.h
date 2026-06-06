#ifndef XQ_APPLICATIONCONTEXT_H
#define XQ_APPLICATIONCONTEXT_H

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QObject>
#include <QString>

namespace xq::core
{

class ApplicationContext : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationContext(mitk::DataStorage::Pointer dataStorage,
                                QObject* parent = nullptr);

    static ApplicationContext* CreateDefault(QObject* parent = nullptr);

    mitk::DataStorage::Pointer DataStorage() const;
    mitk::DataNode::Pointer ActiveNode() const;

public slots:
    void SetActiveNode(mitk::DataNode::Pointer node);
    void PostDiagnostic(const QString& message);

signals:
    void ActiveNodeChanged();
    void DiagnosticPosted(const QString& message);

private:
    mitk::DataStorage::Pointer m_DataStorage;
    mitk::DataNode::Pointer m_ActiveNode;
};

} // namespace xq::core

#endif // XQ_APPLICATIONCONTEXT_H
