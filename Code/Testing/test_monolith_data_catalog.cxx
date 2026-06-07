#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"

#include <QCoreApplication>
#include <QString>

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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::DataCatalogService catalog;
    if (Expect(catalog.Entries().isEmpty(),
               "new DataCatalogService should start empty"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("missing")) == nullptr,
               "missing data id should not resolve"))
        return 1;

    xq::core::DataCatalogEntry imageEntry;
    imageEntry.Id = QStringLiteral("image-001");
    imageEntry.DisplayName = QStringLiteral("CTA Head");
    imageEntry.SourcePath = QStringLiteral("C:/studies/head/series1");
    imageEntry.Modality = QStringLiteral("CT");
    imageEntry.WorkflowRole = xq::core::DataWorkflowRole::Image;

    QString errorMessage;
    if (Expect(catalog.RegisterEntry(imageEntry, &errorMessage),
               "valid image entry should register"))
        return 1;

    xq::core::DataCatalogEntry dicomEntry;
    dicomEntry.Id = QStringLiteral("dicom-002");
    dicomEntry.DisplayName = QStringLiteral("MR Vessel");
    dicomEntry.SourcePath = QStringLiteral("C:/studies/mr/dicom");
    dicomEntry.Modality = QStringLiteral("MR");
    dicomEntry.WorkflowRole = xq::core::DataWorkflowRole::DICOMSeries;

    if (Expect(catalog.RegisterEntry(dicomEntry, &errorMessage),
               "valid DICOM entry should register"))
        return 1;

    const auto entries = catalog.Entries();
    if (Expect(entries.size() == 2,
               "registered entries should be listed in insertion order"))
        return 1;
    if (Expect(entries.at(0).Id == QStringLiteral("image-001"),
               "first registered entry should remain first"))
        return 1;
    if (Expect(entries.at(1).Id == QStringLiteral("dicom-002"),
               "second registered entry should remain second"))
        return 1;

    const auto* foundImage = catalog.FindById(QStringLiteral("image-001"));
    if (Expect(foundImage != nullptr,
               "registered image should be discoverable by id"))
        return 1;
    if (Expect(foundImage->DisplayName == QStringLiteral("CTA Head"),
               "registered image metadata should be preserved"))
        return 1;
    if (Expect(foundImage->WorkflowRole == xq::core::DataWorkflowRole::Image,
               "registered image workflow role should be preserved"))
        return 1;

    errorMessage.clear();
    if (Expect(!catalog.RegisterEntry(imageEntry, &errorMessage),
               "duplicate data id should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("Duplicate")),
               "duplicate id failure should provide a useful error"))
        return 1;
    if (Expect(catalog.Entries().size() == 2,
               "duplicate id should not add a second entry"))
        return 1;

    xq::core::DataCatalogEntry invalidEntry;
    invalidEntry.Id = QStringLiteral("empty-source");
    invalidEntry.DisplayName = QStringLiteral("No Source");
    invalidEntry.WorkflowRole = xq::core::DataWorkflowRole::Image;
    errorMessage.clear();
    if (Expect(!catalog.RegisterEntry(invalidEntry, &errorMessage),
               "entry without a source path should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("source")),
               "empty source path failure should provide a useful error"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->DataCatalog() != nullptr,
               "ApplicationContext should expose DataCatalogService"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "ApplicationContext DataCatalogService should start empty"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
