#ifndef XQ_DATANODEREGISTRYSERVICE_H
#define XQ_DATANODEREGISTRYSERVICE_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

#include <mitkDataNode.h>

namespace xq::core
{

class DataNodeRegistryService : public QObject
{
    Q_OBJECT

public:
    explicit DataNodeRegistryService(QObject* parent = nullptr);

    QStringList CatalogEntryIds() const;
    mitk::DataNode::Pointer FindNode(const QString& catalogEntryId) const;

    bool BindNode(const QString& catalogEntryId,
                  mitk::DataNode::Pointer node,
                  QString* errorMessage = nullptr);
    bool RemoveNode(const QString& catalogEntryId,
                    QString* errorMessage = nullptr);
    void Clear();

signals:
    void BindingsChanged();

private:
    static QString NormalizedId(const QString& catalogEntryId);
    static void SetError(QString* errorMessage, const QString& message);

    QStringList m_CatalogEntryIds;
    QHash<QString, mitk::DataNode::Pointer> m_NodesByCatalogEntryId;
};

} // namespace xq::core

#endif // XQ_DATANODEREGISTRYSERVICE_H
