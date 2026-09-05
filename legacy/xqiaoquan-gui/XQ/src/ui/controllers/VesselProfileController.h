#ifndef XQ_UI_CONTROLLERS_VESSEL_PROFILE_CONTROLLER_H
#define XQ_UI_CONTROLLERS_VESSEL_PROFILE_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQScaleSlot.h"
#include "core/asset/AssetRecord.h"
#include "core/command/XQProjectCommands.h"
#include "services/profile/VesselProfileAssembler.h"
#include "services/profile/VesselProfileImporter.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class XQCommandStack;
class XQDataNode;
class XQProject;

// Project-aware preparation/commit boundary for VesselProfile creation. It
// snapshots immutable XQ payload values before pure service work, then rechecks
// payload identity, revision, stale state and asset identity before submitting
// the existing atomic ProjectNodeBatchCommand.
class VesselProfileController {
private:
    struct ProjectGuard {
        const XQProject* project = nullptr;
        std::uint64_t lifecycleEpoch = 0;
    };

    struct AssetGuard {
        AssetId id;
        AssetCategory category = AssetCategory::ExternalSource;
        AssetKind kind = AssetKind::VesselProfile;
        std::string fingerprint;
    };

    struct SourceGuard {
        DerivationInputStamp stamp;
        XQDomainType domain = XQDomainType::Unknown;
        std::shared_ptr<XQPayload> payloadIdentity;
        std::optional<ScaleSlot> scaleSlot;
    };

public:
    VesselProfileController(XQProject* project, XQCommandStack* stack);

    enum class Status {
        Ok,
        NullContext,
        ProjectNotOpen,
        InvalidIntent,
        TargetExists,
        SourceNotFound,
        SourceTypeMismatch,
        SourceStale,
        SourcePayloadInvalid,
        SourceAssetInvalid,
        EvidenceAssetInvalid,
        AssemblyFailed,
        ImportFailed,
        SourceChanged,
        CommitRejected
    };

    struct OutputIntent {
        NodeId newProfileId;
        std::string name;
        std::optional<ScaleSlot> scaleSlot;

        // At most one may be set. A new record is registered and bound in the
        // same command; an existing record is only bound.
        std::optional<AssetRecord> newProfileAsset;
        std::optional<AssetId> existingProfileAsset;
    };

    struct ContourIntent {
        OutputIntent output;
        NodeId pathNode;
        NodeId contourGroupNode;
        std::string frameOfReferenceId;
        VesselProfileAssembler::Options options;
    };

    struct ImportedGoldIntent {
        OutputIntent output;
        VesselProfileImporter::Request request;

        // Optional external gold artifact lineage parent. It is legal only
        // when the produced Profile is bound to an Asset.
        std::optional<AssetId> evidenceAsset;
    };

    struct PreparedCommand {
        Status status = Status::InvalidIntent;
        std::vector<VesselProfileAssembler::Issue> assemblyIssues;
        VesselProfileImporter::Status importStatus =
            VesselProfileImporter::Status::ProfileValidationFailed;
        VesselProfileValidationResult importValidation;

        bool ok() const
        {
            return status == Status::Ok && batch_ != nullptr;
        }

    private:
        friend class VesselProfileController;
        std::unique_ptr<ProjectNodeBatchSpec> batch_;
        ProjectGuard projectGuard_;
        std::optional<AssetGuard> targetAssetGuard_;
        std::vector<SourceGuard> sourceGuards_;
    };

    // Owner-thread capture: reads Scene/AssetRegistry once, rejects stale
    // sources, and copies all XQ values needed by the worker stage.
    struct CapturedContourInput {
        Status status = Status::InvalidIntent;

        bool ok() const { return status == Status::Ok; }

    private:
        friend class VesselProfileController;
        OutputIntent output_;
        VesselProfileAssembler::Input input_;
        VesselProfileAssembler::Options options_;
        ProjectGuard projectGuard_;
        std::optional<AssetGuard> targetAssetGuard_;
        std::vector<SourceGuard> sourceGuards_;
    };

    struct CapturedImportedGoldInput {
        Status status = Status::InvalidIntent;

        bool ok() const { return status == Status::Ok; }

    private:
        friend class VesselProfileController;
        OutputIntent output_;
        VesselProfileImporter::Request request_;
        ProjectGuard projectGuard_;
        std::optional<AssetGuard> targetAssetGuard_;
        std::vector<SourceGuard> sourceGuards_;
        std::vector<AssetId> additionalAssetSources_;
    };

    CapturedContourInput captureFromContours(const ContourIntent& intent) const;
    CapturedImportedGoldInput captureImportedGold(
        const ImportedGoldIntent& intent) const;

    // Worker-safe computation: these static functions receive only captured
    // value snapshots and never read XQProject/XQScene.
    static PreparedCommand computeFromContours(CapturedContourInput captured);
    static PreparedCommand computeImportedGold(CapturedImportedGoldInput captured);

    PreparedCommand prepareFromContours(const ContourIntent& intent) const;
    PreparedCommand prepareImportedGold(const ImportedGoldIntent& intent) const;

    // Owner-thread commit. A prepared command is single-use.
    Status commitPrepared(PreparedCommand prepared) const;

    Status assembleFromContours(const ContourIntent& intent) const;
    Status importGold(const ImportedGoldIntent& intent) const;

private:
    Status validateOutput(
        const OutputIntent& output,
        std::optional<AssetGuard>* existingTargetGuard) const;
    Status captureSource(const NodeId& id,
                         XQDomainType expectedDomain,
                         SourceGuard* guard,
                         std::shared_ptr<XQPayload>* snapshot) const;
    bool sourceStillCurrent(const SourceGuard& guard) const;
    bool targetAssetStillCurrent(const AssetGuard& guard) const;
    static std::optional<ScaleSlot> resolveScaleSlot(
        const std::optional<ScaleSlot>& explicitScale,
        const std::vector<SourceGuard>& sourceGuards);
    static PreparedCommand buildPrepared(
        const OutputIntent& output,
        VesselProfileV1 profile,
        ProjectGuard projectGuard,
        std::optional<AssetGuard> targetAssetGuard,
        std::vector<SourceGuard> guards,
        const std::vector<AssetId>& additionalAssetSources);

    XQProject* project_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_VESSEL_PROFILE_CONTROLLER_H
