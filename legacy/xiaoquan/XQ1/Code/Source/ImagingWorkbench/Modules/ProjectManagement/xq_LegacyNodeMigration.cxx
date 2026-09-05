#include "xq_LegacyNodeMigration.h"

#include "xq_LegacyImporter.h"

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkPointSet.h>

#include <vector>

namespace
{

bool IsLegacyImportedPointSet(const mitk::DataNode* node, const char* typeName, const char* fileProperty)
{
    if (!node || dynamic_cast<const mitk::PointSet*>(node->GetData()) == nullptr)
        return false;

    std::string typeValue;
    node->GetStringProperty("sv.type", typeValue);
    if (typeValue != typeName)
        return false;

    std::string filePath;
    node->GetStringProperty(fileProperty, filePath);
    return !filePath.empty();
}

void CopyNodeState(mitk::DataNode* destination, const mitk::DataNode* source)
{
    if (!destination || !source)
        return;

    bool visible = true;
    source->GetVisibility(visible, nullptr);
    destination->SetVisibility(visible);

    float color[3] = {1.0f, 1.0f, 1.0f};
    source->GetColor(color);
    destination->SetColor(color[0], color[1], color[2]);

    float opacity = 1.0f;
    source->GetOpacity(opacity, nullptr);
    destination->SetOpacity(opacity);
}

} // namespace

int xq_LegacyNodeMigration::UpgradeImportedLegacyNodes(mitk::DataStorage* dataStorage)
{
    if (!dataStorage)
        return 0;

    xq_LegacyImporter importer;
    int upgradedCount = 0;

    auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNull())
            continue;

        if (IsLegacyImportedPointSet(node, "Path", "sv.path.file"))
        {
            std::string filePath;
            node->GetStringProperty("sv.path.file", filePath);
            auto upgradedNode = importer.ParsePathFile(filePath);
            if (upgradedNode.IsNotNull() && upgradedNode->GetData())
            {
                node->SetData(upgradedNode->GetData());
                CopyNodeState(node, upgradedNode);
                ++upgradedCount;
            }
            continue;
        }

        if (IsLegacyImportedPointSet(node, "ContourGroup", "sv.contourgroup.file"))
        {
            std::string filePath;
            node->GetStringProperty("sv.contourgroup.file", filePath);
            auto upgradedNode = importer.ParseContourGroupFile(filePath);
            if (upgradedNode.IsNotNull() && upgradedNode->GetData())
            {
                node->SetData(upgradedNode->GetData());
                CopyNodeState(node, upgradedNode);
                ++upgradedCount;
            }
        }
    }

    return upgradedCount;
}

int xq_LegacyNodeMigration::ReparentIntoCategoryFolders(mitk::DataStorage* dataStorage)
{
    if (!dataStorage)
        return 0;

    // Collect first, mutate second — re-parenting via Remove+Add invalidates
    // the live iterator otherwise.
    struct Entry
    {
        mitk::DataNode::Pointer                  node;
        xq::pipeline::Stage                      stage = xq::pipeline::Stage::Unknown;
    };
    std::vector<Entry> pending;
    pending.reserve(64);

    auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNull())
            continue;

        std::string stageStr;
        if (!node->GetStringProperty(xq::pipeline::kStageProperty, stageStr) ||
            stageStr.empty())
            continue;

        // Resolve stage via HasStage to avoid adding a new ParseStage helper.
        xq::pipeline::Stage stage = xq::pipeline::Stage::Unknown;
        for (auto s : {xq::pipeline::Stage::Path,
                       xq::pipeline::Stage::ContourGroup,
                       xq::pipeline::Stage::Model,
                       xq::pipeline::Stage::VolumeMesh,
                       xq::pipeline::Stage::SimulationPrep})
        {
            if (xq::pipeline::HasStage(node, s))
            {
                stage = s;
                break;
            }
        }
        if (stage == xq::pipeline::Stage::Unknown)
            continue;

        pending.push_back({node, stage});
    }

    int moved = 0;
    for (const auto& e : pending)
    {
        const auto targetFolder = xq::pipeline::FindCategoryFolder(
            dataStorage, e.stage, e.node.GetPointer());
        if (targetFolder.IsNull())
            continue;

        // Already attached to the target folder? Skip.
        auto parents = dataStorage->GetSources(e.node);
        bool alreadyOk = false;
        if (parents)
        {
            for (auto pit = parents->Begin(); pit != parents->End(); ++pit)
            {
                if (pit->Value().GetPointer() == targetFolder.GetPointer())
                {
                    alreadyOk = true;
                    break;
                }
            }
        }
        if (alreadyOk)
            continue;

        // MITK's DataStorage has no "reparent" API. Cache children first so
        // the subtree survives the Remove+Add round-trip.
        auto children = dataStorage->GetDerivations(e.node);
        std::vector<mitk::DataNode::Pointer> childList;
        if (children)
            for (auto cit = children->Begin(); cit != children->End(); ++cit)
                childList.push_back(cit->Value());

        dataStorage->Remove(e.node);
        dataStorage->Add(e.node, targetFolder);
        for (const auto& child : childList)
        {
            // Child is still in the storage; nothing to do.
            (void)child;
        }
        ++moved;
    }
    return moved;
}
