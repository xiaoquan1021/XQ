#ifndef XQ_DATASELECTIONSERVICE_H
#define XQ_DATASELECTIONSERVICE_H

#include <QObject>
#include <QString>

namespace xq::core
{

class DataCatalogService;
class DataHierarchyService;

class DataSelectionService : public QObject
{
    Q_OBJECT

public:
    DataSelectionService(DataCatalogService& dataCatalog,
                         DataHierarchyService& dataHierarchy,
                         QObject* parent = nullptr);

    bool HasSelection() const;
    QString SelectedHierarchyNodeId() const;
    QString SelectedCatalogEntryId() const;

    bool SelectHierarchyNode(const QString& hierarchyNodeId,
                             QString* errorMessage = nullptr);
    bool SelectCatalogEntry(const QString& catalogEntryId,
                            QString* errorMessage = nullptr);
    void Clear();

signals:
    void SelectionChanged(const QString& hierarchyNodeId,
                          const QString& catalogEntryId);

private:
    bool SetSelection(const QString& hierarchyNodeId,
                      const QString& catalogEntryId);
    static void SetError(QString* errorMessage, const QString& message);

    DataCatalogService& m_DataCatalog;
    DataHierarchyService& m_DataHierarchy;
    QString m_SelectedHierarchyNodeId;
    QString m_SelectedCatalogEntryId;
};

} // namespace xq::core

#endif // XQ_DATASELECTIONSERVICE_H
