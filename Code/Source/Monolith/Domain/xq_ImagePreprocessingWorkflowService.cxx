#include "xq_ImagePreprocessingWorkflowService.h"

namespace xq::domain
{

namespace
{

ImagePreprocessingParameterDescriptor Parameter(
    const QString& id,
    const QString& title,
    ImagePreprocessingParameterValueType type)
{
    ImagePreprocessingParameterDescriptor parameter;
    parameter.Id = id;
    parameter.Title = title;
    parameter.Type = type;
    parameter.Required = true;
    return parameter;
}

ImagePreprocessingParameterDescriptor NumericParameter(
    const QString& id,
    const QString& title)
{
    return Parameter(id,
                     title,
                     ImagePreprocessingParameterValueType::NumericScalar);
}

ImagePreprocessingParameterDescriptor IntegerParameter(
    const QString& id,
    const QString& title)
{
    return Parameter(id,
                     title,
                     ImagePreprocessingParameterValueType::IntegerScalar);
}

const QVector<ImagePreprocessingOperationDescriptor>& DefaultOperations()
{
    static const QVector<ImagePreprocessingOperationDescriptor> operations = {
        {QStringLiteral("binary-threshold"),
         QStringLiteral("Binary Threshold"),
         {NumericParameter(QStringLiteral("lower"),
                           QStringLiteral("Lower Threshold")),
          NumericParameter(QStringLiteral("upper"),
                           QStringLiteral("Upper Threshold")),
          NumericParameter(QStringLiteral("inside-value"),
                           QStringLiteral("Inside Value")),
          NumericParameter(QStringLiteral("outside-value"),
                           QStringLiteral("Outside Value"))}},
        {QStringLiteral("connected-threshold"),
         QStringLiteral("Connected Threshold"),
         {NumericParameter(QStringLiteral("lower"),
                           QStringLiteral("Lower Threshold")),
          NumericParameter(QStringLiteral("upper"),
                           QStringLiteral("Upper Threshold")),
          Parameter(QStringLiteral("seeds"),
                    QStringLiteral("Seeds"),
                    ImagePreprocessingParameterValueType::IntegerPointList)}},
        {QStringLiteral("gaussian-smoothing"),
         QStringLiteral("Gaussian Smoothing"),
         {NumericParameter(QStringLiteral("sigma"),
                           QStringLiteral("Sigma"))}},
        {QStringLiteral("morphology-open-close"),
         QStringLiteral("Morphology Open/Close"),
         {IntegerParameter(QStringLiteral("radius"),
                           QStringLiteral("Radius"))}},
        {QStringLiteral("crop"),
         QStringLiteral("Crop"),
         {IntegerParameter(QStringLiteral("origin-x"),
                           QStringLiteral("Origin X")),
          IntegerParameter(QStringLiteral("origin-y"),
                           QStringLiteral("Origin Y")),
          IntegerParameter(QStringLiteral("origin-z"),
                           QStringLiteral("Origin Z")),
          IntegerParameter(QStringLiteral("size-x"),
                           QStringLiteral("Size X")),
          IntegerParameter(QStringLiteral("size-y"),
                           QStringLiteral("Size Y")),
          IntegerParameter(QStringLiteral("size-z"),
                           QStringLiteral("Size Z"))}},
        {QStringLiteral("resample"),
         QStringLiteral("Resample"),
         {NumericParameter(QStringLiteral("spacing-x"),
                           QStringLiteral("Spacing X")),
          NumericParameter(QStringLiteral("spacing-y"),
                           QStringLiteral("Spacing Y")),
          NumericParameter(QStringLiteral("spacing-z"),
                           QStringLiteral("Spacing Z"))}},
    };

    return operations;
}

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

const QVector<ImagePreprocessingOperationDescriptor>&
ImagePreprocessingWorkflowService::Operations() const
{
    return DefaultOperations();
}

const ImagePreprocessingOperationDescriptor*
ImagePreprocessingWorkflowService::FindOperation(
    const QString& operationId) const
{
    const QString normalizedOperationId = operationId.trimmed();
    const auto& operations = Operations();
    for (const auto& operation : operations)
    {
        if (operation.Id == normalizedOperationId)
            return &operation;
    }

    return nullptr;
}

ImagePreprocessingWorkflowResult
ImagePreprocessingWorkflowService::RunOperation(
    const xq::core::WorkflowContextSnapshot& snapshot,
    const QString& operationId) const
{
    auto result = Run(snapshot);
    if (!result.Succeeded)
        return result;

    const auto* operation = FindOperation(operationId);
    if (!operation)
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing operation was not found."));
    }

    result.OperationId = operation->Id;
    result.OperationTitle = operation->Title;
    result.Message =
        QStringLiteral("%1 preprocessing operation accepted %2.")
            .arg(result.OperationTitle, result.SelectedDataDisplayName);
    return result;
}

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
