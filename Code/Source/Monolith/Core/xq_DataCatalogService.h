#ifndef XQ_DATACATALOGSERVICE_H
#define XQ_DATACATALOGSERVICE_H

#include <QObject>
#include <QString>
#include <QVector>

namespace xq::core
{

enum class DataWorkflowRole
{
    Unknown,
    DICOMSeries,
    Image,
    Path,
    Segmentation,
    Model,
    Mesh,
    SimulationPrep,
    SimulationResult,
    ROMSimulation,
    MultiPhysics
};

struct DataCatalogEntry
{
    QString Id;
    QString DisplayName;
    QString SourcePath;
    QString Modality;
    DataWorkflowRole WorkflowRole = DataWorkflowRole::Unknown;
};

class DataCatalogService : public QObject
{
    Q_OBJECT

public:
    explicit DataCatalogService(QObject* parent = nullptr);

    QVector<DataCatalogEntry> Entries() const;
    const DataCatalogEntry* FindById(const QString& id) const;

    void ReplaceWith(const DataCatalogService& other);
    bool RegisterEntry(const DataCatalogEntry& entry,
                       QString* errorMessage = nullptr);
    bool RenameEntry(const QString& id,
                     const QString& displayName,
                     QString* errorMessage = nullptr);
    bool RemoveEntry(const QString& id,
                     QString* errorMessage = nullptr);

signals:
    void EntriesChanged();

private:
    static QString NormalizedId(const QString& id);
    static void SetError(QString* errorMessage, const QString& message);

    QVector<DataCatalogEntry> m_Entries;
};

} // namespace xq::core

#endif // XQ_DATACATALOGSERVICE_H
