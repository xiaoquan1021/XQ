#include "Core/xq_DataNodeRegistryService.h"

#include "Core/xq_ApplicationContext.h"

#include <QCoreApplication>
#include <QStringList>

#include <mitkDataNode.h>

#include <iostream>
#include <memory>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

mitk::DataNode::Pointer MakeNode(const std::string& name)
{
    auto node = mitk::DataNode::New();
    node->SetName(name);
    return node;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::DataNodeRegistryService registry;
    if (Expect(registry.CatalogEntryIds().isEmpty(),
               "new data node registry should start empty"))
        return 1;
    if (Expect(registry.FindNode(QStringLiteral("image-001")).IsNull(),
               "new data node registry should not find unknown nodes"))
        return 1;

    QString message;
    auto node = MakeNode("CTA");
    if (Expect(!registry.BindNode(QString(), node, &message),
               "registry should reject empty catalog ids"))
        return 1;
    if (Expect(message ==
                   QStringLiteral("Data node catalog entry id is required."),
               "empty catalog id should use registry diagnostic"))
        return 1;

    if (Expect(!registry.BindNode(QStringLiteral("image-001"),
                                  nullptr,
                                  &message),
               "registry should reject null data nodes"))
        return 1;
    if (Expect(message == QStringLiteral("Data node is required."),
               "null data node should use registry diagnostic"))
        return 1;

    if (Expect(registry.BindNode(QStringLiteral(" image-001 "),
                                 node,
                                 &message),
               "registry should bind valid data nodes"))
        return 1;
    if (Expect(message.isEmpty(),
               "valid bind should clear registry diagnostic"))
        return 1;
    if (Expect(registry.CatalogEntryIds() ==
                   QStringList{QStringLiteral("image-001")},
               "registry should list normalized catalog id"))
        return 1;
    if (Expect(registry.FindNode(QStringLiteral("image-001")).GetPointer() ==
                   node.GetPointer(),
               "registry should find bound data node"))
        return 1;

    auto replacement = MakeNode("CTA replacement");
    if (Expect(registry.BindNode(QStringLiteral("image-001"),
                                 replacement,
                                 &message),
               "registry should allow rebinding catalog ids"))
        return 1;
    if (Expect(registry.CatalogEntryIds().size() == 1,
               "registry rebind should not duplicate catalog ids"))
        return 1;
    if (Expect(registry.FindNode(QStringLiteral("image-001")).GetPointer() ==
                   replacement.GetPointer(),
               "registry rebind should replace the data node"))
        return 1;

    if (Expect(registry.RemoveNode(QStringLiteral("image-001"), &message),
               "registry should remove bound data nodes"))
        return 1;
    if (Expect(registry.FindNode(QStringLiteral("image-001")).IsNull(),
               "removed registry node should not be found"))
        return 1;
    if (Expect(registry.CatalogEntryIds().isEmpty(),
               "removed registry id should leave list empty"))
        return 1;

    if (Expect(!registry.RemoveNode(QStringLiteral("image-001"), &message),
               "registry should reject missing removes"))
        return 1;
    if (Expect(message ==
                   QStringLiteral("Data node binding was not found."),
               "missing remove should use registry diagnostic"))
        return 1;

    std::unique_ptr<xq::core::ApplicationContext> context(
        xq::core::ApplicationContext::CreateDefault());
    if (Expect(context->DataNodes() != nullptr,
               "ApplicationContext should expose DataNodeRegistryService"))
        return 1;
    if (Expect(context->DataNodes()->CatalogEntryIds().isEmpty(),
               "ApplicationContext data node registry should start empty"))
        return 1;

    return 0;
}
