#ifndef XQ_DATAHIERARCHYMODEL_H
#define XQ_DATAHIERARCHYMODEL_H

#include <QAbstractItemModel>

#include <memory>
#include <vector>

namespace xq::core
{
class DataHierarchyService;
}

namespace xq::presentation
{

class DataHierarchyModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles
    {
        NodeIdRole = Qt::UserRole + 1
    };

    explicit DataHierarchyModel(xq::core::DataHierarchyService& hierarchy,
                                QObject* parent = nullptr);
    ~DataHierarchyModel() override;

    QModelIndex index(int row,
                      int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex IndexForNodeId(const QString& nodeId) const;

private:
    struct ModelNode;

    void Rebuild();
    ModelNode* NodeForIndex(const QModelIndex& index) const;
    QModelIndex IndexForNodeId(const ModelNode* parentNode,
                               const QString& nodeId) const;
    int RowOfNode(const ModelNode* node) const;

    xq::core::DataHierarchyService& m_Hierarchy;
    std::vector<std::unique_ptr<ModelNode>> m_Nodes;
    ModelNode* m_Root = nullptr;
};

} // namespace xq::presentation

#endif // XQ_DATAHIERARCHYMODEL_H
