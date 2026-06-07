#ifndef XQ_APPLICATIONCONTEXT_H
#define XQ_APPLICATIONCONTEXT_H

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QObject>
#include <QString>
#include <QStringList>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;
class DataImportService;
class DataManagementService;
class DataSelectionService;
class PreferencesService;
class ProjectService;
class ProjectSessionService;
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
    DataCatalogService* DataCatalog() const;
    DataHierarchyService* DataHierarchy() const;
    DataImportService* DataImports() const;
    DataManagementService* DataManagement() const;
    DataSelectionService* DataSelection() const;
    QStringList Diagnostics() const;
    PreferencesService* Preferences() const;
    ProjectService* Projects() const;
    ProjectSessionService* ProjectSession() const;
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
    DataCatalogService* m_DataCatalogService = nullptr;
    DataHierarchyService* m_DataHierarchyService = nullptr;
    DataSelectionService* m_DataSelectionService = nullptr;
    QStringList m_Diagnostics;
    PreferencesService* m_PreferencesService = nullptr;
    ProjectService* m_ProjectService = nullptr;
    TaskRunner* m_TaskRunner = nullptr;
    DataManagementService* m_DataManagementService = nullptr;
    DataImportService* m_DataImportService = nullptr;
    ProjectSessionService* m_ProjectSessionService = nullptr;
};

} // namespace xq::core

#endif // XQ_APPLICATIONCONTEXT_H
