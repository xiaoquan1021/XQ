#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQProject.h"
#include "core/asset/AssetRecord.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "io/blob/Sha256.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "services/image/DicomImportService.h"
#include "services/image/DicomServiceSupport.h"
#include "services/image/ImageResourceResolver.h"
#include "services/resource/GeometryResourceManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace {

namespace fs = std::filesystem;

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

bool closeEnough(double left, double right)
{
    const double scale =
        (std::max)(1.0, (std::max)(std::abs(left), std::abs(right)));
    return std::abs(left - right) <= 1e-8 * scale;
}

bool voxelWorldRoundTrips(const xq::XQImageVolume& image)
{
    const xq::ImageGeometry& geometry = image.geometry();
    const double voxel[3] = {
        (geometry.dimensions[0] - 1) * 0.5,
        (geometry.dimensions[1] - 1) * 0.5,
        (geometry.dimensions[2] - 1) * 0.5
    };
    double world[3] = {};
    double roundTrip[3] = {};
    if (image.voxelToWorld(voxel, world)
            != xq::XQImageVolume::TransformStatus::Ok
        || image.worldToVoxel(world, roundTrip)
            != xq::XQImageVolume::TransformStatus::Ok) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!closeEnough(voxel[axis], roundTrip[axis])) {
            return false;
        }
    }
    return true;
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
        lastSeriesUid = seriesInstanceUid;
        return delegate_.read(directory, seriesInstanceUid);
    }

    int readCount = 0;
    std::string lastSeriesUid;

private:
    xq::GdcmItkDicomSeriesReader delegate_;
};

struct TempTree {
    fs::path path;

    ~TempTree()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

int runRealSeriesGate()
{
    const fs::path sourceDirectory(XQ_DICOM_TEST_DATA_ROOT);
    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery =
        reader.discover(sourceDirectory.string());
    if (!discovery.ok() || discovery.series.size() != 1
        || discovery.series.front().identity.seriesInstanceUid.empty()
        || discovery.series.front().sliceCount < 2
        || discovery.series.front().dimensions[2] < 2) {
        return fail(
            "configured directory must expose exactly one multi-slice DICOM series",
            __LINE__);
    }

    const std::string seriesUid =
        discovery.series.front().identity.seriesInstanceUid;
    std::string expectedFingerprint;
    xq::DicomSeriesIdentity expectedIdentity;
    xq::ImageGeometry expectedGeometry{};
    std::size_t expectedByteCount = 0;
    std::string expectedVoxelDigest;
    {
        const xq::DicomSeriesReadResult initial =
            reader.read(sourceDirectory.string(), seriesUid);
        if (!initial.ok()
            || !xq::isValidCanonicalDicomImageMetadata(initial.volume)
            || !xq::isValidVoxelBufferForImage(initial.volume, *initial.buffer)
            || !xq::isValidDicomSeriesFingerprint(
                initial.descriptor.contentFingerprint)
            || !voxelWorldRoundTrips(initial.volume)) {
            return fail(
                "configured DICOM series must decode to canonical XQ image data",
                __LINE__);
        }
        expectedFingerprint = initial.descriptor.contentFingerprint;
        expectedIdentity = initial.volume.dicomIdentity();
        expectedGeometry = initial.volume.geometry();
        expectedByteCount = initial.buffer->bytes().size();
        expectedVoxelDigest = xq::Sha256::hashHex(
            initial.buffer->bytes().data(), initial.buffer->bytes().size());
    }

    TempTree temp;
    temp.path = fs::temp_directory_path()
        / (std::string("xq-dicom-real-series-")
           + std::to_string(
               std::chrono::high_resolution_clock::now()
                   .time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directories(temp.path, error) || error) {
        return fail("create isolated real-series project directory", __LINE__);
    }
    const fs::path projectPath = temp.path / "real-series.xqproj";

    const xq::NodeId nodeId(83001);
    const xq::AssetId assetId(93001);
    xq::DicomImportRequest request;
    request.sourceDirectory = sourceDirectory.string();
    request.seriesInstanceUid = seriesUid;
    request.nodeId = nodeId;
    request.assetId = assetId;
    {
        xq::DicomImportResult prepared =
            xq::DicomImportService::prepare(reader, request);
        if (!prepared.ok() || prepared.residentSource == nullptr) {
            return fail("prepare configured series for atomic project import",
                        __LINE__);
        }

        xq::XQProject project;
        if (project.open() != xq::XQProject::LifecycleResult::Ok) {
            return fail("open isolated real-series project", __LINE__);
        }
        xq::GeometryResourceManager importManager(
            &project.assetRegistry(), temp.path.string());
        if (importManager.installResidentVoxelSource(
                assetId, prepared.residentSource).valid()) {
            return fail("resident source cannot install before project commit",
                        __LINE__);
        }

        xq::XQCommandStack stack;
        std::unique_ptr<xq::XQCommand> command(
            new xq::ProjectNodeBatchCommand(
                &project, std::move(prepared.batchSpec.value()),
                "Import authorized DICOM series"));
        if (!stack.push(std::move(command))) {
            return fail("commit configured series node and asset atomically",
                        __LINE__);
        }
        if (!importManager.installResidentVoxelSource(
                assetId, prepared.residentSource).valid()) {
            return fail("install configured series residency after commit",
                        __LINE__);
        }

        const xq::XQImageVolumePayload* livePayload =
            imagePayload(project, nodeId);
        const xq::AssetRecord* liveAsset = project.assetRegistry().find(assetId);
        if (livePayload == nullptr || liveAsset == nullptr
            || livePayload->volume().bufferHandle() != nullptr
            || liveAsset->contentFingerprint != expectedFingerprint
            || !xq::sameDicomSeriesIdentity(
                liveAsset->dicom, expectedIdentity)
            || !xq::sameImageGeometry(
                livePayload->volume().geometry(), expectedGeometry)) {
            return fail(
                "committed project stores metadata and external identity only",
                __LINE__);
        }

        if (xq::XQProjectWriter::save(project, projectPath.string())
            != xq::XQProjectWriter::Status::Ok) {
            return fail("save configured real-series project", __LINE__);
        }
    }

    xq::XQProjectReadResult reopened;
    if (xq::XQProjectReader::load(projectPath.string(), &reopened)
        != xq::XQProjectReader::Status::Ok) {
        return fail("reopen configured real-series project", __LINE__);
    }

    const xq::XQImageVolumePayload* reopenedPayload =
        imagePayload(reopened.project, nodeId);
    const xq::AssetRecord* reopenedAsset =
        reopened.project.assetRegistry().find(assetId);
    if (reopenedPayload == nullptr || reopenedAsset == nullptr
        || reopenedPayload->volume().bufferHandle() != nullptr
        || reopenedAsset->contentFingerprint != expectedFingerprint
        || !xq::sameDicomSeriesIdentity(
            reopenedAsset->dicom, expectedIdentity)
        || !xq::sameImageGeometry(
            reopenedPayload->volume().geometry(), expectedGeometry)) {
        return fail("project reopen preserves real-series metadata without decoding",
                    __LINE__);
    }

    xq::GeometryResourceManager reopenedManager(
        &reopened.project.assetRegistry(), temp.path.string());
    CountingDicomReader lazyReader;
    xq::ImageResourceResolveResult resolved =
        xq::ImageResourceResolver::acquire(
            assetId, reopenedPayload->volume(), projectPath.string(),
            reopenedManager, reopened.project.assetRegistry(), lazyReader);
    xq::VoxelLease voxels = resolved.ok()
        ? resolved.source->acquire_whole()
        : xq::VoxelLease();
    const std::string actualVoxelDigest = voxels.view().valid
        ? xq::Sha256::hashHex(
            voxels.view().bytes.data(), voxels.view().bytes.size())
        : std::string();
    if (!resolved.ok() || lazyReader.readCount != 1
        || lazyReader.lastSeriesUid != seriesUid
        || !voxels.view().valid
        || voxels.view().bytes.size() != expectedByteCount
        || actualVoxelDigest != expectedVoxelDigest) {
        return fail("headless lazy acquire restores identical real-series voxels",
                    __LINE__);
    }

    xq::ImageResourceResolveResult cached =
        xq::ImageResourceResolver::acquire(
            assetId, reopenedPayload->volume(), projectPath.string(),
            reopenedManager, reopened.project.assetRegistry(), lazyReader);
    if (!cached.ok() || lazyReader.readCount != 1) {
        return fail("second real-series acquire uses project-scoped residency",
                    __LINE__);
    }
    return 0;
}

} // namespace

int main()
{
    const int result = runRealSeriesGate();
    if (result != 0) {
        return result;
    }
    std::printf("OK: authorized DICOM import, reopen, and lazy voxel recovery\n");
    return 0;
}
