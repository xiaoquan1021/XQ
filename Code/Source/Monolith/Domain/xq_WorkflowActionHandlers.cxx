#include "xq_WorkflowActionHandlers.h"

#include "xq_ImagePreprocessingWorkflowService.h"

#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace xq::domain
{

namespace
{

QString SelectedDataLabel(
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const QString displayName = snapshot.SelectedDataDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return snapshot.SelectedCatalogEntryId;
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateDefaultHandler()
{
    return [](const xq::core::WorkflowContextSnapshot& snapshot,
              QString* message) {
        if (message)
        {
            *message = QStringLiteral("%1 domain workflow accepted %2.")
                           .arg(snapshot.WorkflowTitle,
                                SelectedDataLabel(snapshot));
        }
        return true;
    };
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateImagePreprocessingHandler(xq::core::WorkflowOperationService* operations)
{
    auto service = std::make_shared<ImagePreprocessingWorkflowService>();
    return [service, operations](
               const xq::core::WorkflowContextSnapshot& snapshot,
               QString* message) {
        if (operations)
        {
            const QString operationId =
                operations->SelectedOperationId(snapshot.WorkflowId);
            if (!operationId.trimmed().isEmpty())
            {
                const auto operationResult =
                    service->RunOperation(
                        snapshot,
                        operationId,
                        operations->ParameterValues(snapshot.WorkflowId,
                                                    operationId));
                if (message)
                    *message = operationResult.Message;
                return operationResult.Succeeded;
            }
        }

        const auto result = service->Run(snapshot);
        if (message)
            *message = result.Message;
        return result.Succeeded;
    };
}

QVector<xq::core::WorkflowOperationDescriptor>
ImagePreprocessingOperations()
{
    ImagePreprocessingWorkflowService service;
    QVector<xq::core::WorkflowOperationDescriptor> operations;
    for (const auto& operation : service.Operations())
    {
        xq::core::WorkflowOperationDescriptor descriptor;
        descriptor.Id = operation.Id;
        descriptor.Title = operation.Title;
        for (const auto& parameter : operation.Parameters)
        {
            xq::core::WorkflowOperationParameterDescriptor
                parameterDescriptor;
            parameterDescriptor.Id = parameter.Id;
            parameterDescriptor.Title = parameter.Title;
            parameterDescriptor.Required = parameter.Required;
            switch (parameter.Type)
            {
            case ImagePreprocessingParameterValueType::NumericScalar:
                parameterDescriptor.Type =
                    xq::core::WorkflowOperationParameterValueType::
                        NumericScalar;
                break;
            case ImagePreprocessingParameterValueType::IntegerScalar:
                parameterDescriptor.Type =
                    xq::core::WorkflowOperationParameterValueType::
                        IntegerScalar;
                break;
            case ImagePreprocessingParameterValueType::IntegerPointList:
                parameterDescriptor.Type =
                    xq::core::WorkflowOperationParameterValueType::
                        IntegerPointList;
                break;
            }
            descriptor.Parameters.push_back(parameterDescriptor);
        }
        operations.push_back(descriptor);
    }

    return operations;
}

xq::core::WorkflowOperationParameterDescriptor Parameter(
    const QString& id,
    const QString& title,
    xq::core::WorkflowOperationParameterValueType type)
{
    xq::core::WorkflowOperationParameterDescriptor parameter;
    parameter.Id = id;
    parameter.Title = title;
    parameter.Type = type;
    parameter.Required = true;
    return parameter;
}

xq::core::WorkflowOperationDescriptor Operation(
    const QString& id,
    const QString& title,
    const QVector<xq::core::WorkflowOperationParameterDescriptor>& parameters)
{
    xq::core::WorkflowOperationDescriptor operation;
    operation.Id = id;
    operation.Title = title;
    operation.Parameters = parameters;
    return operation;
}

QVector<xq::core::WorkflowOperationDescriptor> Segmentation2DOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("threshold-contour"),
                  QStringLiteral("Threshold Contour"),
                  {Parameter(QStringLiteral("threshold-lower"),
                             QStringLiteral("Lower Threshold"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("threshold-upper"),
                             QStringLiteral("Upper Threshold"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("manual-contour"),
                  QStringLiteral("Manual Contour"),
                  {Parameter(QStringLiteral("smoothing"),
                             QStringLiteral("Smoothing"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("loft-profiles"),
                  QStringLiteral("Loft Profiles"),
                  {Parameter(QStringLiteral("sample-count"),
                             QStringLiteral("Sample Count"),
                             Type::IntegerScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> Segmentation3DOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("threshold-region"),
                  QStringLiteral("Threshold Region"),
                  {Parameter(QStringLiteral("threshold-lower"),
                             QStringLiteral("Lower Threshold"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("threshold-upper"),
                             QStringLiteral("Upper Threshold"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("region-growing"),
                  QStringLiteral("Region Growing"),
                  {Parameter(QStringLiteral("seed-x"),
                             QStringLiteral("Seed X"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("seed-y"),
                             QStringLiteral("Seed Y"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("threshold-upper"),
                             QStringLiteral("Upper Threshold"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("surface-preview"),
                  QStringLiteral("Surface Preview"),
                  {Parameter(QStringLiteral("smoothing-iterations"),
                             QStringLiteral("Smoothing Iterations"),
                             Type::IntegerScalar)}),
    };
}

} // namespace

int RegisterDefaultWorkflowActionHandlers(
    xq::core::WorkflowActionService& actions,
    xq::core::WorkflowOperationService* operations)
{
    const QStringList dataDependentWorkflowIds = {
        QStringLiteral("image-preprocessing"),
        QStringLiteral("path"),
        QStringLiteral("segmentation-2d"),
        QStringLiteral("segmentation-3d"),
        QStringLiteral("modeling"),
        QStringLiteral("meshing"),
        QStringLiteral("flow-simulation"),
        QStringLiteral("rom-simulation"),
        QStringLiteral("multiphysics"),
    };

    int registered = 0;
    if (operations)
    {
        operations->RegisterOperations(QStringLiteral("image-preprocessing"),
                                       ImagePreprocessingOperations());
        operations->RegisterOperations(QStringLiteral("segmentation-2d"),
                                       Segmentation2DOperations());
        operations->RegisterOperations(QStringLiteral("segmentation-3d"),
                                       Segmentation3DOperations());
    }

    if (actions.RegisterHandler(QStringLiteral("image-preprocessing"),
                                CreateImagePreprocessingHandler(operations)))
    {
        ++registered;
    }

    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (workflowId == QStringLiteral("image-preprocessing"))
            continue;

        if (actions.RegisterHandler(workflowId, CreateDefaultHandler()))
            ++registered;
    }

    return registered;
}

} // namespace xq::domain
