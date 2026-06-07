#ifndef XQ_DATAHIERARCHYSERVICE_H
#define XQ_DATAHIERARCHYSERVICE_H

#include <QObject>
#include <QString>
#include <QVector>

namespace xq::core
{

enum class DataHierarchyNodeKind
{
    Folder,
    DataEntry
};

struct DataHierarchyNode
{
    QString Id;
    QString ParentId;
    QString DisplayName;
    QString DataCatalogEntryId;
    DataHierarchyNodeKind Kind = DataHierarchyNodeKind::Folder;
};

class DataHierarchyService : public QObject
{
    Q_OBJECT

public:
    explicit DataHierarchyService(QObject* parent = nullptr);

    QString RootId() const;
    QVector<DataHierarchyNode> Nodes() const;
    const DataHierarchyNode* FindNode(const QString& id) const;
    QVector<DataHierarchyNode> ChildrenOf(const QString& parentId) const;

    void ReplaceWith(const DataHierarchyService& other);
    bool AddFolder(const QString& id,
                   const QString& parentId,
                   const QString& displayName,
                   QString* errorMessage = nullptr);
    bool AddDataEntry(const QString& id,
                      const QString& parentId,
                      const QString& dataCatalogEntryId,
                      const QString& displayName,
                      QString* errorMessage = nullptr);
    bool RenameDataEntriesForCatalogEntry(const QString& dataCatalogEntryId,
                                          const QString& displayName,
                                          QString* errorMessage = nullptr);
    bool RemoveDataEntriesForCatalogEntry(const QString& dataCatalogEntryId,
                                          QString* errorMessage = nullptr);

private:
    bool AddNode(DataHierarchyNode node, QString* errorMessage);
    static QString NormalizedId(const QString& id);
    static void SetError(QString* errorMessage, const QString& message);

    QVector<DataHierarchyNode> m_Nodes;
};

} // namespace xq::core

#endif // XQ_DATAHIERARCHYSERVICE_H
