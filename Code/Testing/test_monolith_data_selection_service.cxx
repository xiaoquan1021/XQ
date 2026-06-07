#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataSelectionService.h"

#include <QCoreApplication>
#include <QObject>

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

xq::core::DataCatalogEntry MakeEntry(const QString& id,
                                     const QString& displayName)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = id;
    entry.DisplayName = displayName;
    entry.SourcePath = QStringLiteral("C:/studies/") + id;
    entry.Modality = QStringLiteral("CT");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return entry;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::DataCatalogService catalog;
    xq::core::DataHierarchyService hierarchy;
    QString errorMessage;

    if (Expect(catalog.RegisterEntry(MakeEntry(QStringLiteral("image-001"),
                                               QStringLiteral("CTA A")),
                                     &errorMessage),
               "first catalog entry should register"))
        return 1;
    if (Expect(catalog.RegisterEntry(MakeEntry(QStringLiteral("image-002"),
                                               QStringLiteral("CTA B")),
                                     &errorMessage),
               "second catalog entry should register"))
        return 1;
    if (Expect(hierarchy.AddFolder(QStringLiteral("images"),
                                   hierarchy.RootId(),
                                   QStringLiteral("Images"),
                                   &errorMessage),
               "images hierarchy folder should register"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("data-image-001"),
                                      QStringLiteral("images"),
                                      QStringLiteral("image-001"),
                                      QStringLiteral("CTA A"),
                                      &errorMessage),
               "first hierarchy data node should register"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("data-image-002"),
                                      QStringLiteral("images"),
                                      QStringLiteral("image-002"),
                                      QStringLiteral("CTA B"),
                                      &errorMessage),
               "second hierarchy data node should register"))
        return 1;

    xq::core::DataSelectionService selection(catalog, hierarchy);
    int selectionSignals = 0;
    QString lastHierarchyNodeId;
    QString lastCatalogEntryId;
    QObject::connect(&selection,
                     &xq::core::DataSelectionService::SelectionChanged,
                     [&selectionSignals,
                      &lastHierarchyNodeId,
                      &lastCatalogEntryId](const QString& hierarchyNodeId,
                                           const QString& catalogEntryId) {
                         ++selectionSignals;
                         lastHierarchyNodeId = hierarchyNodeId;
                         lastCatalogEntryId = catalogEntryId;
                     });

    if (Expect(!selection.HasSelection(),
               "new data selection should start empty"))
        return 1;
    if (Expect(selection.SelectedHierarchyNodeId().isEmpty(),
               "new data selection should not have a hierarchy node"))
        return 1;
    if (Expect(selection.SelectedCatalogEntryId().isEmpty(),
               "new data selection should not have a catalog entry"))
        return 1;

    if (Expect(selection.SelectHierarchyNode(QStringLiteral("data-image-001"),
                                             &errorMessage),
               "selecting a hierarchy data node should succeed"))
        return 1;
    if (Expect(selection.SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-001"),
               "hierarchy node selection should be stored"))
        return 1;
    if (Expect(selection.SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "hierarchy node selection should resolve catalog entry id"))
        return 1;
    if (Expect(selectionSignals == 1,
               "selecting a hierarchy data node should emit once"))
        return 1;
    if (Expect(lastHierarchyNodeId == QStringLiteral("data-image-001") &&
                   lastCatalogEntryId == QStringLiteral("image-001"),
               "selection signal should include selected ids"))
        return 1;

    if (Expect(selection.SelectHierarchyNode(QStringLiteral("data-image-001"),
                                             &errorMessage),
               "selecting the same hierarchy node should still succeed"))
        return 1;
    if (Expect(selectionSignals == 1,
               "selecting the same data should be a no-op"))
        return 1;

    if (Expect(selection.SelectCatalogEntry(QStringLiteral("image-002"),
                                            &errorMessage),
               "selecting by catalog entry should succeed"))
        return 1;
    if (Expect(selection.SelectedCatalogEntryId() == QStringLiteral("image-002"),
               "catalog selection should store catalog entry id"))
        return 1;
    if (Expect(selection.SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-002"),
               "catalog selection should resolve hierarchy node id"))
        return 1;
    if (Expect(selectionSignals == 2,
               "selecting another data entry should emit once"))
        return 1;

    errorMessage.clear();
    if (Expect(!selection.SelectHierarchyNode(QStringLiteral("missing-node"),
                                              &errorMessage),
               "missing hierarchy node selection should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("hierarchy")),
               "missing hierarchy node selection should provide useful error"))
        return 1;
    if (Expect(selection.SelectedCatalogEntryId() == QStringLiteral("image-002"),
               "failed hierarchy selection should not mutate catalog id"))
        return 1;
    if (Expect(selectionSignals == 2,
               "failed hierarchy selection should not emit"))
        return 1;

    errorMessage.clear();
    if (Expect(!selection.SelectCatalogEntry(QStringLiteral("missing-entry"),
                                             &errorMessage),
               "missing catalog entry selection should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("catalog")),
               "missing catalog entry selection should provide useful error"))
        return 1;
    if (Expect(selection.SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-002"),
               "failed catalog selection should not mutate hierarchy id"))
        return 1;

    selection.Clear();
    if (Expect(!selection.HasSelection(),
               "clear should remove data selection"))
        return 1;
    if (Expect(selectionSignals == 3,
               "clear should emit once"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->DataSelection() != nullptr,
               "ApplicationContext should expose DataSelectionService"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
