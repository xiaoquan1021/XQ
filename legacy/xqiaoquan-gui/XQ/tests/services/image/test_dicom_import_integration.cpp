#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQProject.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "services/image/DicomImportService.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t derivedRelationCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

bool close(double left, double right)
{
    return std::abs(left - right) < 1e-5;
}

std::string validFingerprint(char digit = 'a')
{
    return std::string("dicom-series-v1:sha256:") + std::string(64, digit);
}

bool fingerprintHasExpectedFormat(const std::string& fingerprint)
{
    const std::string prefix = "dicom-series-v1:sha256:";
    return fingerprint.size() == prefix.size() + 64
        && fingerprint.compare(0, prefix.size(), prefix) == 0
        && fingerprint.find_first_not_of(
               "0123456789abcdefABCDEF", prefix.size()) == std::string::npos;
}

bool diagnosticsContainText(const std::vector<xq::Diagnostic>& diagnostics,
                            const std::string& text)
{
    for (std::vector<xq::Diagnostic>::const_iterator it = diagnostics.begin();
         it != diagnostics.end(); ++it) {
        if (it->code().find(text) != std::string::npos
            || it->message().find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool diagnosticsContainCode(const std::vector<xq::Diagnostic>& diagnostics,
                            const std::string& code)
{
    for (std::vector<xq::Diagnostic>::const_iterator it = diagnostics.begin();
         it != diagnostics.end(); ++it) {
        if (it->code() == code) {
            return true;
        }
    }
    return false;
}

xq::DicomSeriesReadResult makeFakeReadResult()
{
    const int dims[3] = {2, 2, 2};
    const std::vector<float> values = {
        -100.0f, -90.0f, -80.0f, -70.0f,
        -60.0f, -50.0f, -40.0f, -30.0f
    };
    std::vector<std::uint8_t> bytes(values.size() * sizeof(float));
    std::memcpy(bytes.data(), values.data(), bytes.size());

    xq::DicomSeriesReadResult result;
    result.status = xq::DicomSeriesStatus::Ok;
    result.descriptor.identity = {"1.2.3.1", "1.2.3.2", "1.2.3.3"};
    result.descriptor.modality = xq::ImageModality::CT;
    result.descriptor.dimensions[0] = dims[0];
    result.descriptor.dimensions[1] = dims[1];
    result.descriptor.dimensions[2] = dims[2];
    result.descriptor.sliceCount = 2;
    result.descriptor.safeDisplayName = "PATIENT_SECRET unsafe adapter name";
    result.descriptor.contentFingerprint = validFingerprint();

    xq::ImageGeometry geometry{};
    geometry.dimensions[0] = dims[0];
    geometry.dimensions[1] = dims[1];
    geometry.dimensions[2] = dims[2];
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 2.0;
    geometry.direction[0][0] = 1.0;
    geometry.direction[1][1] = 1.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    result.volume.setGeometry(geometry);
    result.volume.setScalarType(xq::ScalarType::Float32);
    result.volume.setComponentCount(1);
    result.volume.setIntensityRange({-100.0, -30.0});
    result.volume.setModality(xq::ImageModality::CT);
    result.volume.setDicomIdentity(result.descriptor.identity);
    result.volume.setRescaleSlope(1.0);
    result.volume.setRescaleIntercept(0.0);
    result.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::Float32, dims, 1, std::move(bytes));
    return result;
}

class FakeDicomSeriesReader final : public xq::IDicomSeriesReader {
public:
    xq::DicomSeriesDiscoveryResult discover(const std::string&) override
    {
        return xq::DicomSeriesDiscoveryResult{};
    }

    xq::DicomSeriesReadResult read(const std::string& directory,
                                   const std::string& seriesInstanceUid) override
    {
        ++readCount;
        lastDirectory = directory;
        lastSeriesUid = seriesInstanceUid;
        if (failureStatus != xq::DicomSeriesStatus::Ok) {
            xq::DicomSeriesReadResult failed;
            failed.status = failureStatus;
            failed.diagnostics.emplace_back(
                xq::DiagnosticSeverity::Error,
                failureDiagnosticCode,
                failureDiagnosticMessage);
            return failed;
        }
        if (useFixedResult) {
            return fixedResult;
        }
        xq::DicomSeriesReadResult result = makeFakeReadResult();
        result.descriptor.contentFingerprint = fingerprint;
        result.diagnostics = readDiagnostics;
        return result;
    }

    int readCount = 0;
    xq::DicomSeriesStatus failureStatus = xq::DicomSeriesStatus::Ok;
    std::string fingerprint = validFingerprint();
    std::vector<xq::Diagnostic> readDiagnostics;
    std::string failureDiagnosticCode = "dicom.fake_failure";
    std::string failureDiagnosticMessage =
        "The requested synthetic series is unavailable.";
    std::string lastDirectory;
    std::string lastSeriesUid;
    bool useFixedResult = false;
    xq::DicomSeriesReadResult fixedResult;
};

int testRealAdapterPrepareCommitUndoRedo()
{
    const std::filesystem::path source =
        std::filesystem::path(XQ_DICOM_FIXTURE_ROOT) / "regular-oblique";
    const xq::NodeId nodeId(81001);
    const xq::AssetId assetId(91001);

    xq::GdcmItkDicomSeriesReader reader;
    xq::DicomImportRequest request;
    request.sourceDirectory = source.string();
    request.sourceRelPath = "dicom/regular-oblique";
    request.seriesInstanceUid = "1.2.826.0.1.3680043.10.543.101";
    request.nodeId = nodeId;
    request.assetId = assetId;

    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open import project", __LINE__);
    }
    xq::DicomImportResult prepared =
        xq::DicomImportService::prepare(reader, request);
    if (!prepared.ok() || nodeCount(project.scene()) != 0
        || project.assetRegistry().assetCount() != 0) {
        return fail("DICOM import preparation succeeds without project side effects", __LINE__);
    }

    const xq::ProjectNodeBatchSpec& batch = prepared.batchSpec.value();
    const std::shared_ptr<xq::XQImageVolumePayload> payload =
        std::dynamic_pointer_cast<xq::XQImageVolumePayload>(batch.node.payload());
    if (batch.node.id() != nodeId || batch.node.domainType() != xq::XQDomainType::Image
        || batch.node.hasAssetId() || !batch.node.hasScaleSlot()
        || batch.node.scaleSlot().value() != xq::ScaleSlot::Organ
        || payload == nullptr || payload->volume().bufferHandle() != nullptr
        || batch.node.display_name() != "CT 4x3x4 (4 slices, UID ...10.543.101)"
        || !batch.assetToRegister.has_value() || !batch.bindAsset.has_value()) {
        return fail("prepared node is a safe metadata-only Organ image", __LINE__);
    }

    const xq::AssetRecord& asset = batch.assetToRegister.value();
    if (asset.id != assetId || batch.bindAsset.value() != assetId
        || asset.category != xq::AssetCategory::ExternalSource
        || asset.kind != xq::AssetKind::Image
        || !std::filesystem::path(asset.sourceAbsPath).is_absolute()
        || asset.sourceRelPath != request.sourceRelPath
        || !asset.hasDicom || asset.dicom.seriesInstanceUid != request.seriesInstanceUid
        || !fingerprintHasExpectedFormat(asset.contentFingerprint)
        || !asset.hasGeometry || asset.geometry.dimensions[2] != 4
        || !asset.blobs.empty()) {
        return fail("prepared asset preserves external DICOM identity and locator", __LINE__);
    }
    const std::string expectedFingerprint = asset.contentFingerprint;

    const xq::VoxelMeta meta = prepared.residentSource->meta();
    xq::VoxelLease whole = prepared.residentSource->acquire_whole();
    if (!meta.valid || meta.type != xq::ScalarType::Float32
        || meta.dims[0] != 4 || meta.dims[1] != 3 || meta.dims[2] != 4
        || !whole.view().valid || !close(whole.view().scalarAt(0), -824.0)
        || !close(whole.view().scalarAt(47), -682.0)) {
        return fail("prepared resident source exposes canonical rescaled voxels", __LINE__);
    }

    xq::XQCommandStack stack;
    std::unique_ptr<xq::XQCommand> command(new xq::ProjectNodeBatchCommand(
        &project, std::move(prepared.batchSpec.value()), "Import DICOM series"));
    if (!stack.push(std::move(command))) {
        return fail("prepared DICOM project batch commits", __LINE__);
    }
    const xq::XQDataNode* liveNode = project.scene().find(nodeId);
    const xq::AssetRecord* liveAsset = project.assetRegistry().find(assetId);
    if (liveNode == nullptr || !liveNode->hasAssetId() || liveNode->assetId() != assetId
        || liveAsset == nullptr || liveAsset->contentFingerprint != expectedFingerprint
        || nodeCount(project.scene()) != 1 || project.assetRegistry().assetCount() != 1) {
        return fail("commit atomically publishes image node, asset and binding", __LINE__);
    }

    if (!stack.undo() || project.scene().find(nodeId) != nullptr
        || project.assetRegistry().find(assetId) != nullptr
        || nodeCount(project.scene()) != 0 || project.assetRegistry().assetCount() != 0
        || !whole.view().valid || !close(whole.view().scalarAt(0), -824.0)) {
        return fail("undo removes project state without invalidating prepared residency", __LINE__);
    }
    if (!stack.redo() || project.scene().find(nodeId) == nullptr
        || project.assetRegistry().find(assetId) == nullptr) {
        return fail("redo restores the same DICOM node and asset identity", __LINE__);
    }

    xq::DicomImportResult duplicate =
        xq::DicomImportService::prepare(reader, request);
    if (!duplicate.ok()) {
        return fail("duplicate proposal remains pure and preparable", __LINE__);
    }
    const std::size_t undoBefore = stack.undo_count();
    std::unique_ptr<xq::XQCommand> duplicateCommand(new xq::ProjectNodeBatchCommand(
        &project, std::move(duplicate.batchSpec.value())));
    if (stack.push(std::move(duplicateCommand))
        || stack.undo_count() != undoBefore || nodeCount(project.scene()) != 1
        || project.assetRegistry().assetCount() != 1) {
        return fail("duplicate ids fail atomically without a new undo entry", __LINE__);
    }
    return 0;
}

int testSafeNameAndFailurePropagation()
{
    FakeDicomSeriesReader reader;
    xq::DicomImportRequest request;
    request.sourceDirectory = "C:/PATIENT_SECRET/source";
    request.sourceRelPath = "source";
    request.seriesInstanceUid = "1.2.3.2";
    request.nodeId = xq::NodeId(1);
    request.assetId = xq::AssetId(2);
    reader.readDiagnostics.emplace_back(
        xq::DiagnosticSeverity::Warning,
        "PATIENT_SECRET.adapter_code",
        "PATIENT_SECRET adapter message");

    xq::DicomImportResult prepared =
        xq::DicomImportService::prepare(reader, request);
    if (!prepared.ok() || reader.readCount != 1
        || reader.lastSeriesUid != request.seriesInstanceUid
        || prepared.batchSpec->node.display_name()
            != "CT 2x2x2 (2 slices, UID 1.2.3.2)"
        || prepared.batchSpec->node.display_name().find("PATIENT_SECRET")
            != std::string::npos
        || prepared.batchSpec->assetToRegister->displayName.find("unsafe")
            != std::string::npos
        || diagnosticsContainText(prepared.diagnostics, "PATIENT_SECRET")
        || !diagnosticsContainCode(
            prepared.diagnostics, "dicom.import.reader_warning")) {
        return fail("service rebuilds safe display and diagnostic text", __LINE__);
    }

    reader.failureStatus = xq::DicomSeriesStatus::SeriesNotFound;
    reader.failureDiagnosticCode = "PATIENT_SECRET.failure_code";
    reader.failureDiagnosticMessage = "PATIENT_SECRET failure message";
    xq::DicomImportResult missing =
        xq::DicomImportService::prepare(reader, request);
    if (missing.status != xq::DicomSeriesStatus::SeriesNotFound
        || missing.batchSpec.has_value() || missing.residentSource != nullptr
        || missing.diagnostics.empty()
        || diagnosticsContainText(missing.diagnostics, "PATIENT_SECRET")
        || !diagnosticsContainCode(
            missing.diagnostics, "dicom.import.series_not_found")) {
        return fail("reader failure maps to fixed non-PHI diagnostics", __LINE__);
    }

    FakeDicomSeriesReader invalidReader;
    request.nodeId = xq::NodeId::invalid();
    xq::DicomImportResult invalid =
        xq::DicomImportService::prepare(invalidReader, request);
    if (invalid.status != xq::DicomSeriesStatus::InvalidArgument
        || invalidReader.readCount != 0 || invalid.batchSpec.has_value()
        || invalid.residentSource != nullptr) {
        return fail("invalid stable identity is rejected before reading", __LINE__);
    }
    return 0;
}

int testRootedRelativeLocatorsRejected()
{
    FakeDicomSeriesReader reader;
    xq::DicomImportRequest request;
    request.sourceDirectory = "C:/synthetic/source";
    request.seriesInstanceUid = "1.2.3.2";
    request.nodeId = xq::NodeId(9);
    request.assetId = xq::AssetId(10);

    std::vector<std::string> invalidLocators = {"/series"};
#ifdef _WIN32
    invalidLocators.push_back("C:series");
    invalidLocators.push_back(R"(\series)");
#endif
    for (std::vector<std::string>::const_iterator it = invalidLocators.begin();
         it != invalidLocators.end(); ++it) {
        request.sourceRelPath = *it;
        xq::DicomImportResult rejected =
            xq::DicomImportService::prepare(reader, request);
        if (rejected.status != xq::DicomSeriesStatus::InvalidArgument
            || rejected.ok() || rejected.batchSpec.has_value()
            || rejected.residentSource != nullptr || reader.readCount != 0
            || !diagnosticsContainCode(
                rejected.diagnostics, "dicom.import.invalid_argument")) {
            return fail("rooted relative locator is rejected before reading",
                        __LINE__);
        }
    }
    return 0;
}

int testMalformedFingerprintsRejected()
{
    FakeDicomSeriesReader reader;
    xq::DicomImportRequest request;
    request.sourceDirectory = "C:/synthetic/source";
    request.sourceRelPath = "source";
    request.seriesInstanceUid = "1.2.3.2";
    request.nodeId = xq::NodeId(11);
    request.assetId = xq::AssetId(12);

    const std::vector<std::string> malformed = {
        "dicom-series-v1:sha256:fake",
        std::string("dicom-series-v2:sha256:") + std::string(64, 'a'),
        std::string("dicom-series-v1:sha512:") + std::string(64, 'a'),
        std::string("dicom-series-v1:sha256:") + std::string(63, 'a'),
        std::string("dicom-series-v1:sha256:") + std::string(65, 'a'),
        std::string("dicom-series-v1:sha256:") + std::string(63, 'a') + "g"
    };
    for (std::vector<std::string>::const_iterator it = malformed.begin();
         it != malformed.end(); ++it) {
        reader.fingerprint = *it;
        xq::DicomImportResult rejected =
            xq::DicomImportService::prepare(reader, request);
        if (rejected.status != xq::DicomSeriesStatus::InconsistentSeries
            || rejected.batchSpec.has_value()
            || rejected.residentSource != nullptr
            || !diagnosticsContainCode(
                rejected.diagnostics, "dicom.import.inconsistent_series")) {
            return fail("malformed versioned SHA-256 fingerprint is rejected", __LINE__);
        }
    }
    return 0;
}

int testOverflowingVoxelShapeRejected()
{
    FakeDicomSeriesReader reader;
    reader.useFixedResult = true;
    reader.fixedResult = makeFakeReadResult();

    const int hugeDimensions[3] = {1 << 30, 1 << 30, 16};
    xq::ImageGeometry geometry = reader.fixedResult.volume.geometry();
    for (int axis = 0; axis < 3; ++axis) {
        geometry.dimensions[axis] = hugeDimensions[axis];
        reader.fixedResult.descriptor.dimensions[axis] = hugeDimensions[axis];
    }
    reader.fixedResult.descriptor.sliceCount =
        static_cast<std::size_t>(hugeDimensions[2]);
    reader.fixedResult.volume.setGeometry(geometry);
    reader.fixedResult.buffer =
        std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::Float32,
            hugeDimensions,
            1,
            std::vector<std::uint8_t>());
    if (!reader.fixedResult.buffer->is_valid()
        || !reader.fixedResult.ok()) {
        return fail("overflow fixture exposes legacy wrapped buffer size", __LINE__);
    }

    xq::DicomImportRequest request;
    request.sourceDirectory = "C:/synthetic/source";
    request.sourceRelPath = "source";
    request.seriesInstanceUid = "1.2.3.2";
    request.nodeId = xq::NodeId(31);
    request.assetId = xq::AssetId(32);
    xq::DicomImportResult rejected =
        xq::DicomImportService::prepare(reader, request);
    if (rejected.status != xq::DicomSeriesStatus::InconsistentSeries
        || rejected.ok() || rejected.batchSpec.has_value()
        || rejected.residentSource != nullptr
        || !diagnosticsContainCode(
            rejected.diagnostics, "dicom.import.inconsistent_series")) {
        return fail("overflowing voxel shape is rejected before batch creation",
                    __LINE__);
    }
    return 0;
}

int testCommitRejectionsKeepProjectStateAtomic()
{
    FakeDicomSeriesReader reader;
    xq::DicomImportRequest request;
    request.sourceDirectory = "C:/synthetic/source";
    request.sourceRelPath = "source";
    request.seriesInstanceUid = "1.2.3.2";
    request.nodeId = xq::NodeId(21);
    request.assetId = xq::AssetId(22);

    xq::XQProject sceneConflictProject;
    if (sceneConflictProject.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open scene-conflict project", __LINE__);
    }
    xq::DicomImportResult sceneConflict =
        xq::DicomImportService::prepare(reader, request);
    if (!sceneConflict.ok()) {
        return fail("prepare scene-conflict DICOM batch", __LINE__);
    }
    std::shared_ptr<const xq::IVoxelSource> sceneResident =
        std::move(sceneConflict.residentSource);
    std::weak_ptr<const xq::IVoxelSource> sceneResidentWeak = sceneResident;
    xq::XQDataNode existingNode(
        request.nodeId,
        xq::XQDomainType::Image,
        "Existing image node",
        sceneConflict.batchSpec->node.payload());
    if (sceneConflictProject.scene().insert(existingNode)
        != xq::XQScene::InsertResult::Inserted) {
        return fail("install scene conflict", __LINE__);
    }
    xq::XQCommandStack sceneStack;
    std::unique_ptr<xq::XQCommand> sceneCommand(new xq::ProjectNodeBatchCommand(
        &sceneConflictProject, std::move(sceneConflict.batchSpec.value())));
    if (sceneStack.push(std::move(sceneCommand))) {
        return fail("scene conflict rejects DICOM project batch", __LINE__);
    }
    const xq::XQDataNode* preservedNode =
        sceneConflictProject.scene().find(request.nodeId);
    xq::VoxelLease sceneWhole = sceneResident->acquire_whole();
    if (preservedNode == nullptr
        || preservedNode->display_name() != "Existing image node"
        || preservedNode->hasAssetId() || preservedNode->hasScaleSlot()
        || nodeCount(sceneConflictProject.scene()) != 1
        || derivedRelationCount(sceneConflictProject.scene()) != 0
        || sceneConflictProject.assetRegistry().assetCount() != 0
        || sceneConflictProject.assetRegistry().relationCount() != 0
        || sceneStack.undo_count() != 0 || sceneStack.redo_count() != 0
        || !sceneWhole.view().valid
        || !close(sceneWhole.view().scalarAt(0), -100.0)) {
        return fail("scene rejection preserves project state and caller residency", __LINE__);
    }
    sceneResident.reset();
    if (!sceneResidentWeak.expired()) {
        return fail("scene rejection does not retain prepared residency", __LINE__);
    }

    xq::XQProject registryConflictProject;
    if (registryConflictProject.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open registry-conflict project", __LINE__);
    }
    xq::DicomImportResult registryConflict =
        xq::DicomImportService::prepare(reader, request);
    if (!registryConflict.ok()) {
        return fail("prepare registry-conflict DICOM batch", __LINE__);
    }
    std::shared_ptr<const xq::IVoxelSource> registryResident =
        std::move(registryConflict.residentSource);
    std::weak_ptr<const xq::IVoxelSource> registryResidentWeak = registryResident;
    xq::AssetRecord existingAsset =
        registryConflict.batchSpec->assetToRegister.value();
    existingAsset.displayName = "Existing image asset";
    existingAsset.contentFingerprint = "preexisting-fingerprint";
    existingAsset.sourceAbsPath = "C:/existing/source";
    existingAsset.sourceRelPath = "existing/source";
    if (!registryConflictProject.assetRegistry().registerAsset(existingAsset)) {
        return fail("install registry conflict", __LINE__);
    }
    xq::XQCommandStack registryStack;
    std::unique_ptr<xq::XQCommand> registryCommand(new xq::ProjectNodeBatchCommand(
        &registryConflictProject, std::move(registryConflict.batchSpec.value())));
    if (registryStack.push(std::move(registryCommand))) {
        return fail("registry conflict rejects DICOM project batch", __LINE__);
    }
    const xq::AssetRecord* preservedAsset =
        registryConflictProject.assetRegistry().find(request.assetId);
    xq::VoxelLease registryWhole = registryResident->acquire_whole();
    if (preservedAsset == nullptr
        || preservedAsset->displayName != "Existing image asset"
        || preservedAsset->contentFingerprint != "preexisting-fingerprint"
        || preservedAsset->sourceAbsPath != "C:/existing/source"
        || preservedAsset->sourceRelPath != "existing/source"
        || nodeCount(registryConflictProject.scene()) != 0
        || derivedRelationCount(registryConflictProject.scene()) != 0
        || registryConflictProject.assetRegistry().assetCount() != 1
        || registryConflictProject.assetRegistry().relationCount() != 0
        || registryStack.undo_count() != 0 || registryStack.redo_count() != 0
        || !registryWhole.view().valid
        || !close(registryWhole.view().scalarAt(0), -100.0)) {
        return fail("registry rejection preserves project state and caller residency", __LINE__);
    }
    registryResident.reset();
    if (!registryResidentWeak.expired()) {
        return fail("registry rejection does not retain prepared residency", __LINE__);
    }
    return 0;
}

} // namespace

int main()
{
    int result = testRealAdapterPrepareCommitUndoRedo();
    if (result != 0) {
        return result;
    }
    result = testSafeNameAndFailurePropagation();
    if (result != 0) {
        return result;
    }
    result = testRootedRelativeLocatorsRejected();
    if (result != 0) {
        return result;
    }
    result = testMalformedFingerprintsRejected();
    if (result != 0) {
        return result;
    }
    result = testOverflowingVoxelShapeRejected();
    if (result != 0) {
        return result;
    }
    result = testCommitRejectionsKeepProjectStateAtomic();
    if (result != 0) {
        return result;
    }
    std::printf("OK: DICOM import preparation and atomic project commit\n");
    return 0;
}
