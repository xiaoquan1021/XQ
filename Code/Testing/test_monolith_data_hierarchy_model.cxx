#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Presentation/xq_DataHierarchyModel.h"

#include <QCoreApplication>
#include <QModelIndex>
#include <Qt>

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

xq::core::DataImportRequest MakeImageImport(const QString& id,
                                            const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::DataHierarchyModel model(*context->DataHierarchy());

    if (Expect(model.rowCount(QModelIndex()) == 0,
               "new hierarchy model should start without root children"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto firstImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(firstImport.Succeeded, "first image import should succeed"))
    {
        delete context;
        return 1;
    }

    if (Expect(model.rowCount(QModelIndex()) == 1,
               "image import should expose one root folder in the model"))
    {
        delete context;
        return 1;
    }

    QModelIndex imagesIndex = model.index(0, 0, QModelIndex());
    if (Expect(imagesIndex.isValid(),
               "images folder model index should be valid"))
    {
        delete context;
        return 1;
    }
    if (Expect(model.data(imagesIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("Images"),
               "images folder should expose display name"))
    {
        delete context;
        return 1;
    }
    if (Expect(model.data(imagesIndex,
                          xq::presentation::DataHierarchyModel::NodeIdRole)
                   .toString() == QStringLiteral("images"),
               "images folder should expose stable node id"))
    {
        delete context;
        return 1;
    }

    if (Expect(model.rowCount(imagesIndex) == 1,
               "images folder should expose one imported data child"))
    {
        delete context;
        return 1;
    }
    QModelIndex imageIndex = model.index(0, 0, imagesIndex);
    if (Expect(imageIndex.isValid(),
               "imported image model index should be valid"))
    {
        delete context;
        return 1;
    }
    if (Expect(model.parent(imageIndex) == imagesIndex,
               "imported image parent should be images folder"))
    {
        delete context;
        return 1;
    }
    if (Expect(model.data(imageIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("CTA A"),
               "imported image should expose display name"))
    {
        delete context;
        return 1;
    }
    if (Expect(model.data(imageIndex,
                          xq::presentation::DataHierarchyModel::NodeIdRole)
                   .toString() == QStringLiteral("data-image-001"),
               "imported image should expose stable node id"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("image-001"),
                   QStringLiteral("Renamed CTA"),
                   &errorMessage),
               "rename should succeed"))
    {
        delete context;
        return 1;
    }
    imagesIndex = model.index(0, 0, QModelIndex());
    QModelIndex renamedImageIndex = model.index(0, 0, imagesIndex);
    if (Expect(model.data(renamedImageIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("Renamed CTA"),
               "model should refresh renamed image display name"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RemoveEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "remove should succeed"))
    {
        delete context;
        return 1;
    }
    imagesIndex = model.index(0, 0, QModelIndex());
    if (Expect(model.rowCount(imagesIndex) == 0,
               "model should refresh removed image row"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
