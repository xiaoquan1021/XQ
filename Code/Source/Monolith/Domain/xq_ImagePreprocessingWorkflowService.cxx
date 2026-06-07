#include "xq_ImagePreprocessingWorkflowService.h"

namespace xq::domain
{

namespace
{

bool IsImagePreprocessingRole(xq::core::DataWorkflowRole role)
{
    return role == xq::core::DataWorkflowRole::DICOMSeries ||
           role == xq::core::DataWorkflowRole::Image;
}

QString SelectedDataLabel(
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const QString displayName = snapshot.SelectedDataDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return snapshot.SelectedCatalogEntryId;
}

ImagePreprocessingWorkflowResult FailedResult(const QString& message)
{
    ImagePreprocessingWorkflowResult result;
    result.Message = message;
    return result;
}

} // namespace

ImagePreprocessingWorkflowResult ImagePreprocessingWorkflowService::Run(
    const xq::core::WorkflowContextSnapshot& snapshot) const
{
    if (snapshot.WorkflowId != QStringLiteral("image-preprocessing"))
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing service requires the image-preprocessing workflow."));
    }

    if (!snapshot.HasSelectedData ||
        snapshot.SelectedCatalogEntryId.trimmed().isEmpty())
    {
        return FailedResult(QStringLiteral(
            "Selected data is required for image preprocessing."));
    }

    if (!IsImagePreprocessingRole(snapshot.SelectedDataRole))
    {
        return FailedResult(QStringLiteral(
            "Selected data is not compatible with image preprocessing."));
    }

    ImagePreprocessingWorkflowResult result;
    result.Succeeded = true;
    result.SourceCatalogEntryId = snapshot.SelectedCatalogEntryId;
    result.SelectedDataDisplayName = SelectedDataLabel(snapshot);
    result.Message =
        QStringLiteral("Image Preprocessing domain workflow accepted %1.")
            .arg(result.SelectedDataDisplayName);
    return result;
}

} // namespace xq::domain
