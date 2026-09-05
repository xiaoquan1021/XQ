#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQContourGroupPayload.h>
#include <core/XQDataNode.h>
#include <core/XQDerivationStamp.h>
#include <core/XQDomainType.h>
#include <core/XQImageVolume.h>
#include <core/XQImageVolumePayload.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQPayload.h>
#include <core/XQProject.h>
#include <core/XQScaleSlot.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfile.h>
#include <core/XQVesselProfilePayload.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

const xq::NodeId kImageNode(1101);
const xq::NodeId kPathNode(1102);
const xq::NodeId kContourNode(1103);
const xq::NodeId kProfileNode(1104);

const xq::AssetId kImageAsset(2101);
const xq::AssetId kPathAsset(2102);
const xq::AssetId kContourAsset(2103);
const xq::AssetId kProfileAsset(2104);

constexpr xq::ContentRevision kImageRevision = 101;
constexpr xq::ContentRevision kPathRevision = 202;
constexpr xq::ContentRevision kContourRevision = 303;
constexpr xq::ContentRevision kProfileRevision = 404;

const char* const kImageFingerprint = "sha256:image-series-shell-a";
const char* const kPathFingerprint = "sha256:path-shell-a";
const char* const kContourFingerprint = "sha256:contour-shell-a";
const char* const kProfileFingerprint = "sha256:profile-shell-a";

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

bool close_enough(double left, double right)
{
    return std::abs(left - right) <= 1e-12;
}

bool same_point(const xq::Point3& left, const xq::Point3& right)
{
    return close_enough(left.x, right.x)
        && close_enough(left.y, right.y)
        && close_enough(left.z, right.z);
}

bool same_geometry(const xq::ImageGeometry& left, const xq::ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || !close_enough(left.spacing[axis], right.spacing[axis])
            || !close_enough(left.origin[axis], right.origin[axis])) {
            return false;
        }
        for (int component = 0; component < 3; ++component) {
            if (!close_enough(
                    left.direction[axis][component], right.direction[axis][component])) {
                return false;
            }
        }
    }
    return true;
}

bool same_image_metadata(const xq::XQImageVolume& left, const xq::XQImageVolume& right)
{
    if (left.hasGeometry() != right.hasGeometry()) {
        return false;
    }
    if (left.hasGeometry() && !same_geometry(left.geometry(), right.geometry())) {
        return false;
    }
    if (left.scalarType() != right.scalarType()
        || left.componentCount() != right.componentCount()
        || !close_enough(left.intensityRange().minimum, right.intensityRange().minimum)
        || !close_enough(left.intensityRange().maximum, right.intensityRange().maximum)
        || left.modality() != right.modality()
        || left.hasDicomIdentity() != right.hasDicomIdentity()) {
        return false;
    }
    if (left.hasDicomIdentity()
        && (left.dicomIdentity().studyInstanceUid != right.dicomIdentity().studyInstanceUid
            || left.dicomIdentity().seriesInstanceUid
                != right.dicomIdentity().seriesInstanceUid
            || left.dicomIdentity().frameOfReferenceUid
                != right.dicomIdentity().frameOfReferenceUid)) {
        return false;
    }
    return close_enough(left.windowCenter(), right.windowCenter())
        && close_enough(left.windowWidth(), right.windowWidth())
        && close_enough(left.rescaleSlope(), right.rescaleSlope())
        && close_enough(left.rescaleIntercept(), right.rescaleIntercept());
}

bool same_path(const xq::XQPath& left, const xq::XQPath& right)
{
    if (left.id() != right.id()
        || left.interpolation() != right.interpolation()
        || left.hasSourceImageNode() != right.hasSourceImageNode()
        || !close_enough(left.sampleSpacing(), right.sampleSpacing())
        || left.controlPoints().size() != right.controlPoints().size()) {
        return false;
    }
    if (left.hasSourceImageNode() && left.sourceImageNode() != right.sourceImageNode()) {
        return false;
    }
    for (std::size_t i = 0; i < left.controlPoints().size(); ++i) {
        if (!same_point(left.controlPoints()[i].position, right.controlPoints()[i].position)) {
            return false;
        }
    }
    return true;
}

bool same_contour_group(const xq::XQContourGroup& left, const xq::XQContourGroup& right)
{
    if (left.id() != right.id()
        || left.hasSourcePathNode() != right.hasSourcePathNode()
        || left.contours().size() != right.contours().size()) {
        return false;
    }
    if (left.hasSourcePathNode() && left.sourcePathNode() != right.sourcePathNode()) {
        return false;
    }
    for (std::size_t i = 0; i < left.contours().size(); ++i) {
        const xq::XQContour& expected = left.contours()[i];
        const xq::XQContour& actual = right.contours()[i];
        if (expected.contourId != actual.contourId
            || !close_enough(expected.pathArcLength, actual.pathArcLength)
            || expected.type != actual.type
            || expected.closed != actual.closed
            || !same_point(expected.frame.origin, actual.frame.origin)
            || !same_point(expected.frame.normal, actual.frame.normal)
            || !same_point(expected.frame.xAxis, actual.frame.xAxis)
            || !same_point(expected.frame.yAxis, actual.frame.yAxis)
            || expected.points.size() != actual.points.size()) {
            return false;
        }
        for (std::size_t point = 0; point < expected.points.size(); ++point) {
            if (!same_point(expected.points[point], actual.points[point])) {
                return false;
            }
        }
    }
    return true;
}

bool same_derivation_stamp(const xq::DerivationStamp& left,
                           const xq::DerivationStamp& right)
{
    if (left.algorithmId != right.algorithmId
        || left.algorithmVersion != right.algorithmVersion
        || left.parameterSummary != right.parameterSummary
        || left.randomSeed != right.randomSeed
        || left.inputs.size() != right.inputs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.inputs.size(); ++i) {
        const xq::DerivationInputStamp& expected = left.inputs[i];
        const xq::DerivationInputStamp& actual = right.inputs[i];
        if (expected.nodeId != actual.nodeId
            || expected.contentRevision != actual.contentRevision
            || expected.assetId != actual.assetId
            || expected.assetFingerprint != actual.assetFingerprint) {
            return false;
        }
    }
    return true;
}

bool same_profile(const xq::VesselProfileV1& left, const xq::VesselProfileV1& right)
{
    if (left.contractVersion != right.contractVersion
        || left.coordinateSystem != right.coordinateSystem
        || left.lengthUnit != right.lengthUnit
        || left.areaUnit != right.areaUnit
        || left.frameOfReferenceId != right.frameOfReferenceId
        || left.sourcePathNode != right.sourcePathNode
        || left.sourceEvidenceNodes != right.sourceEvidenceNodes
        || left.externalEvidenceId != right.externalEvidenceId
        || left.externalEvidenceFingerprint != right.externalEvidenceFingerprint
        || !same_derivation_stamp(left.derivationStamp, right.derivationStamp)
        || left.samples.size() != right.samples.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.samples.size(); ++i) {
        const xq::VesselProfileSample& expected = left.samples[i];
        const xq::VesselProfileSample& actual = right.samples[i];
        if (expected.sampleId != actual.sampleId
            || !close_enough(expected.arcLengthMm, actual.arcLengthMm)
            || !same_point(expected.positionMm, actual.positionMm)
            || !same_point(expected.unitTangent, actual.unitTangent)
            || !close_enough(expected.areaMm2, actual.areaMm2)
            || expected.evidenceKind != actual.evidenceKind
            || expected.quality != actual.quality
            || expected.sourceEvidenceNode != actual.sourceEvidenceNode) {
            return false;
        }
    }
    return true;
}

std::filesystem::path unique_temp_root(const char* label)
{
    static unsigned int sequence = 0;
    ++sequence;
    const long long tick = std::chrono::high_resolution_clock::now()
                               .time_since_epoch()
                               .count();
    return std::filesystem::temp_directory_path()
        / (std::string("xq_shell_domain_persistence_") + label + "_"
           + std::to_string(tick) + "_" + std::to_string(sequence));
}

class TempProjectFiles {
public:
    explicit TempProjectFiles(const char* label)
        : root_(unique_temp_root(label)),
          project_path_(root_ / "shell.xqproj"),
          assets_path_(root_ / "shell.assets")
    {
        std::error_code error;
        std::filesystem::create_directories(root_, error);
        ready_ = !error;
    }

    ~TempProjectFiles()
    {
        std::error_code error;
        std::filesystem::remove(project_path_, error);
        error.clear();
        std::filesystem::remove(project_path_.string() + ".tmp", error);
        error.clear();
        std::filesystem::remove_all(assets_path_, error);
        error.clear();
        std::filesystem::remove_all(root_, error);
    }

    TempProjectFiles(const TempProjectFiles&) = delete;
    TempProjectFiles& operator=(const TempProjectFiles&) = delete;

    bool ready() const
    {
        return ready_;
    }

    const std::filesystem::path& project_path() const
    {
        return project_path_;
    }

private:
    std::filesystem::path root_;
    std::filesystem::path project_path_;
    std::filesystem::path assets_path_;
    bool ready_ = false;
};

xq::ImageGeometry make_image_geometry()
{
    xq::ImageGeometry geometry{};
    geometry.dimensions[0] = 128;
    geometry.dimensions[1] = 96;
    geometry.dimensions[2] = 42;
    geometry.spacing[0] = 0.625;
    geometry.spacing[1] = 0.75;
    geometry.spacing[2] = 1.25;
    geometry.origin[0] = -120.5;
    geometry.origin[1] = 44.25;
    geometry.origin[2] = 8.75;
    geometry.direction[0][0] = 0.0;
    geometry.direction[0][1] = -1.0;
    geometry.direction[0][2] = 0.0;
    geometry.direction[1][0] = 1.0;
    geometry.direction[1][1] = 0.0;
    geometry.direction[1][2] = 0.0;
    geometry.direction[2][0] = 0.0;
    geometry.direction[2][1] = 0.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

xq::XQImageVolume make_image_volume()
{
    xq::XQImageVolume image;
    image.setGeometry(make_image_geometry());
    image.setScalarType(xq::ScalarType::Int16);
    image.setComponentCount(2);
    image.setIntensityRange({-1024.5, 3071.25});
    image.setModality(xq::ImageModality::CT);
    image.setDicomIdentity({"1.2.840.113619.study.shell",
                            "1.2.840.113619.series.shell",
                            "1.2.840.113619.frame.shell"});
    image.setWindowCenter(41.5);
    image.setWindowWidth(375.25);
    image.setRescaleSlope(1.125);
    image.setRescaleIntercept(-1024.0);
    image.setBuffer(std::make_shared<xq::ImageBufferHandle>());
    return image;
}

bool make_path(xq::XQPath* out)
{
    if (out == nullptr) {
        return false;
    }
    xq::XQPath path;
    path.setId(xq::NodeId(3102));
    path.setInterpolation(xq::PathInterpolation::Spline);
    path.setSourceImageNode(kImageNode);
    path.setControlPoints({{{-10.0, 0.0, 0.0}},
                           {{-5.0, 1.5, 2.0}},
                           {{0.0, 3.0, 4.0}},
                           {{6.0, 4.5, 7.0}}});
    const xq::XQPath::ResampleStatus status = path.resample(2.5);
    if (status != xq::XQPath::ResampleStatus::Ok) {
        return false;
    }
    *out = std::move(path);
    return true;
}

xq::XQContourGroup make_contour_group()
{
    xq::XQContourGroup group;
    group.setId(xq::NodeId(3103));
    group.setSourcePathNode(kPathNode);

    xq::XQContour contour{};
    contour.contourId = xq::NodeId(4101);
    contour.pathArcLength = 5.0;
    contour.frame.origin = {-4.0, 1.8, 2.4};
    contour.frame.normal = {0.0, 0.0, 1.0};
    contour.frame.xAxis = {1.0, 0.0, 0.0};
    contour.frame.yAxis = {0.0, 1.0, 0.0};
    contour.type = xq::ContourType::SplinePolygon;
    contour.closed = true;
    contour.points = {{-6.0, 1.8, 2.4},
                      {-4.0, 3.3, 2.4},
                      {-2.0, 1.8, 2.4},
                      {-4.0, 0.3, 2.4}};
    group.addContour(contour);
    return group;
}

xq::VesselProfileV1 make_profile()
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "1.2.840.113619.frame.shell";
    profile.sourcePathNode = kPathNode;
    profile.sourceEvidenceNodes = {kContourNode};
    profile.externalEvidenceId = "gold-standard-shell-profile";
    profile.externalEvidenceFingerprint = "sha256:external-gold-shell-a";

    profile.derivationStamp.algorithmId = "xq.shell.profile.assembler";
    profile.derivationStamp.algorithmVersion = "1.0.0";
    profile.derivationStamp.parameterSummary = "station-spacing-mm=5;evidence=mixed";
    profile.derivationStamp.randomSeed = 8675309;

    xq::DerivationInputStamp pathInput;
    pathInput.nodeId = kPathNode;
    pathInput.contentRevision = kPathRevision;
    pathInput.assetId = kPathAsset;
    pathInput.assetFingerprint = kPathFingerprint;
    profile.derivationStamp.inputs.push_back(pathInput);

    xq::DerivationInputStamp contourInput;
    contourInput.nodeId = kContourNode;
    contourInput.contentRevision = kContourRevision;
    contourInput.assetId = std::nullopt;
    contourInput.assetFingerprint.clear();
    profile.derivationStamp.inputs.push_back(contourInput);

    xq::VesselProfileSample measuredStart;
    measuredStart.sampleId = xq::VesselSampleId(5101);
    measuredStart.arcLengthMm = 0.0;
    measuredStart.positionMm = {-10.0, 0.0, 0.0};
    measuredStart.unitTangent = {1.0, 0.0, 0.0};
    measuredStart.areaMm2 = 32.5;
    measuredStart.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
    measuredStart.quality = xq::VesselSampleQuality::Accepted;
    measuredStart.sourceEvidenceNode = kContourNode;
    profile.samples.push_back(measuredStart);

    xq::VesselProfileSample importedMiddle;
    importedMiddle.sampleId = xq::VesselSampleId(5102);
    importedMiddle.arcLengthMm = 5.0;
    importedMiddle.positionMm = {-4.0, 1.8, 2.4};
    importedMiddle.unitTangent = {0.0, 1.0, 0.0};
    importedMiddle.areaMm2 = 27.25;
    importedMiddle.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
    importedMiddle.quality = xq::VesselSampleQuality::ReviewRequired;
    importedMiddle.sourceEvidenceNode = xq::NodeId::invalid();
    profile.samples.push_back(importedMiddle);

    xq::VesselProfileSample measuredEnd;
    measuredEnd.sampleId = xq::VesselSampleId(5103);
    measuredEnd.arcLengthMm = 10.0;
    measuredEnd.positionMm = {1.5, 3.5, 5.0};
    measuredEnd.unitTangent = {0.0, 0.0, 1.0};
    measuredEnd.areaMm2 = 21.75;
    measuredEnd.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
    measuredEnd.quality = xq::VesselSampleQuality::Accepted;
    measuredEnd.sourceEvidenceNode = kContourNode;
    profile.samples.push_back(measuredEnd);

    return profile;
}

xq::AssetRecord make_asset(const xq::AssetId& id,
                           xq::AssetCategory category,
                           xq::AssetKind kind,
                           const char* name,
                           const char* fingerprint)
{
    xq::AssetRecord record;
    record.id = id;
    record.category = category;
    record.kind = kind;
    record.displayName = name;
    record.contentFingerprint = fingerprint;
    return record;
}

bool read_text(const std::filesystem::path& path, std::string* out)
{
    if (out == nullptr) {
        return false;
    }
    std::ifstream input(path.c_str(), std::ios::in);
    if (!input.good()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return false;
    }
    *out = buffer.str();
    return true;
}

bool write_text(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    output << text;
    return output.good();
}

bool replace_once(std::string* text, const std::string& before, const std::string& after)
{
    if (text == nullptr) {
        return false;
    }
    const std::size_t position = text->find(before);
    if (position == std::string::npos || text->find(before, position + 1) != std::string::npos) {
        return false;
    }
    text->replace(position, before.size(), after);
    return true;
}

std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>>
scene_relations(const xq::XQScene& scene)
{
    std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations;
    scene.visit_derived_relations(
        [&relations](const xq::NodeId& source, const xq::NodeId& derived) {
            relations.insert(std::make_pair(source.value(), derived.value()));
        });
    return relations;
}

int test_schema_13_domain_roundtrip()
{
    TempProjectFiles files("roundtrip");
    if (!files.ready()) {
        return fail("create round-trip temp directory", __LINE__);
    }

    xq::XQProject project;
    const xq::XQProject::LifecycleResult openResult = project.open();
    if (openResult != xq::XQProject::LifecycleResult::Ok) {
        return fail("open source project", __LINE__);
    }

    xq::XQImageVolume image = make_image_volume();
    const std::shared_ptr<xq::ImageBufferHandle> callerBuffer = image.bufferHandle();
    if (callerBuffer == nullptr || !callerBuffer->is_valid()) {
        return fail("image fixture starts with caller-owned residency", __LINE__);
    }
    const std::shared_ptr<xq::XQImageVolumePayload> imagePayload =
        std::make_shared<xq::XQImageVolumePayload>(image);
    if (imagePayload->volume().bufferHandle() != nullptr
        || image.bufferHandle() != callerBuffer
        || !callerBuffer->is_valid()) {
        return fail("typed image payload is metadata-only without clearing caller buffer", __LINE__);
    }

    xq::XQPath path;
    const bool pathBuilt = make_path(&path);
    if (!pathBuilt) {
        return fail("build resampled path fixture", __LINE__);
    }
    const xq::XQContourGroup contours = make_contour_group();
    const xq::VesselProfileV1 profile = make_profile();
    const xq::VesselProfileValidationResult profileValidation =
        xq::VesselProfileValidator::validate(profile);
    if (!profileValidation.ok()) {
        return fail("mixed-evidence profile fixture is valid", __LINE__);
    }

    xq::AssetRecord imageAsset = make_asset(
        kImageAsset, xq::AssetCategory::ExternalSource, xq::AssetKind::Image,
        "Shell CT series", kImageFingerprint);
    imageAsset.sourceAbsPath = "C:\\Patient Data\\Shell A\\DICOM Series";
    imageAsset.sourceRelPath = "dicom/shell-a";
    imageAsset.hasDicom = true;
    imageAsset.dicom = image.dicomIdentity();
    imageAsset.hasGeometry = true;
    imageAsset.geometry = image.geometry();

    const xq::AssetRecord pathAsset = make_asset(
        kPathAsset, xq::AssetCategory::Derived, xq::AssetKind::Path,
        "Navigation path", kPathFingerprint);
    const xq::AssetRecord contourAsset = make_asset(
        kContourAsset, xq::AssetCategory::Derived, xq::AssetKind::Contour,
        "Measured contour", kContourFingerprint);
    const xq::AssetRecord profileAsset = make_asset(
        kProfileAsset, xq::AssetCategory::Derived, xq::AssetKind::VesselProfile,
        "Solver profile", kProfileFingerprint);

    const bool imageAssetRegistered = project.assetRegistry().registerAsset(imageAsset);
    const bool pathAssetRegistered = project.assetRegistry().registerAsset(pathAsset);
    const bool contourAssetRegistered = project.assetRegistry().registerAsset(contourAsset);
    const bool profileAssetRegistered = project.assetRegistry().registerAsset(profileAsset);
    if (!imageAssetRegistered || !pathAssetRegistered
        || !contourAssetRegistered || !profileAssetRegistered) {
        return fail("register explicit shell assets", __LINE__);
    }

    xq::XQDataNode imageNode(
        kImageNode, xq::XQDomainType::Image, "Whole-body CT", imagePayload);
    imageNode.setContentRevision(kImageRevision);
    imageNode.clearScaleSlot();
    imageNode.setAssetId(kImageAsset);

    xq::XQDataNode pathNode(
        kPathNode, xq::XQDomainType::Path, "Aortic navigation path",
        std::make_shared<xq::XQPathPayload>(path));
    pathNode.setContentRevision(kPathRevision);
    pathNode.setScaleSlot(xq::ScaleSlot::Organ);
    pathNode.setAssetId(kPathAsset);

    xq::XQDataNode contourNode(
        kContourNode, xq::XQDomainType::ContourGroup, "Cross-section evidence",
        std::make_shared<xq::XQContourGroupPayload>(contours));
    contourNode.setContentRevision(kContourRevision);
    contourNode.setScaleSlot(xq::ScaleSlot::Micro);
    contourNode.setAssetId(kContourAsset);

    xq::XQDataNode profileNode(
        kProfileNode, xq::XQDomainType::VesselProfile, "VesselProfileV1",
        std::make_shared<xq::XQVesselProfilePayload>(profile));
    profileNode.setContentRevision(kProfileRevision);
    profileNode.setScaleSlot(xq::ScaleSlot::Cell);
    profileNode.setAssetId(kProfileAsset);

    const xq::XQScene::InsertResult imageInserted = project.scene().insert(imageNode);
    const xq::XQScene::InsertResult pathInserted = project.scene().insert(pathNode);
    const xq::XQScene::InsertResult contourInserted = project.scene().insert(contourNode);
    const xq::XQScene::InsertResult profileInserted = project.scene().insert(profileNode);
    if (imageInserted != xq::XQScene::InsertResult::Inserted
        || pathInserted != xq::XQScene::InsertResult::Inserted
        || contourInserted != xq::XQScene::InsertResult::Inserted
        || profileInserted != xq::XQScene::InsertResult::Inserted) {
        return fail("insert four shell domain nodes", __LINE__);
    }

    const xq::XQScene::RelationResult pathLinked =
        project.scene().link_derived(kPathNode, kProfileNode);
    const xq::XQScene::RelationResult contourLinked =
        project.scene().link_derived(kContourNode, kProfileNode);
    if (pathLinked != xq::XQScene::RelationResult::Linked
        || contourLinked != xq::XQScene::RelationResult::Linked) {
        return fail("link Path and Contour as Profile parents", __LINE__);
    }
    if (project.assetRegistry().relationCount() != 0) {
        return fail("source project starts without manually duplicated asset lineage", __LINE__);
    }

    const xq::XQProjectWriter::Status saveStatus =
        xq::XQProjectWriter::save(project, files.project_path().string());
    if (saveStatus != xq::XQProjectWriter::Status::Ok) {
        return fail("save schema 1.3 shell project", __LINE__);
    }

    std::string savedText;
    const bool savedTextRead = read_text(files.project_path(), &savedText);
    if (!savedTextRead
        || savedText.find("XQ_NATIVE_PROJECT schemaVersion 1.3\n") == std::string::npos
        || savedText.find("minimumReaderVersion 1.3\n") == std::string::npos) {
        return fail("writer declares schema and minimum reader 1.3", __LINE__);
    }

    xq::XQProjectReadResult loaded = {};
    const xq::XQProjectReader::Status loadStatus =
        xq::XQProjectReader::load(files.project_path().string(), &loaded);
    if (loadStatus != xq::XQProjectReader::Status::Ok) {
        return fail("reopen schema 1.3 shell project", __LINE__);
    }
    if (!loaded.diagnostics.empty()
        || loaded.project.state() != xq::XQProject::LifecycleState::Open) {
        return fail("reopened shell project is open without diagnostics", __LINE__);
    }

    const xq::XQDataNode* loadedImage = loaded.project.scene().find(kImageNode);
    const xq::XQDataNode* loadedPath = loaded.project.scene().find(kPathNode);
    const xq::XQDataNode* loadedContour = loaded.project.scene().find(kContourNode);
    const xq::XQDataNode* loadedProfile = loaded.project.scene().find(kProfileNode);
    if (loadedImage == nullptr || loadedPath == nullptr
        || loadedContour == nullptr || loadedProfile == nullptr) {
        return fail("reopened project contains all four nodes", __LINE__);
    }

    if (loadedImage->domainType() != xq::XQDomainType::Image
        || loadedImage->hasScaleSlot()
        || loadedImage->contentRevision() != kImageRevision
        || !loadedImage->hasAssetId() || loadedImage->assetId() != kImageAsset) {
        return fail("image node preserves absent scale and revision", __LINE__);
    }
    if (loadedPath->domainType() != xq::XQDomainType::Path
        || !loadedPath->hasScaleSlot()
        || loadedPath->scaleSlot().value() != xq::ScaleSlot::Organ
        || loadedPath->contentRevision() != kPathRevision
        || !loadedPath->hasAssetId() || loadedPath->assetId() != kPathAsset) {
        return fail("path node preserves Organ scale and revision", __LINE__);
    }
    if (loadedContour->domainType() != xq::XQDomainType::ContourGroup
        || !loadedContour->hasScaleSlot()
        || loadedContour->scaleSlot().value() != xq::ScaleSlot::Micro
        || loadedContour->contentRevision() != kContourRevision
        || !loadedContour->hasAssetId() || loadedContour->assetId() != kContourAsset) {
        return fail("contour node preserves Micro scale and revision", __LINE__);
    }
    if (loadedProfile->domainType() != xq::XQDomainType::VesselProfile
        || !loadedProfile->hasScaleSlot()
        || loadedProfile->scaleSlot().value() != xq::ScaleSlot::Cell
        || loadedProfile->contentRevision() != kProfileRevision
        || !loadedProfile->hasAssetId() || loadedProfile->assetId() != kProfileAsset) {
        return fail("profile node preserves Cell scale and revision", __LINE__);
    }

    const std::shared_ptr<xq::XQImageVolumePayload> loadedImagePayload =
        std::dynamic_pointer_cast<xq::XQImageVolumePayload>(loadedImage->payload());
    if (loadedImagePayload == nullptr
        || !same_image_metadata(image, loadedImagePayload->volume())
        || loadedImagePayload->volume().bufferHandle() != nullptr) {
        return fail("typed image metadata round-trips without voxel residency", __LINE__);
    }

    const std::shared_ptr<xq::XQPathPayload> loadedPathPayload =
        std::dynamic_pointer_cast<xq::XQPathPayload>(loadedPath->payload());
    if (loadedPathPayload == nullptr || !same_path(path, loadedPathPayload->path())) {
        return fail("typed navigation path round-trips", __LINE__);
    }

    const std::shared_ptr<xq::XQContourGroupPayload> loadedContourPayload =
        std::dynamic_pointer_cast<xq::XQContourGroupPayload>(loadedContour->payload());
    if (loadedContourPayload == nullptr
        || !same_contour_group(contours, loadedContourPayload->group())) {
        return fail("typed contour source/frame/type/closed/points round-trip", __LINE__);
    }

    const std::shared_ptr<xq::XQVesselProfilePayload> loadedProfilePayload =
        std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(loadedProfile->payload());
    if (loadedProfilePayload == nullptr
        || !same_profile(profile, loadedProfilePayload->profile())) {
        return fail("mixed-evidence profile and DerivationStamp round-trip", __LINE__);
    }
    const xq::VesselProfileValidationResult reopenedValidation =
        xq::VesselProfileValidator::validate(loadedProfilePayload->profile());
    if (!reopenedValidation.ok()) {
        return fail("reopened mixed-evidence profile remains valid", __LINE__);
    }

    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>>
        expectedSceneRelations = {
            std::make_pair(kPathNode.value(), kProfileNode.value()),
            std::make_pair(kContourNode.value(), kProfileNode.value()),
        };
    if (scene_relations(loaded.project.scene()) != expectedSceneRelations) {
        return fail("Path and Contour remain Profile scene parents", __LINE__);
    }

    const xq::AssetRegistry& registry = loaded.project.assetRegistry();
    if (registry.assetCount() != 4 || registry.relationCount() != 2
        || !registry.hasRelation(kPathAsset, kProfileAsset)
        || !registry.hasRelation(kContourAsset, kProfileAsset)) {
        return fail("scene multi-parent lineage is mirrored to assets", __LINE__);
    }

    const xq::AssetRecord* reopenedImageAsset = registry.find(kImageAsset);
    const xq::AssetRecord* reopenedPathAsset = registry.find(kPathAsset);
    const xq::AssetRecord* reopenedContourAsset = registry.find(kContourAsset);
    const xq::AssetRecord* reopenedProfileAsset = registry.find(kProfileAsset);
    if (reopenedImageAsset == nullptr || reopenedPathAsset == nullptr
        || reopenedContourAsset == nullptr || reopenedProfileAsset == nullptr) {
        return fail("all explicit assets reopen", __LINE__);
    }
    if (reopenedImageAsset->category != xq::AssetCategory::ExternalSource
        || reopenedImageAsset->kind != xq::AssetKind::Image
        || reopenedImageAsset->sourceAbsPath != imageAsset.sourceAbsPath
        || reopenedImageAsset->sourceRelPath != imageAsset.sourceRelPath
        || reopenedImageAsset->contentFingerprint != kImageFingerprint
        || !reopenedImageAsset->hasDicom
        || reopenedImageAsset->dicom.studyInstanceUid
            != image.dicomIdentity().studyInstanceUid
        || reopenedImageAsset->dicom.seriesInstanceUid
            != image.dicomIdentity().seriesInstanceUid
        || reopenedImageAsset->dicom.frameOfReferenceUid
            != image.dicomIdentity().frameOfReferenceUid
        || !reopenedImageAsset->hasGeometry
        || !same_geometry(reopenedImageAsset->geometry, image.geometry())) {
        return fail("external image asset identity and metadata round-trip", __LINE__);
    }
    if (reopenedPathAsset->kind != xq::AssetKind::Path
        || reopenedContourAsset->kind != xq::AssetKind::Contour
        || reopenedProfileAsset->kind != xq::AssetKind::VesselProfile) {
        return fail("Path Contour and VesselProfile asset kinds remain distinct", __LINE__);
    }

    std::string invalidScaleText = savedText;
    if (!replace_once(&invalidScaleText,
                      " scale cell revision 404\n",
                      " scale galaxy revision 404\n")
        || !write_text(files.project_path(), invalidScaleText)) {
        return fail("prepare invalid scale token fixture", __LINE__);
    }
    xq::XQProjectReadResult invalidScale = {};
    const xq::XQProjectReader::Status invalidScaleStatus =
        xq::XQProjectReader::load(files.project_path().string(), &invalidScale);
    if (invalidScaleStatus == xq::XQProjectReader::Status::Ok) {
        return fail("invalid schema 1.3 scale token is rejected", __LINE__);
    }

    std::string invalidProfileText = savedText;
    if (!replace_once(&invalidProfileText,
                      " area 27.25 evidence imported_gold",
                      " area 0 evidence imported_gold")
        || !write_text(files.project_path(), invalidProfileText)) {
        return fail("prepare validator-rejected profile fixture", __LINE__);
    }
    xq::XQProjectReadResult invalidProfile = {};
    const xq::XQProjectReader::Status invalidProfileStatus =
        xq::XQProjectReader::load(files.project_path().string(), &invalidProfile);
    if (invalidProfileStatus == xq::XQProjectReader::Status::Ok) {
        return fail("invalid persisted VesselProfile is rejected", __LINE__);
    }

    return 0;
}

int test_shared_asset_payload_roundtrip_and_conflict()
{
    TempProjectFiles files("shared_asset");
    if (!files.ready()) {
        return fail("create shared-asset temp directory", __LINE__);
    }

    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open shared-asset project", __LINE__);
    }
    const xq::AssetId sharedAsset(9201);
    const xq::AssetRecord record = make_asset(
        sharedAsset, xq::AssetCategory::Derived, xq::AssetKind::Surface,
        "Shared unresolved surface", "sha256:shared-surface");
    if (!project.assetRegistry().registerAsset(record)) {
        return fail("register shared surface asset", __LINE__);
    }

    const std::shared_ptr<xq::XQSourcePayload> sharedPayload =
        std::make_shared<xq::XQSourcePayload>(
            xq::XQDomainType::SurfaceModel, "Models/shared.mdl");
    xq::XQDataNode first(
        xq::NodeId(9101), xq::XQDomainType::SurfaceModel, "Shared surface A", sharedPayload);
    xq::XQDataNode second(
        xq::NodeId(9102), xq::XQDomainType::SurfaceModel, "Shared surface B", sharedPayload);
    first.setAssetId(sharedAsset);
    second.setAssetId(sharedAsset);
    if (project.scene().insert(first) != xq::XQScene::InsertResult::Inserted
        || project.scene().insert(second) != xq::XQScene::InsertResult::Inserted) {
        return fail("insert two nodes sharing one asset", __LINE__);
    }
    if (xq::XQProjectWriter::save(project, files.project_path().string())
        != xq::XQProjectWriter::Status::Ok) {
        return fail("save shared-asset project", __LINE__);
    }

    xq::XQProjectReadResult loaded = {};
    if (xq::XQProjectReader::load(files.project_path().string(), &loaded)
        != xq::XQProjectReader::Status::Ok) {
        return fail("reload shared-asset project", __LINE__);
    }
    const xq::XQDataNode* loadedFirst = loaded.project.scene().find(xq::NodeId(9101));
    const xq::XQDataNode* loadedSecond = loaded.project.scene().find(xq::NodeId(9102));
    const std::shared_ptr<xq::XQSourcePayload> firstPayload = loadedFirst == nullptr
        ? std::shared_ptr<xq::XQSourcePayload>()
        : std::dynamic_pointer_cast<xq::XQSourcePayload>(loadedFirst->payload());
    const std::shared_ptr<xq::XQSourcePayload> secondPayload = loadedSecond == nullptr
        ? std::shared_ptr<xq::XQSourcePayload>()
        : std::dynamic_pointer_cast<xq::XQSourcePayload>(loadedSecond->payload());
    if (firstPayload == nullptr || secondPayload == nullptr
        || firstPayload->domainType() != xq::XQDomainType::SurfaceModel
        || secondPayload->domainType() != xq::XQDomainType::SurfaceModel
        || firstPayload->sourcePath() != "Models/shared.mdl"
        || secondPayload->sourcePath() != "Models/shared.mdl"
        || firstPayload.get() != secondPayload.get()) {
        return fail("all nodes sharing an asset restore its canonical payload", __LINE__);
    }

    TempProjectFiles conflictFiles("shared_asset_conflict");
    if (!conflictFiles.ready()) {
        return fail("create shared-asset conflict directory", __LINE__);
    }
    xq::XQProject conflict;
    if (conflict.open() != xq::XQProject::LifecycleResult::Ok
        || !conflict.assetRegistry().registerAsset(record)) {
        return fail("prepare shared-asset conflict project", __LINE__);
    }
    xq::XQDataNode conflictA(
        xq::NodeId(9111), xq::XQDomainType::SurfaceModel, "Conflict A",
        std::make_shared<xq::XQSourcePayload>(
            xq::XQDomainType::SurfaceModel, "Models/a.mdl"));
    xq::XQDataNode conflictB(
        xq::NodeId(9112), xq::XQDomainType::SurfaceModel, "Conflict B",
        std::make_shared<xq::XQSourcePayload>(
            xq::XQDomainType::SurfaceModel, "Models/b.mdl"));
    conflictA.setAssetId(sharedAsset);
    conflictB.setAssetId(sharedAsset);
    if (conflict.scene().insert(conflictA) != xq::XQScene::InsertResult::Inserted
        || conflict.scene().insert(conflictB) != xq::XQScene::InsertResult::Inserted) {
        return fail("insert conflicting shared-asset nodes", __LINE__);
    }
    if (xq::XQProjectWriter::save(conflict, conflictFiles.project_path().string())
        != xq::XQProjectWriter::Status::WriteError) {
        return fail("conflicting shared-asset payloads are rejected", __LINE__);
    }
    std::error_code conflictExistsError;
    if (std::filesystem::exists(conflictFiles.project_path(), conflictExistsError)
        || conflictExistsError) {
        return fail("shared-asset conflict publishes no project", __LINE__);
    }

    return 0;
}

int test_v12_contour_asset_kind_migration()
{
    TempProjectFiles files("v12_contour_migration");
    if (!files.ready()) {
        return fail("create contour migration directory", __LINE__);
    }
    const std::string legacy =
        "XQ_NATIVE_PROJECT schemaVersion 1.2\n"
        "writerVersion XQ-M9-001\n"
        "minimumReaderVersion 1.2\n"
        "createdWith XQrebuild\n"
        "projectId legacy-contour-kind\n"
        "scene\n"
        "nodes 1\n"
        "node 7401 contour_group LegacyContour\n"
        "relations 0\n"
        "stale 0\n"
        "endScene\n"
        "assets 1\n"
        "asset 8401 category Derived kind Image name 0\n"
        "  external 0 absPath 0 relPath 0 dicom 0 fingerprint 0\n"
        "  geometry 0\n"
        "  blobs 0\n"
        "  payload source\n"
        "    source contour_group 1 Segmentations%2FLegacy.ctgr\n"
        "  endPayload\n"
        "endAsset\n"
        "nodeAssets 1\n"
        "nodeAsset 7401 8401\n"
        "relations 0\n"
        "endAssets\n"
        "provenance\n"
        "records 1\n"
        "record synthetic load XQ-M9-001 0\n"
        "endProvenance\n"
        "diagnostics 0\n"
        "end\n";
    if (!write_text(files.project_path(), legacy)) {
        return fail("write schema 1.2 contour fixture", __LINE__);
    }

    xq::XQProjectReadResult migrated = {};
    if (xq::XQProjectReader::load(files.project_path().string(), &migrated)
        != xq::XQProjectReader::Status::Ok) {
        return fail("load schema 1.2 contour fixture", __LINE__);
    }
    const xq::AssetRecord* legacyAsset = migrated.project.assetRegistry().find(xq::AssetId(8401));
    if (legacyAsset == nullptr || legacyAsset->kind != xq::AssetKind::Image) {
        return fail("legacy project retains its in-memory Image kind before save", __LINE__);
    }

    std::error_code removeError;
    std::filesystem::remove(files.project_path(), removeError);
    if (removeError
        || xq::XQProjectWriter::save(migrated.project, files.project_path().string())
            != xq::XQProjectWriter::Status::Ok) {
        return fail("upgrade schema 1.2 contour project to 1.3", __LINE__);
    }
    std::string upgradedText;
    if (!read_text(files.project_path(), &upgradedText)
        || upgradedText.find("asset 8401 category Derived kind Contour name 0\n")
            == std::string::npos) {
        return fail("writer normalizes legacy Contour AssetKind", __LINE__);
    }

    xq::XQProjectReadResult reopened = {};
    if (xq::XQProjectReader::load(files.project_path().string(), &reopened)
        != xq::XQProjectReader::Status::Ok) {
        return fail("reload upgraded contour project", __LINE__);
    }
    const xq::XQDataNode* contourNode = reopened.project.scene().find(xq::NodeId(7401));
    const xq::AssetRecord* contourAsset = reopened.project.assetRegistry().find(xq::AssetId(8401));
    const std::shared_ptr<xq::XQSourcePayload> contourSource = contourNode == nullptr
        ? std::shared_ptr<xq::XQSourcePayload>()
        : std::dynamic_pointer_cast<xq::XQSourcePayload>(contourNode->payload());
    if (contourNode == nullptr || contourAsset == nullptr
        || contourAsset->kind != xq::AssetKind::Contour
        || contourSource == nullptr
        || contourSource->domainType() != xq::XQDomainType::ContourGroup
        || contourSource->sourcePath() != "Segmentations/Legacy.ctgr") {
        return fail("upgraded contour kind and typed payload reopen consistently", __LINE__);
    }
    return 0;
}

int test_writer_rejects_domain_and_asset_kind_mismatch()
{
    TempProjectFiles domainFiles("domain_mismatch");
    if (!domainFiles.ready()) {
        return fail("create domain mismatch directory", __LINE__);
    }
    xq::XQProject domainProject;
    if (domainProject.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open domain mismatch project", __LINE__);
    }
    xq::XQDataNode mismatchedDomain(
        xq::NodeId(9501), xq::XQDomainType::Path, "Path with image payload",
        std::make_shared<xq::XQImageVolumePayload>(make_image_volume()));
    if (domainProject.scene().insert(mismatchedDomain)
            != xq::XQScene::InsertResult::Inserted
        || xq::XQProjectWriter::save(domainProject, domainFiles.project_path().string())
            != xq::XQProjectWriter::Status::WriteError) {
        return fail("writer rejects node and payload domain mismatch", __LINE__);
    }

    TempProjectFiles kindFiles("asset_kind_mismatch");
    if (!kindFiles.ready()) {
        return fail("create asset kind mismatch directory", __LINE__);
    }
    xq::XQProject kindProject;
    if (kindProject.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open asset kind mismatch project", __LINE__);
    }
    const xq::AssetId wrongAsset(9601);
    const xq::AssetRecord wrongRecord = make_asset(
        wrongAsset, xq::AssetCategory::Derived, xq::AssetKind::Surface,
        "Wrong kind", "sha256:wrong-kind");
    if (!kindProject.assetRegistry().registerAsset(wrongRecord)) {
        return fail("register wrong-kind asset", __LINE__);
    }
    xq::XQDataNode imageNode(
        xq::NodeId(9502), xq::XQDomainType::Image, "Image bound to surface",
        std::make_shared<xq::XQImageVolumePayload>(make_image_volume()));
    imageNode.setAssetId(wrongAsset);
    if (kindProject.scene().insert(imageNode) != xq::XQScene::InsertResult::Inserted
        || xq::XQProjectWriter::save(kindProject, kindFiles.project_path().string())
            != xq::XQProjectWriter::Status::WriteError) {
        return fail("writer rejects payload and AssetKind mismatch", __LINE__);
    }

    std::error_code domainExistsError;
    std::error_code kindExistsError;
    if (std::filesystem::exists(domainFiles.project_path(), domainExistsError)
        || domainExistsError
        || std::filesystem::exists(kindFiles.project_path(), kindExistsError)
        || kindExistsError) {
        return fail("writer mismatch failures publish no project", __LINE__);
    }
    return 0;
}

class UnsupportedImagePayload final : public xq::XQPayload {
public:
    xq::XQDomainType domainType() const override
    {
        return xq::XQDomainType::Image;
    }

    std::shared_ptr<xq::XQPayload> clone() const override
    {
        return std::make_shared<UnsupportedImagePayload>();
    }
};

int test_unsupported_concrete_payload_is_rejected()
{
    TempProjectFiles files("unsupported");
    if (!files.ready()) {
        return fail("create unsupported-payload temp directory", __LINE__);
    }

    xq::XQProject project;
    const xq::XQProject::LifecycleResult openResult = project.open();
    if (openResult != xq::XQProject::LifecycleResult::Ok) {
        return fail("open unsupported-payload project", __LINE__);
    }

    xq::XQDataNode node(
        xq::NodeId(9901), xq::XQDomainType::Image, "Unsupported image payload",
        std::make_shared<UnsupportedImagePayload>());
    node.setContentRevision(1);
    const xq::XQScene::InsertResult inserted = project.scene().insert(node);
    if (inserted != xq::XQScene::InsertResult::Inserted) {
        return fail("insert unsupported concrete payload", __LINE__);
    }

    const xq::XQProjectWriter::Status saveStatus =
        xq::XQProjectWriter::save(project, files.project_path().string());
    if (saveStatus != xq::XQProjectWriter::Status::WriteError) {
        return fail("unsupported concrete payload returns WriteError", __LINE__);
    }

    std::error_code existsError;
    const bool projectExists = std::filesystem::exists(files.project_path(), existsError);
    if (existsError || projectExists) {
        return fail("failed save does not publish a partial project", __LINE__);
    }

    return 0;
}

} // namespace

int main()
{
    int result = test_schema_13_domain_roundtrip();
    if (result != 0) {
        return result;
    }

    result = test_unsupported_concrete_payload_is_rejected();
    if (result != 0) {
        return result;
    }

    result = test_shared_asset_payload_roundtrip_and_conflict();
    if (result != 0) {
        return result;
    }

    result = test_v12_contour_asset_kind_migration();
    if (result != 0) {
        return result;
    }

    result = test_writer_rejects_domain_and_asset_kind_mismatch();
    if (result != 0) {
        return result;
    }

    std::printf("OK: shell domain schema 1.3 persistence\n");
    return 0;
}
