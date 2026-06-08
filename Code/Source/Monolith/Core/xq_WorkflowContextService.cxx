#include "xq_WorkflowContextService.h"

#include "xq_DataSelectionService.h"
#include "xq_WorkflowRegistry.h"
#include "xq_WorkflowSelectionService.h"

#include <algorithm>

namespace xq::core
{

namespace
{

bool ContainsRole(const QVector<DataWorkflowRole>& roles,
                  DataWorkflowRole role)
{
    return std::any_of(roles.begin(), roles.end(),
                       [role](DataWorkflowRole acceptedRole) {
                           return acceptedRole == role;
                       });
}

} // namespace

bool operator==(const WorkflowContextSnapshot& lhs,
                const WorkflowContextSnapshot& rhs)
{
    return lhs.WorkflowId == rhs.WorkflowId &&
           lhs.WorkflowTitle == rhs.WorkflowTitle &&
           lhs.RequiresSelectedData == rhs.RequiresSelectedData &&
           lhs.HasSelectedData == rhs.HasSelectedData &&
           lhs.HasCompatibleSelection == rhs.HasCompatibleSelection &&
           lhs.SelectedCatalogEntryId == rhs.SelectedCatalogEntryId &&
           lhs.SelectedDataDisplayName == rhs.SelectedDataDisplayName &&
           lhs.SelectedDataRole == rhs.SelectedDataRole;
}

bool operator!=(const WorkflowContextSnapshot& lhs,
                const WorkflowContextSnapshot& rhs)
{
    return !(lhs == rhs);
}

WorkflowContextService::WorkflowContextService(
    WorkflowSelectionService& workflowSelection,
    DataSelectionService& dataSelection,
    DataCatalogService& dataCatalog,
    QObject* parent)
    : QObject(parent)
    , m_WorkflowSelection(workflowSelection)
    , m_DataSelection(dataSelection)
    , m_DataCatalog(dataCatalog)
    , m_Snapshot(BuildSnapshot())
{
    connect(&m_WorkflowSelection,
            &WorkflowSelectionService::WorkflowChanged,
            this,
            [this](const QString&) {
                Refresh();
            });
    connect(&m_DataSelection,
            &DataSelectionService::SelectionChanged,
            this,
            [this](const QString&, const QString&) {
                Refresh();
            });
    connect(&m_DataCatalog,
            &DataCatalogService::EntriesChanged,
            this,
            [this]() {
                Refresh();
            });
}

WorkflowContextSnapshot WorkflowContextService::Snapshot() const
{
    return m_Snapshot;
}

QVector<DataWorkflowRole> WorkflowContextService::AcceptedDataRolesForWorkflow(
    const QString& workflowId)
{
    if (workflowId == QStringLiteral("image-preprocessing"))
    {
        return {DataWorkflowRole::DICOMSeries,
                DataWorkflowRole::Image};
    }

    if (workflowId == QStringLiteral("path"))
    {
        return {DataWorkflowRole::Image};
    }

    if (workflowId == QStringLiteral("segmentation-2d") ||
        workflowId == QStringLiteral("segmentation-3d"))
    {
        return {DataWorkflowRole::Image,
                DataWorkflowRole::Path};
    }

    if (workflowId == QStringLiteral("modeling"))
    {
        return {DataWorkflowRole::Path,
                DataWorkflowRole::Segmentation,
                DataWorkflowRole::Model};
    }

    if (workflowId == QStringLiteral("meshing"))
    {
        return {DataWorkflowRole::Model,
                DataWorkflowRole::Mesh};
    }

    if (workflowId == QStringLiteral("flow-simulation") ||
        workflowId == QStringLiteral("rom-simulation") ||
        workflowId == QStringLiteral("multiphysics"))
    {
        return {DataWorkflowRole::Mesh,
                DataWorkflowRole::SimulationPrep,
                DataWorkflowRole::SimulationResult};
    }

    return {};
}

WorkflowContextSnapshot WorkflowContextService::BuildSnapshot() const
{
    WorkflowContextSnapshot snapshot;
    snapshot.WorkflowId = m_WorkflowSelection.SelectedWorkflowId();

    if (const auto* workflow = FindWorkflowById(snapshot.WorkflowId))
        snapshot.WorkflowTitle = workflow->Title;

    const QVector<DataWorkflowRole> acceptedRoles =
        AcceptedDataRolesForWorkflow(snapshot.WorkflowId);
    snapshot.RequiresSelectedData = !acceptedRoles.isEmpty();

    const QString selectedCatalogEntryId =
        m_DataSelection.SelectedCatalogEntryId();
    if (const auto* entry = m_DataCatalog.FindById(selectedCatalogEntryId))
    {
        snapshot.HasSelectedData = true;
        snapshot.SelectedCatalogEntryId = entry->Id;
        snapshot.SelectedDataDisplayName = entry->DisplayName;
        snapshot.SelectedDataRole = entry->WorkflowRole;
    }

    snapshot.HasCompatibleSelection =
        !snapshot.RequiresSelectedData ||
        (snapshot.HasSelectedData &&
         ContainsRole(acceptedRoles, snapshot.SelectedDataRole));

    return snapshot;
}

void WorkflowContextService::Refresh()
{
    const WorkflowContextSnapshot nextSnapshot = BuildSnapshot();
    if (nextSnapshot == m_Snapshot)
        return;

    m_Snapshot = nextSnapshot;
    emit ContextChanged();
}

} // namespace xq::core
