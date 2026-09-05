#ifndef XQ_UI_XQ_SCENE_MODEL_H
#define XQ_UI_XQ_SCENE_MODEL_H

#include "core/NodeId.h"

#include <QAbstractItemModel>

#include <vector>

namespace xq {

class XQDataNode;
class XQScene;

class XQSceneModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        DisplayNameColumn = 0,
        DomainTypeColumn = 1,
        StaleColumn = 2,
        ColumnCount = 3
    };

    explicit XQSceneModel(QObject* parent = nullptr);
    explicit XQSceneModel(const XQScene* scene, QObject* parent = nullptr);

    const XQScene* scene() const;
    void setScene(const XQScene* scene);
    void refresh();

    QModelIndex index(int row,
                      int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    const XQDataNode* node_for_index(const QModelIndex& index) const;
    void rebuild_index();

    const XQScene* scene_;
    std::vector<NodeId> node_ids_;
};

} // namespace xq

#endif // XQ_UI_XQ_SCENE_MODEL_H
