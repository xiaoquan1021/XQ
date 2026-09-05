#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQProject.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "services/image/DicomImportService.h"
#include "services/image/ImageResourceResolver.h"
#include "services/resource/GeometryResourceManager.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr char kRegularSeriesUid[] =
    "1.2.826.0.1.3680043.10.543.101";
constexpr char kPhiSentinel[] = "PATIENT_SECRET_7788";

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

bool close(double left, double right)
{
    return std::abs(left - right) < 1e-5;
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

bool copyDirectory(const fs::path& source, const fs::path& destination)
{
    std::error_code error;
    fs::remove_all(destination, error);
    error.clear();
    fs::create_directories(destination.parent_path(), error);
    if (error) {
        return false;
    }
    fs::copy(source, destination,
             fs::copy_options::recursive
                 | fs::copy_options::overwrite_existing,
             error);
    return !error && fs::is_directory(destination);
}

bool sameExistingPath(const fs::path& left, const fs::path& right)
{
    std::error_code error;
    return fs::equivalent(left, right, error) && !error;
}

bool flipLastFileByte(const fs::path& path)
{
    std::fstream stream(
        path.string(), std::ios::in | std::ios::out | std::ios::binary);
    if (!stream) {
        return false;
    }
    stream.seekg(-1, std::ios::end);
    char value = 0;
    stream.read(&value, 1);
    if (!stream) {
        return false;
    }
    value = static_cast<char>(static_cast<unsigned char>(value) ^ 0x01U);
    stream.seekp(-1, std::ios::end);
    stream.write(&value, 1);
    stream.flush();
    return static_cast<bool>(stream);
}

const xq::XQImageVolumePayload* imagePayload(const xq::XQProject& project,
                                             const xq::NodeId& nodeId)
{
    const xq::XQDataNode* node = project.scene().find(nodeId);
    return node == nullptr
        ? nullptr
        : dynamic_cast<const xq::XQImageVolumePayload*>(node->payload().get());
}

class CountingDicomReader final : public xq::IDicomSeriesReader {
public:
    xq::DicomSeriesDiscoveryResult discover(
        const std::string& directory) override
    {
        return delegate_.discover(directory);
    }

    xq::DicomSeriesReadResult read(
        const std::string& directory,
        const std::string& seriesInstanceUid) override
    {
        ++readCount;
        lastDirectory = directory;
        lastSeriesUid = seriesInstanceUid;
        return delegate_.read(directory, seriesInstanceUid);
    }

    int readCount = 0;
    std::string lastDirectory;
    std::string lastSeriesUid;

private:
    xq::GdcmItkDicomSeriesReader delegate_;
};

class StubDicomReader final : public xq::IDicomSeriesReader {
public:
    explicit StubDicomReader(xq::DicomSeriesReadResult result)
        : result_(std::move(result))
    {
    }

    xq::DicomSeriesDiscoveryResult discover(const std::string&) override
    {
        return xq::DicomSeriesDiscoveryResult{};
    }

    xq::DicomSeriesReadResult read(
        const std::string& directory,
        const std::string& seriesInstanceUid) override
    {
        ++readCount;
        lastDirectory = directory;
        lastSeriesUid = seriesInstanceUid;
        if (onRead) {
            onRead();
        }
        return result_;
    }

    int readCount = 0;
    std::string lastDirectory;
    std::string lastSeriesUid;
    std::function<void()> onRead;

private:
    xq::DicomSeriesReadResult result_;
};

int runResolverIntegration()
{
    const fs::path fixtureRoot(XQ_DICOM_FIXTURE_ROOT);
    const fs::path regularFixture = fixtureRoot / "regular-oblique";
    const fs::path multiFixture = fixtureRoot / "multi-series";
    const fs::path root =
        fs::temp_directory_path() / "xq_image_resource_resolver_test";
    const fs::path relativeSeries = root / "relative-series";
    const fs::path fallbackSeries = root / "fallback-series";
    const fs::path replacementSeries = root / "replacement-series";
    const fs::path driftSeries = root / "drift-series";
    const fs::path projectPath = root / "dicom-project.xqproj";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);
    if (error || !copyDirectory(regularFixture, relativeSeries)
        || !copyDirectory(regularFixture, fallbackSeries)
        || !copyDirectory(multiFixture, replacementSeries)
        || !copyDirectory(regularFixture, driftSeries)
        || !flipLastFileByte(driftSeries / "slice-4.dcm")) {
        return fail("copy fixed DICOM fixtures into resolver sandbox", __LINE__);
    }

    const xq::NodeId nodeId(82001);
    const xq::AssetId assetId(92001);
    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open import project", __LINE__);
    }

    CountingDicomReader importReader;
    xq::DicomImportRequest request;
    request.sourceDirectory = fallbackSeries.string();
    request.sourceRelPath = "relative-series";
    request.seriesInstanceUid = kRegularSeriesUid;
    request.nodeId = nodeId;
    request.assetId = assetId;
    xq::DicomImportResult prepared =
        xq::DicomImportService::prepare(importReader, request);
    if (!prepared.ok() || importReader.readCount != 1) {
        return fail("prepare real DICOM import for resolver", __LINE__);
    }

    xq::GeometryResourceManager importManager(
        &project.assetRegistry(), root.string());
    if (importManager.installResidentVoxelSource(
            assetId, prepared.residentSource).valid()
        || importManager.blockCount() != 0) {
        return fail("residency cannot install before asset command commit", __LINE__);
    }

    xq::XQCommandStack stack;
    std::unique_ptr<xq::XQCommand> command(
        new xq::ProjectNodeBatchCommand(
            &project, std::move(prepared.batchSpec.value()),
            "Import DICOM series"));
    if (!stack.push(std::move(command))) {
        return fail("commit DICOM image node and asset atomically", __LINE__);
    }
    xq::GeometryResourceManager::VoxelSourceHandle installed =
        importManager.installResidentVoxelSource(
            assetId, prepared.residentSource);
    if (!installed.valid() || importManager.blockCount() != 1
        || importManager.mapCount() != 0) {
        return fail("install prepared residency after successful commit", __LINE__);
    }

    const xq::XQImageVolumePayload* livePayload = imagePayload(project, nodeId);
    if (livePayload == nullptr
        || livePayload->volume().bufferHandle() != nullptr) {
        return fail("committed image payload remains metadata-only", __LINE__);
    }
    xq::ImageResourceResolveResult resident =
        xq::ImageResourceResolver::acquire(
            assetId, livePayload->volume(), projectPath.string(),
            importManager, project.assetRegistry(), importReader);
    xq::VoxelLease residentVoxels = resident.ok()
        ? resident.source->acquire_whole()
        : xq::VoxelLease();
    if (!resident.ok() || importReader.readCount != 1
        || !residentVoxels.view().valid
        || !close(residentVoxels.view().scalarAt(0), -824.0)
        || !close(residentVoxels.view().scalarAt(47), -682.0)) {
        return fail("resident hit is headless and does not call reader", __LINE__);
    }

    if (xq::XQProjectWriter::save(project, projectPath.string())
        != xq::XQProjectWriter::Status::Ok) {
        return fail("save metadata-only DICOM project", __LINE__);
    }
    xq::XQProjectReadResult reopened;
    if (xq::XQProjectReader::load(projectPath.string(), &reopened)
        != xq::XQProjectReader::Status::Ok) {
        return fail("reopen metadata-only DICOM project", __LINE__);
    }
    const xq::XQImageVolumePayload* reopenedPayload =
        imagePayload(reopened.project, nodeId);
    const xq::AssetRecord* reopenedAsset =
        reopened.project.assetRegistry().find(assetId);
    if (reopenedPayload == nullptr || reopenedAsset == nullptr
        || reopenedPayload->volume().bufferHandle() != nullptr
        || reopenedAsset->sourceRelPath != "relative-series"
        || reopenedAsset->dicom.seriesInstanceUid != kRegularSeriesUid) {
        return fail("project round-trip preserves DICOM locator and metadata", __LINE__);
    }

    xq::XQProject otherProject;
    if (otherProject.open() != xq::XQProject::LifecycleResult::Ok
        || !otherProject.assetRegistry().registerAsset(*reopenedAsset)) {
        return fail("create second project with colliding DICOM asset id", __LINE__);
    }
    {
        xq::GeometryResourceManager otherManager(
            &otherProject.assetRegistry(), root.string());
        if (!otherManager.installResidentVoxelSource(
                assetId, prepared.residentSource).valid()) {
            return fail("install colliding residency in second project", __LINE__);
        }
        CountingDicomReader reader;
        xq::ImageResourceResolveResult rejected =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                otherManager, reopened.project.assetRegistry(), reader);
        if (rejected.status != xq::DicomSeriesStatus::InvalidArgument
            || rejected.ok() || reader.readCount != 0
            || otherManager.blockCount() != 1) {
            return fail("resolver rejects manager from another project registry",
                        __LINE__);
        }
    }

    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult resolved =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        xq::VoxelLease voxels = resolved.ok()
            ? resolved.source->acquire_whole()
            : xq::VoxelLease();
        if (!resolved.ok() || reader.readCount != 1
            || reader.lastSeriesUid != kRegularSeriesUid
            || !sameExistingPath(reader.lastDirectory, relativeSeries)
            || !voxels.view().valid
            || !close(voxels.view().scalarAt(0), -824.0)
            || manager.blockCount() != 1 || manager.mapCount() != 0) {
            return fail("new project manager lazily acquires relative DICOM source",
                        __LINE__);
        }
        xq::ImageResourceResolveResult cached =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (!cached.ok() || reader.readCount != 1) {
            return fail("lazy acquire installs project-scoped resident cache", __LINE__);
        }
    }

    fs::remove_all(relativeSeries, error);
    if (error) {
        return fail("remove relative source for absolute fallback", __LINE__);
    }
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult resolved =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (!resolved.ok() || reader.readCount != 1
            || !sameExistingPath(reader.lastDirectory, fallbackSeries)) {
            return fail("missing relative source falls back to absolute locator",
                        __LINE__);
        }
    }

    xq::AssetRecord* mutableAsset =
        reopened.project.assetRegistry().find(assetId);
    if (mutableAsset == nullptr) {
        return fail("find mutable reopened DICOM asset", __LINE__);
    }
    mutableAsset->sourceRelPath = "replacement-series";
    mutableAsset->sourceAbsPath.clear();
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult missingSeries =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (missingSeries.status != xq::DicomSeriesStatus::SeriesNotFound
            || missingSeries.ok() || reader.readCount != 1
            || reader.lastSeriesUid != kRegularSeriesUid
            || manager.blockCount() != 0
            || !diagnosticsContainCode(
                missingSeries.diagnostics,
                "dicom.resource.series_not_found")) {
            return fail("reachable replacement without persisted UID is SeriesNotFound",
                        __LINE__);
        }
    }

    mutableAsset->sourceRelPath = "drift-series";
    mutableAsset->sourceAbsPath.clear();
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult changed =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (changed.status != xq::DicomSeriesStatus::SourceChanged
            || changed.ok() || reader.readCount != 1
            || reader.lastSeriesUid != kRegularSeriesUid
            || manager.blockCount() != 0
            || !diagnosticsContainCode(
                changed.diagnostics, "dicom.resource.source_changed")) {
            return fail("real same-UID DICOM content drift is SourceChanged",
                        __LINE__);
        }
    }

    xq::GdcmItkDicomSeriesReader sourceReader;
    xq::DicomSeriesReadResult changedRead =
        sourceReader.read(fallbackSeries.string(), kRegularSeriesUid);
    if (!changedRead.ok() || changedRead.descriptor.contentFingerprint.empty()) {
        return fail("read baseline source for controlled content drift", __LINE__);
    }
    char& finalFingerprintDigit =
        changedRead.descriptor.contentFingerprint.back();
    finalFingerprintDigit = finalFingerprintDigit == '0' ? '1' : '0';
    changedRead.diagnostics.emplace_back(
        xq::DiagnosticSeverity::Warning,
        std::string(kPhiSentinel) + ".reader_code",
        std::string(kPhiSentinel) + " reader message");
    mutableAsset->sourceRelPath.clear();
    mutableAsset->sourceAbsPath = fallbackSeries.string();
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        StubDicomReader reader(std::move(changedRead));
        xq::ImageResourceResolveResult changed =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (changed.status != xq::DicomSeriesStatus::SourceChanged
            || changed.ok() || reader.readCount != 1
            || reader.lastSeriesUid != kRegularSeriesUid
            || manager.blockCount() != 0
            || diagnosticsContainText(changed.diagnostics, kPhiSentinel)
            || !diagnosticsContainCode(
                changed.diagnostics, "dicom.resource.reader_warning")
            || !diagnosticsContainCode(
                changed.diagnostics, "dicom.resource.source_changed")) {
            return fail("same UID fingerprint drift is redacted SourceChanged",
                        __LINE__);
        }
    }

    xq::DicomSeriesReadResult stableRead =
        sourceReader.read(fallbackSeries.string(), kRegularSeriesUid);
    if (!stableRead.ok()) {
        return fail("read stable source for asset revalidation", __LINE__);
    }
    const std::string stableFingerprint = mutableAsset->contentFingerprint;
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        StubDicomReader reader(std::move(stableRead));
        reader.onRead = [&reopened, assetId]() {
            xq::AssetRecord* current =
                reopened.project.assetRegistry().find(assetId);
            xq::AssetRecord replacement = *current;
            char& finalDigit = replacement.contentFingerprint.back();
            finalDigit = finalDigit == '0' ? '1' : '0';
            reopened.project.assetRegistry().unregisterAsset(assetId);
            reopened.project.assetRegistry().registerAsset(replacement);
        };
        xq::ImageResourceResolveResult changed =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (changed.status != xq::DicomSeriesStatus::SourceChanged
            || changed.ok() || reader.readCount != 1
            || manager.blockCount() != 0) {
            return fail("asset replacement during read is revalidated before install",
                        __LINE__);
        }
    }
    mutableAsset = reopened.project.assetRegistry().find(assetId);
    if (mutableAsset == nullptr) {
        return fail("reacquire asset after reader-side replacement", __LINE__);
    }
    mutableAsset->contentFingerprint = stableFingerprint;

    const std::string stableRelativePath = mutableAsset->sourceRelPath;
    const std::string stableAbsolutePath = mutableAsset->sourceAbsPath;
    xq::DicomSeriesReadResult locatorRead =
        sourceReader.read(fallbackSeries.string(), kRegularSeriesUid);
    if (!locatorRead.ok()) {
        return fail("read stable source for locator revalidation", __LINE__);
    }
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        StubDicomReader reader(std::move(locatorRead));
        reader.onRead = [&reopened, assetId]() {
            xq::AssetRecord* current =
                reopened.project.assetRegistry().find(assetId);
            xq::AssetRecord replacement = *current;
            replacement.sourceRelPath = "drift-series";
            replacement.sourceAbsPath.clear();
            reopened.project.assetRegistry().unregisterAsset(assetId);
            reopened.project.assetRegistry().registerAsset(replacement);
        };
        xq::ImageResourceResolveResult changed =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (changed.status != xq::DicomSeriesStatus::SourceChanged
            || changed.ok() || reader.readCount != 1
            || !sameExistingPath(reader.lastDirectory, fallbackSeries)
            || manager.blockCount() != 0) {
            return fail("locator replacement during read is rejected before install",
                        __LINE__);
        }
    }
    mutableAsset = reopened.project.assetRegistry().find(assetId);
    if (mutableAsset == nullptr) {
        return fail("reacquire asset after locator-side replacement", __LINE__);
    }
    mutableAsset->sourceRelPath = stableRelativePath;
    mutableAsset->sourceAbsPath = stableAbsolutePath;

    xq::DicomSeriesReadResult emptyRead;
    emptyRead.status = xq::DicomSeriesStatus::Ok;
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        StubDicomReader reader(std::move(emptyRead));
        xq::ImageResourceResolveResult failed =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (failed.status != xq::DicomSeriesStatus::ReadFailed
            || failed.ok() || reader.readCount != 1
            || manager.blockCount() != 0
            || !diagnosticsContainCode(
                failed.diagnostics, "dicom.resource.read_failed")) {
            return fail("Ok status without voxel buffer is ReadFailed",
                        __LINE__);
        }
    }

    xq::XQImageVolume invalidMetadata = reopenedPayload->volume();
    invalidMetadata.setWindowCenter(
        (std::numeric_limits<double>::quiet_NaN)());
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult invalid =
            xq::ImageResourceResolver::acquire(
                assetId, invalidMetadata, projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (invalid.status != xq::DicomSeriesStatus::InvalidArgument
            || invalid.ok() || reader.readCount != 0
            || manager.blockCount() != 0) {
            return fail("non-finite persisted image metadata is rejected pre-read",
                        __LINE__);
        }
    }

    invalidMetadata = reopenedPayload->volume();
    invalidMetadata.setRescaleSlope(2.0);
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult invalid =
            xq::ImageResourceResolver::acquire(
                assetId, invalidMetadata, projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (invalid.status != xq::DicomSeriesStatus::InvalidArgument
            || invalid.ok() || reader.readCount != 0
            || manager.blockCount() != 0) {
            return fail("non-canonical persisted rescale is rejected pre-read",
                        __LINE__);
        }
    }

    std::vector<std::string> invalidRelativeLocators = {"/series"};
#ifdef _WIN32
    invalidRelativeLocators.push_back("C:series");
    invalidRelativeLocators.push_back(R"(\series)");
#endif
    for (std::vector<std::string>::const_iterator it =
             invalidRelativeLocators.begin();
         it != invalidRelativeLocators.end(); ++it) {
        mutableAsset->sourceRelPath = *it;
        mutableAsset->sourceAbsPath = fallbackSeries.string();
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult invalid =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (invalid.status != xq::DicomSeriesStatus::InvalidArgument
            || invalid.ok() || reader.readCount != 0
            || manager.blockCount() != 0
            || !diagnosticsContainCode(
                invalid.diagnostics, "dicom.resource.invalid_argument")) {
            return fail("rooted relative locator is rejected without fallback",
                        __LINE__);
        }
    }

    mutableAsset->sourceRelPath = "missing-relative-series";
    mutableAsset->sourceAbsPath = fallbackSeries.string();
    fs::remove_all(fallbackSeries, error);
    if (error) {
        return fail("remove absolute fallback source", __LINE__);
    }
    {
        xq::GeometryResourceManager manager(
            &reopened.project.assetRegistry(), root.string());
        CountingDicomReader reader;
        xq::ImageResourceResolveResult missing =
            xq::ImageResourceResolver::acquire(
                assetId, reopenedPayload->volume(), projectPath.string(),
                manager, reopened.project.assetRegistry(), reader);
        if (missing.status != xq::DicomSeriesStatus::SourceNotFound
            || missing.ok() || reader.readCount != 0
            || manager.blockCount() != 0
            || !diagnosticsContainCode(
                missing.diagnostics, "dicom.resource.source_not_found")) {
            return fail("missing relative and absolute locators are SourceNotFound",
                        __LINE__);
        }
    }

    fs::remove_all(root, error);
    return 0;
}

} // namespace

int main()
{
    const int result = runResolverIntegration();
    if (result != 0) {
        return result;
    }
    std::printf("OK: project-scoped DICOM residency and lazy resolve\n");
    return 0;
}
