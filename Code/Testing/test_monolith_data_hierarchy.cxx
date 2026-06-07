#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataHierarchyService.h"

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

    xq::core::DataHierarchyService hierarchy;
    if (Expect(hierarchy.RootId() == QStringLiteral("root"),
               "new hierarchy should expose a stable root id"))
        return 1;
    if (Expect(hierarchy.FindNode(hierarchy.RootId()) != nullptr,
               "new hierarchy should contain the root node"))
        return 1;
    if (Expect(hierarchy.ChildrenOf(hierarchy.RootId()).isEmpty(),
               "new hierarchy root should start without children"))
        return 1;

    QString errorMessage;
    if (Expect(hierarchy.AddFolder(QStringLiteral("images"),
                                   hierarchy.RootId(),
                                   QStringLiteral("Images"),
                                   &errorMessage),
               "valid folder should be added under root"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("cta-node"),
                                      QStringLiteral("images"),
                                      QStringLiteral("image-001"),
                                      QStringLiteral("CTA"),
                                      &errorMessage),
               "valid data entry node should be added under folder"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("mr-node"),
                                      QStringLiteral("images"),
                                      QStringLiteral("dicom-002"),
                                      QStringLiteral("MR"),
                                      &errorMessage),
               "second data entry node should be added under folder"))
        return 1;

    const auto rootChildren = hierarchy.ChildrenOf(hierarchy.RootId());
    if (Expect(rootChildren.size() == 1 &&
                   rootChildren.at(0).Id == QStringLiteral("images"),
               "root children should preserve folder order"))
        return 1;

    const auto imageChildren = hierarchy.ChildrenOf(QStringLiteral("images"));
    if (Expect(imageChildren.size() == 2,
               "folder should expose ordered children"))
        return 1;
    if (Expect(imageChildren.at(0).Id == QStringLiteral("cta-node"),
               "first data node order should be preserved"))
        return 1;
    if (Expect(imageChildren.at(1).DataCatalogEntryId ==
                   QStringLiteral("dicom-002"),
               "data node should preserve catalog entry id"))
        return 1;
    if (Expect(imageChildren.at(0).Kind ==
                   xq::core::DataHierarchyNodeKind::DataEntry,
               "data node kind should be DataEntry"))
        return 1;

    errorMessage.clear();
    if (Expect(!hierarchy.AddFolder(QStringLiteral("images"),
                                    hierarchy.RootId(),
                                    QStringLiteral("Duplicate"),
                                    &errorMessage),
               "duplicate node id should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("Duplicate")),
               "duplicate node id should produce a useful error"))
        return 1;
    if (Expect(hierarchy.ChildrenOf(hierarchy.RootId()).size() == 1,
               "duplicate node id should not mutate hierarchy"))
        return 1;

    errorMessage.clear();
    if (Expect(!hierarchy.AddFolder(QStringLiteral("orphan"),
                                    QStringLiteral("missing-parent"),
                                    QStringLiteral("Orphan"),
                                    &errorMessage),
               "missing parent id should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("parent")),
               "missing parent failure should produce a useful error"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->DataHierarchy() != nullptr,
               "ApplicationContext should expose DataHierarchyService"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()->RootId() == QStringLiteral("root"),
               "context DataHierarchyService should expose stable root"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
