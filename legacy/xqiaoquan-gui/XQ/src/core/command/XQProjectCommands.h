#ifndef XQ_CORE_COMMAND_XQ_PROJECT_COMMANDS_H
#define XQ_CORE_COMMAND_XQ_PROJECT_COMMANDS_H

#include "core/XQDataNode.h"
#include "core/XQDerivationStamp.h"
#include "core/XQProject.h"
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRelation.h"
#include "core/command/XQCommand.h"

#include <optional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xq {

// Prepared atomic addition of one authoritative project node. The node itself
// must be unbound; bindAsset describes the binding committed together with the
// optional new AssetRecord. Source stamps double as Scene parents and freshness
// expectations captured by the producing service.
struct ProjectNodeBatchSpec {
    explicit ProjectNodeBatchSpec(XQDataNode preparedNode)
        : node(std::move(preparedNode))
    {
    }

    XQDataNode node;
    std::optional<AssetRecord> assetToRegister;
    std::optional<AssetId> bindAsset;
    std::vector<DerivationInputStamp> sources;

    // Asset-only lineage parents (for example an ImportedGold artifact that
    // has no fabricated Scene node).
    std::vector<AssetId> additionalAssetSources;
};

class ProjectNodeBatchCommand : public XQCommand {
public:
    ProjectNodeBatchCommand(
        XQProject* project,
        ProjectNodeBatchSpec spec,
        std::string label = "Add project node batch");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    bool validate_prepared_state() const;
    void rollback_applied_steps();

    XQProject* project_;
    ProjectNodeBatchSpec spec_;
    bool asset_registered_;
    bool node_inserted_;
    std::vector<AssetRelation> added_asset_relations_;
    std::string label_;
};

// Commits several ProjectNodeBatchSpec values as one undo entry. Later specs
// may depend on nodes inserted by earlier specs; any failure rolls every
// already-applied batch back before execute() returns false.
class ProjectNodeBundleCommand : public XQCommand {
public:
    ProjectNodeBundleCommand(
        XQProject* project,
        std::vector<ProjectNodeBatchSpec> specs,
        std::string label = "Add project node bundle");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    XQProject* project_;
    std::vector<std::unique_ptr<ProjectNodeBatchCommand>> commands_;
    std::size_t appliedCount_;
    std::string label_;
};

} // namespace xq

#endif // XQ_CORE_COMMAND_XQ_PROJECT_COMMANDS_H
