#ifndef XQ_WORKFLOWCONTEXTSERVICE_H
#define XQ_WORKFLOWCONTEXTSERVICE_H

#include "xq_DataCatalogService.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace xq::core
{

class DataSelectionService;
class WorkflowSelectionService;

struct WorkflowContextSnapshot
{
    QString WorkflowId;
    QString WorkflowTitle;
    bool RequiresSelectedData = false;
    bool HasSelectedData = false;
    bool HasCompatibleSelection = true;
    QString SelectedCatalogEntryId;
    QString SelectedDataDisplayName;
    DataWorkflowRole SelectedDataRole = DataWorkflowRole::Unknown;
};

bool operator==(const WorkflowContextSnapshot& lhs,
                const WorkflowContextSnapshot& rhs);
bool operator!=(const WorkflowContextSnapshot& lhs,
                const WorkflowContextSnapshot& rhs);

class WorkflowContextService : public QObject
{
    Q_OBJECT

public:
    WorkflowContextService(WorkflowSelectionService& workflowSelection,
                           DataSelectionService& dataSelection,
                           DataCatalogService& dataCatalog,
                           QObject* parent = nullptr);

    WorkflowContextSnapshot Snapshot() const;

    static QVector<DataWorkflowRole> AcceptedDataRolesForWorkflow(
        const QString& workflowId);

signals:
    void ContextChanged();

private:
    WorkflowContextSnapshot BuildSnapshot() const;
    void Refresh();

    WorkflowSelectionService& m_WorkflowSelection;
    DataSelectionService& m_DataSelection;
    DataCatalogService& m_DataCatalog;
    WorkflowContextSnapshot m_Snapshot;
};

} // namespace xq::core

#endif // XQ_WORKFLOWCONTEXTSERVICE_H
