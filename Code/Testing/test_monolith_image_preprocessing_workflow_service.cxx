#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_ImagePreprocessingWorkflowService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

xq::core::WorkflowContextSnapshot MakeSnapshot(
    const QString& workflowId,
    const QString& workflowTitle,
    const QString& entryId,
    const QString& displayName,
    xq::core::DataWorkflowRole role)
{
    xq::core::WorkflowContextSnapshot snapshot;
    snapshot.WorkflowId = workflowId;
    snapshot.WorkflowTitle = workflowTitle;
    snapshot.RequiresSelectedData = true;
    snapshot.HasSelectedData = !entryId.trimmed().isEmpty();
    snapshot.HasCompatibleSelection = true;
    snapshot.SelectedCatalogEntryId = entryId;
    snapshot.SelectedDataDisplayName = displayName;
    snapshot.SelectedDataRole = role;
    return snapshot;
}

xq::core::DataImportRequest MakeImport(const QString& id,
                                       const QString& displayName,
                                       xq::core::DataWorkflowRole role)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = role;
    return request;
}

int ExpectParameter(
    const xq::domain::ImagePreprocessingParameterDescriptor& parameter,
    const QString& id,
    xq::domain::ImagePreprocessingParameterValueType valueType)
{
    if (Expect(parameter.Id == id,
               "image preprocessing parameter id should be stable"))
        return 1;
    if (Expect(!parameter.Title.trimmed().isEmpty(),
               "image preprocessing parameter title should be user-facing"))
        return 1;
    if (Expect(parameter.Type == valueType,
               "image preprocessing parameter type should match operation"))
        return 1;
    if (Expect(parameter.Required,
               "image preprocessing operation parameters should be required"))
        return 1;

    return 0;
}

QVariantList Point(int x, int y, int z)
{
    QVariantList point;
    point.append(x);
    point.append(y);
    point.append(z);
    return point;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::domain::ImagePreprocessingWorkflowService service;

    const QStringList expectedOperationIds = {
        QStringLiteral("binary-threshold"),
        QStringLiteral("connected-threshold"),
        QStringLiteral("gaussian-smoothing"),
        QStringLiteral("morphology-open-close"),
        QStringLiteral("crop"),
        QStringLiteral("resample"),
    };
    const auto operations = service.Operations();
    if (Expect(operations.size() == expectedOperationIds.size(),
               "image preprocessing service should expose six operations"))
        return 1;

    for (int i = 0; i < expectedOperationIds.size(); ++i)
    {
        if (Expect(operations.at(i).Id == expectedOperationIds.at(i),
                   "image preprocessing operation id order should be stable"))
            return 1;
        if (Expect(!operations.at(i).Title.trimmed().isEmpty(),
                   "image preprocessing operation title should be user-facing"))
            return 1;
    }

    const auto* smoothed =
        service.FindOperation(QStringLiteral(" gaussian-smoothing "));
    if (Expect(smoothed != nullptr,
               "image preprocessing operation lookup should trim ids"))
        return 1;
    if (Expect(smoothed->Title == QStringLiteral("Gaussian Smoothing"),
               "image preprocessing lookup should return descriptor data"))
        return 1;
    if (Expect(service.FindOperation(QStringLiteral("marching-cubes")) == nullptr,
               "surface extraction should not be in preprocessing catalog"))
        return 1;

    const auto* binaryThreshold =
        service.FindOperation(QStringLiteral("binary-threshold"));
    if (Expect(binaryThreshold->Parameters.size() == 4,
               "binary threshold should expose four parameters"))
        return 1;
    if (ExpectParameter(binaryThreshold->Parameters.at(0),
                        QStringLiteral("lower"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;
    if (ExpectParameter(binaryThreshold->Parameters.at(1),
                        QStringLiteral("upper"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;
    if (ExpectParameter(binaryThreshold->Parameters.at(2),
                        QStringLiteral("inside-value"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;
    if (ExpectParameter(binaryThreshold->Parameters.at(3),
                        QStringLiteral("outside-value"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;

    const auto* connectedThreshold =
        service.FindOperation(QStringLiteral("connected-threshold"));
    if (Expect(connectedThreshold->Parameters.size() == 3,
               "connected threshold should expose three parameters"))
        return 1;
    if (ExpectParameter(connectedThreshold->Parameters.at(0),
                        QStringLiteral("lower"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;
    if (ExpectParameter(connectedThreshold->Parameters.at(1),
                        QStringLiteral("upper"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;
    if (ExpectParameter(connectedThreshold->Parameters.at(2),
                        QStringLiteral("seeds"),
                        xq::domain::ImagePreprocessingParameterValueType::IntegerPointList))
        return 1;

    const auto* gaussianSmoothing =
        service.FindOperation(QStringLiteral("gaussian-smoothing"));
    if (Expect(gaussianSmoothing->Parameters.size() == 1,
               "Gaussian smoothing should expose one parameter"))
        return 1;
    if (ExpectParameter(gaussianSmoothing->Parameters.at(0),
                        QStringLiteral("sigma"),
                        xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
        return 1;

    const auto* morphology =
        service.FindOperation(QStringLiteral("morphology-open-close"));
    if (Expect(morphology->Parameters.size() == 1,
               "morphology should expose one parameter"))
        return 1;
    if (ExpectParameter(morphology->Parameters.at(0),
                        QStringLiteral("radius"),
                        xq::domain::ImagePreprocessingParameterValueType::IntegerScalar))
        return 1;

    const auto* crop = service.FindOperation(QStringLiteral("crop"));
    const QStringList cropParameterIds = {
        QStringLiteral("origin-x"),
        QStringLiteral("origin-y"),
        QStringLiteral("origin-z"),
        QStringLiteral("size-x"),
        QStringLiteral("size-y"),
        QStringLiteral("size-z"),
    };
    if (Expect(crop->Parameters.size() == cropParameterIds.size(),
               "crop should expose six ordered parameters"))
        return 1;
    for (int i = 0; i < cropParameterIds.size(); ++i)
    {
        if (ExpectParameter(crop->Parameters.at(i),
                            cropParameterIds.at(i),
                            xq::domain::ImagePreprocessingParameterValueType::IntegerScalar))
            return 1;
    }

    const auto* resample = service.FindOperation(QStringLiteral("resample"));
    const QStringList resampleParameterIds = {
        QStringLiteral("spacing-x"),
        QStringLiteral("spacing-y"),
        QStringLiteral("spacing-z"),
    };
    if (Expect(resample->Parameters.size() == resampleParameterIds.size(),
               "resample should expose three ordered parameters"))
        return 1;
    for (int i = 0; i < resampleParameterIds.size(); ++i)
    {
        if (ExpectParameter(resample->Parameters.at(i),
                            resampleParameterIds.at(i),
                            xq::domain::ImagePreprocessingParameterValueType::NumericScalar))
            return 1;
    }

    QVariantMap gaussianParameters;
    gaussianParameters.insert(QStringLiteral("sigma"), 1.25);
    const auto gaussianValidation =
        service.ValidateOperationParameters(QStringLiteral("gaussian-smoothing"),
                                            gaussianParameters);
    if (Expect(gaussianValidation.Succeeded,
               "valid Gaussian smoothing parameters should pass"))
        return 1;

    const auto missingSigmaValidation =
        service.ValidateOperationParameters(QStringLiteral("gaussian-smoothing"),
                                            QVariantMap());
    if (Expect(!missingSigmaValidation.Succeeded,
               "missing Gaussian sigma should fail validation"))
        return 1;
    if (Expect(missingSigmaValidation.Message ==
                   QStringLiteral("Image preprocessing parameter is required: sigma."),
               "missing parameter message should include id"))
        return 1;

    QVariantMap cropParameters;
    cropParameters.insert(QStringLiteral("origin-x"), 0);
    cropParameters.insert(QStringLiteral("origin-y"), 1);
    cropParameters.insert(QStringLiteral("origin-z"), 2);
    cropParameters.insert(QStringLiteral("size-x"), 10);
    cropParameters.insert(QStringLiteral("size-y"), 11);
    cropParameters.insert(QStringLiteral("size-z"), 12);
    const auto cropValidation =
        service.ValidateOperationParameters(QStringLiteral("crop"),
                                            cropParameters);
    if (Expect(cropValidation.Succeeded,
               "valid crop integer parameters should pass"))
        return 1;

    cropParameters.insert(QStringLiteral("origin-x"), 0.5);
    const auto wrongCropValidation =
        service.ValidateOperationParameters(QStringLiteral("crop"),
                                            cropParameters);
    if (Expect(!wrongCropValidation.Succeeded,
               "non-integer crop parameter should fail validation"))
        return 1;
    if (Expect(wrongCropValidation.Message ==
                   QStringLiteral("Image preprocessing parameter must be an integer: origin-x."),
               "integer type rejection should include parameter id"))
        return 1;

    QVariantList seeds;
    seeds.append(QVariant::fromValue(Point(1, 2, 3)));
    seeds.append(QVariant::fromValue(Point(4, 5, 6)));
    QVariantMap connectedParameters;
    connectedParameters.insert(QStringLiteral("lower"), 20.0);
    connectedParameters.insert(QStringLiteral("upper"), 120.0);
    connectedParameters.insert(QStringLiteral("seeds"), seeds);
    const auto connectedValidation =
        service.ValidateOperationParameters(QStringLiteral("connected-threshold"),
                                            connectedParameters);
    if (Expect(connectedValidation.Succeeded,
               "valid connected-threshold seed list should pass"))
        return 1;

    const auto unknownValidation =
        service.ValidateOperationParameters(QStringLiteral("missing-operation"),
                                            gaussianParameters);
    if (Expect(!unknownValidation.Succeeded,
               "unknown operation validation should fail"))
        return 1;
    if (Expect(unknownValidation.Message ==
                   QStringLiteral("Image preprocessing operation was not found."),
               "unknown operation validation should reuse operation message"))
        return 1;

    const auto smoothOperationResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-001"),
                     QStringLiteral("CTA Image"),
                     xq::core::DataWorkflowRole::Image),
        QStringLiteral(" gaussian-smoothing "));
    if (Expect(smoothOperationResult.Succeeded,
               "known image preprocessing operation should run"))
        return 1;
    if (Expect(smoothOperationResult.OperationId ==
                   QStringLiteral("gaussian-smoothing"),
               "operation request should expose normalized operation id"))
        return 1;
    if (Expect(smoothOperationResult.OperationTitle ==
                   QStringLiteral("Gaussian Smoothing"),
               "operation request should expose operation title"))
        return 1;
    if (Expect(smoothOperationResult.Message ==
                   QStringLiteral("Gaussian Smoothing preprocessing operation accepted CTA Image."),
               "operation request success message should include operation title and data"))
        return 1;

    const auto parameterizedSmoothResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-002"),
                     QStringLiteral("Parameterized CTA"),
                     xq::core::DataWorkflowRole::Image),
        QStringLiteral(" gaussian-smoothing "),
        gaussianParameters);
    if (Expect(parameterizedSmoothResult.Succeeded,
               "valid parameterized image preprocessing operation should run"))
        return 1;
    if (Expect(parameterizedSmoothResult.SourceCatalogEntryId ==
                   QStringLiteral("image-002"),
               "parameterized operation should expose source id"))
        return 1;
    if (Expect(parameterizedSmoothResult.OperationId ==
                   QStringLiteral("gaussian-smoothing"),
               "parameterized operation should expose normalized operation id"))
        return 1;
    if (Expect(parameterizedSmoothResult.OperationTitle ==
                   QStringLiteral("Gaussian Smoothing"),
               "parameterized operation should expose operation title"))
        return 1;
    if (Expect(parameterizedSmoothResult.Message ==
                   QStringLiteral("Gaussian Smoothing preprocessing operation accepted Parameterized CTA."),
               "parameterized operation success message should stay stable"))
        return 1;

    const auto missingSigmaOperationResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-003"),
                     QStringLiteral("Missing Sigma CTA"),
                     xq::core::DataWorkflowRole::Image),
        QStringLiteral("gaussian-smoothing"),
        QVariantMap());
    if (Expect(!missingSigmaOperationResult.Succeeded,
               "parameterized operation should reject missing required parameters"))
        return 1;
    if (Expect(missingSigmaOperationResult.Message ==
                   QStringLiteral("Image preprocessing parameter is required: sigma."),
               "parameterized operation should return validator message"))
        return 1;
    if (Expect(missingSigmaOperationResult.OperationId.isEmpty(),
               "failed parameterized operation should not report operation metadata"))
        return 1;

    const auto missingParameterizedOperationResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-004"),
                     QStringLiteral("Missing Operation CTA"),
                     xq::core::DataWorkflowRole::Image),
        QStringLiteral("missing-operation"),
        gaussianParameters);
    if (Expect(!missingParameterizedOperationResult.Succeeded,
               "parameterized unknown operation should fail"))
        return 1;
    if (Expect(missingParameterizedOperationResult.Message ==
                   QStringLiteral("Image preprocessing operation was not found."),
               "parameterized unknown operation should reuse operation message"))
        return 1;

    const auto missingOperationResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-001"),
                     QStringLiteral("CTA Image"),
                     xq::core::DataWorkflowRole::Image),
        QStringLiteral("missing-operation"));
    if (Expect(!missingOperationResult.Succeeded,
               "unknown image preprocessing operation should fail"))
        return 1;
    if (Expect(missingOperationResult.Message ==
                   QStringLiteral("Image preprocessing operation was not found."),
               "unknown operation rejection message should be explicit"))
        return 1;

    const auto incompatibleOperationResult = service.RunOperation(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("model-001"),
                     QStringLiteral("Aorta Model"),
                     xq::core::DataWorkflowRole::Model),
        QStringLiteral("resample"));
    if (Expect(!incompatibleOperationResult.Succeeded,
               "operation request should reject incompatible data"))
        return 1;
    if (Expect(incompatibleOperationResult.Message ==
                   QStringLiteral("Selected data is not compatible with image preprocessing."),
               "operation request should reuse compatibility rejection"))
        return 1;

    const auto imageResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-001"),
                     QStringLiteral("CTA Image"),
                     xq::core::DataWorkflowRole::Image));
    if (Expect(imageResult.Succeeded,
               "image preprocessing service should accept image data"))
        return 1;
    if (Expect(imageResult.SourceCatalogEntryId ==
                   QStringLiteral("image-001"),
               "image preprocessing result should expose source id"))
        return 1;
    if (Expect(imageResult.SelectedDataDisplayName ==
                   QStringLiteral("CTA Image"),
               "image preprocessing result should expose display name"))
        return 1;
    if (Expect(imageResult.Message ==
                   QStringLiteral("Image Preprocessing domain workflow accepted CTA Image."),
               "image preprocessing success message should stay stable"))
        return 1;

    const auto dicomResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("dicom-001"),
                     QStringLiteral("CT Series"),
                     xq::core::DataWorkflowRole::DICOMSeries));
    if (Expect(dicomResult.Succeeded,
               "image preprocessing service should accept DICOM series"))
        return 1;

    const auto wrongWorkflowResult = service.Run(
        MakeSnapshot(QStringLiteral("path"),
                     QStringLiteral("Path"),
                     QStringLiteral("image-002"),
                     QStringLiteral("Wrong Workflow CTA"),
                     xq::core::DataWorkflowRole::Image));
    if (Expect(!wrongWorkflowResult.Succeeded,
               "image preprocessing service should reject wrong workflow id"))
        return 1;
    if (Expect(wrongWorkflowResult.Message ==
                   QStringLiteral("Image preprocessing service requires the image-preprocessing workflow."),
               "wrong workflow rejection message should be explicit"))
        return 1;

    const auto incompatibleRoleResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("model-001"),
                     QStringLiteral("Aorta Model"),
                     xq::core::DataWorkflowRole::Model));
    if (Expect(!incompatibleRoleResult.Succeeded,
               "image preprocessing service should reject incompatible data"))
        return 1;
    if (Expect(incompatibleRoleResult.Message ==
                   QStringLiteral("Selected data is not compatible with image preprocessing."),
               "incompatible role rejection message should be explicit"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions());
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("registered-image"),
                                           QStringLiteral("Registered CTA"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "registered image import should succeed"))
    {
        delete context;
        return 1;
    }

    QString actionMessage;
    if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "registered image preprocessing handler should run service"))
    {
        delete context;
        return 1;
    }
    const auto history = context->Tasks()->History();
    if (Expect(!history.empty(),
               "registered image preprocessing handler should record task"))
    {
        delete context;
        return 1;
    }
    if (Expect(history.back().Message ==
                   QStringLiteral("Image Preprocessing domain workflow accepted Registered CTA."),
               "registered image preprocessing handler should use service message"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
