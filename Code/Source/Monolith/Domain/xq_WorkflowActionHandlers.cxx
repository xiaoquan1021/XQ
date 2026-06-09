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

QString OperationTitle(xq::core::WorkflowOperationService* operations,
                       const QString& workflowId,
                       const QString& operationId)
{
    if (!operations)
        return {};

    for (const auto& operation :
         operations->OperationsForWorkflow(workflowId))
    {
        if (operation.Id == operationId)
            return operation.Title;
    }

    return {};
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateOperationAwareHandler(xq::core::WorkflowOperationService* operations,
                            const QString& operationKind)
{
    if (!operations)
        return CreateDefaultHandler();

    return [operations, operationKind](
               const xq::core::WorkflowContextSnapshot& snapshot,
               QString* message) {
        const QString operationId =
            operations->SelectedOperationId(snapshot.WorkflowId);
        const QString operationTitle =
            OperationTitle(operations, snapshot.WorkflowId, operationId);
        if (operationTitle.trimmed().isEmpty())
        {
            return CreateDefaultHandler()(snapshot, message);
        }

        if (message)
        {
            if (!snapshot.RequiresSelectedData)
            {
                *message =
                    QStringLiteral("%1 %2 operation accepted.")
                        .arg(operationTitle, operationKind);
            }
            else
            {
                *message =
                    QStringLiteral("%1 %2 operation accepted %3.")
                        .arg(operationTitle,
                             operationKind,
                             SelectedDataLabel(snapshot));
            }
        }
        return true;
    };
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateSegmentationHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations,
                                       QStringLiteral("segmentation"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreatePathHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations, QStringLiteral("path"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateModelingHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations, QStringLiteral("modeling"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateMeshingHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations, QStringLiteral("meshing"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateFlowSimulationHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations,
                                       QStringLiteral("flow simulation"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateRomSimulationHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations,
                                       QStringLiteral("rom simulation"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateMultiPhysicsHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations,
                                       QStringLiteral("multiphysics"));
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreatePythonApiHandler(xq::core::WorkflowOperationService* operations)
{
    return CreateOperationAwareHandler(operations,
                                       QStringLiteral("python api"));
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

xq::core::WorkflowOperationParameterOption Option(const QString& id,
                                                  const QString& title)
{
    xq::core::WorkflowOperationParameterOption option;
    option.Id = id;
    option.Title = title;
    return option;
}

xq::core::WorkflowOperationParameterDescriptor OptionParameter(
    const QString& id,
    const QString& title,
    const QVector<xq::core::WorkflowOperationParameterOption>& options)
{
    auto parameter =
        Parameter(id,
                  title,
                  xq::core::WorkflowOperationParameterValueType::Option);
    parameter.Options = options;
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

QVector<xq::core::WorkflowOperationDescriptor> PathOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("create-centerline"),
                  QStringLiteral("Create Centerline"),
                  {Parameter(QStringLiteral("seed-points"),
                             QStringLiteral("Seed Points"),
                             Type::IntegerPointList),
                   Parameter(QStringLiteral("control-point-count"),
                             QStringLiteral("Control Point Count"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("edit-control-points"),
                  QStringLiteral("Edit Control Points"),
                  {Parameter(QStringLiteral("snap-distance"),
                             QStringLiteral("Snap Distance"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("smooth-path"),
                  QStringLiteral("Smooth Path"),
                  {Parameter(QStringLiteral("smoothing-factor"),
                             QStringLiteral("Smoothing Factor"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("iteration-count"),
                             QStringLiteral("Iteration Count"),
                             Type::IntegerScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> ModelingOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("loft-surface"),
                  QStringLiteral("Loft Surface"),
                  {Parameter(QStringLiteral("sample-count"),
                             QStringLiteral("Sample Count"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("build-solid-model"),
                  QStringLiteral("Build Solid Model"),
                  {Parameter(QStringLiteral("blend-radius"),
                             QStringLiteral("Blend Radius"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("wall-thickness"),
                             QStringLiteral("Wall Thickness"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("trim-branches"),
                  QStringLiteral("Trim Branches"),
                  {Parameter(QStringLiteral("trim-distance"),
                             QStringLiteral("Trim Distance"),
                             Type::NumericScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> MeshingOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("generate-surface-mesh"),
                  QStringLiteral("Generate Surface Mesh"),
                  {Parameter(QStringLiteral("target-edge-length"),
                             QStringLiteral("Target Edge Length"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("generate-volume-mesh"),
                  QStringLiteral("Generate Volume Mesh"),
                  {Parameter(QStringLiteral("element-size"),
                             QStringLiteral("Element Size"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("optimization-steps"),
                             QStringLiteral("Optimization Steps"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("boundary-layers"),
                  QStringLiteral("Boundary Layers"),
                  {Parameter(QStringLiteral("layer-count"),
                             QStringLiteral("Layer Count"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("growth-rate"),
                             QStringLiteral("Growth Rate"),
                             Type::NumericScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> FlowSimulationOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("configure-cfd-job"),
                  QStringLiteral("Configure CFD Job"),
                  {OptionParameter(
                       QStringLiteral("solver-profile"),
                       QStringLiteral("Solver Profile"),
                       {Option(QStringLiteral("steady"),
                               QStringLiteral("Steady")),
                        Option(QStringLiteral("pulsatile"),
                               QStringLiteral("Pulsatile")),
                        Option(QStringLiteral("transient"),
                               QStringLiteral("Transient"))}),
                   Parameter(QStringLiteral("inlet-count"),
                             QStringLiteral("Inlet Count"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("outlet-count"),
                             QStringLiteral("Outlet Count"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("run-steady-flow"),
                  QStringLiteral("Steady Flow Solve"),
                  {Parameter(QStringLiteral("convergence-tolerance"),
                             QStringLiteral("Convergence Tolerance"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("max-iterations"),
                             QStringLiteral("Max Iterations"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("review-flow-results"),
                  QStringLiteral("Review Results"),
                  {Parameter(QStringLiteral("sample-count"),
                             QStringLiteral("Sample Count"),
                             Type::IntegerScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> RomSimulationOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("build-1d-network"),
                  QStringLiteral("Build 1D Network"),
                  {Parameter(QStringLiteral("branch-count"),
                             QStringLiteral("Branch Count"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("outlet-count"),
                             QStringLiteral("Outlet Count"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("calibrate-boundary-conditions"),
                  QStringLiteral("Calibrate Boundary Conditions"),
                  {Parameter(QStringLiteral("target-flow-rate"),
                             QStringLiteral("Target Flow Rate"),
                             Type::NumericScalar),
                   Parameter(QStringLiteral("resistance-scale"),
                             QStringLiteral("Resistance Scale"),
                             Type::NumericScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> MultiPhysicsOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("configure-coupling"),
                  QStringLiteral("Configure Coupling"),
                  {Parameter(QStringLiteral("coupling-iterations"),
                             QStringLiteral("Coupling Iterations"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("relaxation-factor"),
                             QStringLiteral("Relaxation Factor"),
                             Type::NumericScalar)}),
        Operation(QStringLiteral("review-coupled-results"),
                  QStringLiteral("Review Coupled Results"),
                  {Parameter(QStringLiteral("sample-count"),
                             QStringLiteral("Sample Count"),
                             Type::IntegerScalar)}),
    };
}

QVector<xq::core::WorkflowOperationDescriptor> PythonApiOperations()
{
    using Type = xq::core::WorkflowOperationParameterValueType;
    return {
        Operation(QStringLiteral("open-python-console"),
                  QStringLiteral("Open Python Console"),
                  {Parameter(QStringLiteral("max-history-items"),
                             QStringLiteral("Max History Items"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("run-project-script"),
                  QStringLiteral("Project Script Runner"),
                  {Parameter(QStringLiteral("script-timeout-seconds"),
                             QStringLiteral("Script Timeout Seconds"),
                             Type::IntegerScalar),
                   Parameter(QStringLiteral("max-output-lines"),
                             QStringLiteral("Max Output Lines"),
                             Type::IntegerScalar)}),
        Operation(QStringLiteral("export-api-snippet"),
                  QStringLiteral("Export API Snippet"),
                  {Parameter(QStringLiteral("snippet-count"),
                             QStringLiteral("Snippet Count"),
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
        operations->RegisterOperations(QStringLiteral("path"),
                                       PathOperations());
        operations->RegisterOperations(QStringLiteral("segmentation-2d"),
                                       Segmentation2DOperations());
        operations->RegisterOperations(QStringLiteral("segmentation-3d"),
                                       Segmentation3DOperations());
        operations->RegisterOperations(QStringLiteral("modeling"),
                                       ModelingOperations());
        operations->RegisterOperations(QStringLiteral("meshing"),
                                       MeshingOperations());
        operations->RegisterOperations(QStringLiteral("flow-simulation"),
                                       FlowSimulationOperations());
        operations->RegisterOperations(QStringLiteral("rom-simulation"),
                                       RomSimulationOperations());
        operations->RegisterOperations(QStringLiteral("multiphysics"),
                                       MultiPhysicsOperations());
        operations->RegisterOperations(QStringLiteral("python-api"),
                                       PythonApiOperations());
    }

    if (actions.RegisterHandler(QStringLiteral("image-preprocessing"),
                                CreateImagePreprocessingHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("path"),
                                CreatePathHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("segmentation-2d"),
                                CreateSegmentationHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("segmentation-3d"),
                                CreateSegmentationHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("modeling"),
                                CreateModelingHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("meshing"),
                                CreateMeshingHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("flow-simulation"),
                                CreateFlowSimulationHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("rom-simulation"),
                                CreateRomSimulationHandler(operations)))
    {
        ++registered;
    }

    if (actions.RegisterHandler(QStringLiteral("multiphysics"),
                                CreateMultiPhysicsHandler(operations)))
    {
        ++registered;
    }

    if (operations &&
        actions.RegisterHandler(QStringLiteral("python-api"),
                                CreatePythonApiHandler(operations)))
    {
        ++registered;
    }

    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (workflowId == QStringLiteral("image-preprocessing") ||
            workflowId == QStringLiteral("path") ||
            workflowId == QStringLiteral("segmentation-2d") ||
            workflowId == QStringLiteral("segmentation-3d") ||
            workflowId == QStringLiteral("modeling") ||
            workflowId == QStringLiteral("meshing") ||
            workflowId == QStringLiteral("flow-simulation") ||
            workflowId == QStringLiteral("rom-simulation") ||
            workflowId == QStringLiteral("multiphysics"))
            continue;

        if (actions.RegisterHandler(workflowId, CreateDefaultHandler()))
            ++registered;
    }

    return registered;
}

} // namespace xq::domain
