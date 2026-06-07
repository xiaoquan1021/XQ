#ifndef XQ_APPLICATIONCONTEXT_H
#define XQ_APPLICATIONCONTEXT_H

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QObject>
#include <QString>
#include <QStringList>

namespace xq::core
{

class PreferencesService;
class ProjectService;
class TaskRunner;

class ApplicationContext : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationContext(mitk::DataStorage::Pointer dataStorage,
                                QObject* parent = nullptr);

    static ApplicationContext* CreateDefault(QObject* parent = nullptr);

    mitk::DataStorage::Pointer DataStorage() const;
    mitk::DataNode::Pointer ActiveNode() const;
    QStringList Diagnostics() const;
    PreferencesService* Preferences() const;
    ProjectService* Projects() const;
    TaskRunner* Tasks() const;

public slots:
    void SetActiveNode(mitk::DataNode::Pointer node);
    void ClearActiveNode();
    void PostDiagnostic(const QString& message);

signals:
    void ActiveNodeChanged();
    void SelectionChanged(mitk::DataNode::Pointer node);
    void DiagnosticPosted(const QString& message);

private:
    mitk::DataStorage::Pointer m_DataStorage;
    mitk::DataNode::Pointer m_ActiveNode;
    QStringList m_Diagnostics;
    PreferencesService* m_PreferencesService = nullptr;
    ProjectService* m_ProjectService = nullptr;
    TaskRunner* m_TaskRunner = nullptr;
};

} // namespace xq::core

#endif // XQ_APPLICATIONCONTEXT_H
