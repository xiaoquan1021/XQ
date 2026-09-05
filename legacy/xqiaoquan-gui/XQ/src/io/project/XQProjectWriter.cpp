#include "io/project/XQProjectWriter.h"

#include "core/XQAiAnalysisPayload.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQMeshPayload.h"
#include "core/XQPathPayload.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQSimulationCasePayload.h"
#include "core/XQSourcePayload.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/source/ResidentTetSource.h"
#include "io/blob/BlobStore.h"
#include "io/blob/MerkleSidecar.h"
#include "io/blob/Sha256.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

const char* const kMagic = "XQ_NATIVE_PROJECT";
const char* const kSchemaVersion = "1.3";
const char* const kWriterVersion = "XQ-SHELL-A-001";
const char* const kMinimumReaderVersion = "1.3";
const char* const kCreatedWith = "XQrebuild";
const char* const kProjectId = "synthetic-l0-project";

struct NodeRecord {
    std::string id;
    std::string domain_type;
    std::string display_name;
    std::string scale_slot;
    ContentRevision content_revision;
};

struct RelationRecord {
    std::string source;
    std::string derived;
};

struct StaleRecord {
    std::string id;
    XQScene::StaleReason reason;
};

bool is_unreserved(unsigned char value)
{
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z')
        || (value >= '0' && value <= '9')
        || value == '.'
        || value == '_'
        || value == '-';
}

std::string encode_field(const std::string& value)
{
    const char* const hex = "0123456789ABCDEF";
    std::string encoded;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        const unsigned char ch = static_cast<unsigned char>(*it);
        if (is_unreserved(ch)) {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0F]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }
    return encoded;
}

const char* stale_reason_text(XQScene::StaleReason reason)
{
    switch (reason) {
    case XQScene::StaleReason::None:
        return "None";
    case XQScene::StaleReason::SourceChanged:
        return "SourceChanged";
    }
    return "None";
}

const char* asset_category_text(AssetCategory category)
{
    switch (category) {
    case AssetCategory::ExternalSource:
        return "ExternalSource";
    case AssetCategory::ManagedOriginal:
        return "ManagedOriginal";
    case AssetCategory::ManagedCanonical:
        return "ManagedCanonical";
    case AssetCategory::Derived:
        return "Derived";
    }
    return "";
}

const char* asset_kind_text(AssetKind kind)
{
    switch (kind) {
    case AssetKind::Image:
        return "Image";
    case AssetKind::SegmentationMask:
        return "SegmentationMask";
    case AssetKind::Surface:
        return "Surface";
    case AssetKind::Mesh:
        return "Mesh";
    case AssetKind::SimulationCase:
        return "SimulationCase";
    case AssetKind::FlowResult:
        return "FlowResult";
    case AssetKind::AiAnalysis:
        return "AiAnalysis";
    case AssetKind::Path:
        return "Path";
    case AssetKind::Contour:
        return "Contour";
    case AssetKind::VesselProfile:
        return "VesselProfile";
    }
    return "";
}

const char* coordinate_system_text(ImageCoordinateSystem coordinateSystem)
{
    switch (coordinateSystem) {
    case ImageCoordinateSystem::LPS: return "LPS";
    case ImageCoordinateSystem::RAS: return "RAS";
    }
    return "";
}

const char* scalar_type_text(ScalarType type)
{
    switch (type) {
    case ScalarType::Unknown: return "unknown";
    case ScalarType::Int8: return "int8";
    case ScalarType::UInt8: return "uint8";
    case ScalarType::Int16: return "int16";
    case ScalarType::UInt16: return "uint16";
    case ScalarType::Int32: return "int32";
    case ScalarType::UInt32: return "uint32";
    case ScalarType::Float32: return "float32";
    case ScalarType::Float64: return "float64";
    }
    return "";
}

const char* modality_text(ImageModality modality)
{
    switch (modality) {
    case ImageModality::Unknown: return "unknown";
    case ImageModality::CT: return "ct";
    case ImageModality::MR: return "mr";
    case ImageModality::Other: return "other";
    }
    return "";
}

const char* contour_type_text(ContourType type)
{
    switch (type) {
    case ContourType::Manual: return "manual";
    case ContourType::Circle: return "circle";
    case ContourType::Ellipse: return "ellipse";
    case ContourType::SplinePolygon: return "spline_polygon";
    case ContourType::LevelSetResult: return "level_set";
    case ContourType::ThresholdResult: return "threshold";
    }
    return "";
}

const char* evidence_kind_text(VesselEvidenceKind kind)
{
    switch (kind) {
    case VesselEvidenceKind::MeasuredContour: return "measured_contour";
    case VesselEvidenceKind::SegmentationDerived: return "segmentation_derived";
    case VesselEvidenceKind::ImportedGold: return "imported_gold";
    case VesselEvidenceKind::Unknown: break;
    }
    return "";
}

const char* sample_quality_text(VesselSampleQuality quality)
{
    switch (quality) {
    case VesselSampleQuality::Accepted: return "accepted";
    case VesselSampleQuality::ReviewRequired: return "review_required";
    case VesselSampleQuality::Unknown: break;
    }
    return "";
}

// A possibly-empty string field is written as "<present 0|1> [<encoded>]" so an
// empty value is unambiguous (a bare "-" sentinel would collide with the
// encoded form of a literal "-", field-spec §3).
void write_optional_string(std::ostream& output, const std::string& value)
{
    if (value.empty()) {
        output << "0";
    } else {
        output << "1 " << encode_field(value);
    }
}

// Full-precision double so the text round-trips bit-for-bit (design §3).
std::string format_double(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

void write_blob_line(std::ostream& output, const std::string& role, const BufferRef& ref)
{
    // blob <role> <relPath> <byteCount> <sha256> <formatVersion> <endianness>
    //      <elementType> <components> <elementCount>
    // The blob file is headerless (D6): every field below is the single source
    // of truth for decoding it.
    output << "blob "
           << encode_field(role) << " "
           << encode_field(ref.relPath) << " "
           << ref.byteCount << " "
           << encode_field(ref.sha256) << " "
           << ref.formatVersion << " "
           << static_cast<unsigned int>(ref.endianness) << " "
           << static_cast<unsigned int>(ref.elementType) << " "
           << ref.components << " "
           << ref.elementCount << "\n";
}

// Writes an optional NodeId as "<id>" or "-" (the no-binding sentinel).
void write_optional_node(std::ostream& output, bool has, const NodeId& node)
{
    if (has) {
        output << node.serialize();
    } else {
        output << "-";
    }
}

void write_optional_asset(std::ostream& output, const std::optional<AssetId>& asset)
{
    if (asset.has_value()) {
        output << asset->serialize();
    } else {
        output << "-";
    }
}

const char* model_source_text(ModelSource source)
{
    switch (source) {
    case ModelSource::Unknown:
        return "Unknown";
    case ModelSource::Loaded:
        return "Loaded";
    case ModelSource::Generated:
        return "Generated";
    }
    return "Unknown";
}

// ---- entity payload text blocks (S4) ------------------------------------
//
// Each block is written between "  payload <kindToken>" and "  endPayload",
// inside an asset record after its blob lines. Only the small scalar fields go
// here; the large arrays (voxels / points / tris / tets) live in content-
// addressed blobs referenced by the asset's blob lines. Layout mirrors
// field-spec §1 exactly so the reader can rebuild field for field.

void write_payload_source(std::ostream& output, const XQSourcePayload& payload)
{
    output << "    source " << domainTypeToString(payload.domainType()) << " ";
    write_optional_string(output, payload.sourcePath());
    output << "\n";
}

bool write_payload_image(std::ostream& output, const XQImageVolume& image)
{
    const char* scalarToken = scalar_type_text(image.scalarType());
    const char* modalityToken = modality_text(image.modality());
    if (scalarToken[0] == '\0' || modalityToken[0] == '\0') {
        return false;
    }

    output << "    imageGeometry " << (image.hasGeometry() ? "1" : "0");
    if (image.hasGeometry()) {
        const ImageGeometry& g = image.geometry();
        const char* coordinateToken = coordinate_system_text(g.coordinateSystem);
        if (coordinateToken[0] == '\0') {
            return false;
        }
        output << " dims " << g.dimensions[0] << " " << g.dimensions[1] << " " << g.dimensions[2];
        output << " spacing";
        for (int i = 0; i < 3; ++i) output << " " << format_double(g.spacing[i]);
        output << " origin";
        for (int i = 0; i < 3; ++i) output << " " << format_double(g.origin[i]);
        output << " direction";
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) output << " " << format_double(g.direction[r][c]);
        }
        output << " coordSys " << coordinateToken;
    }
    output << "\n";

    output << "    imageScalar " << scalarToken
           << " components " << image.componentCount()
           << " range " << format_double(image.intensityRange().minimum)
           << " " << format_double(image.intensityRange().maximum) << "\n";
    output << "    imageModality " << modalityToken << "\n";
    output << "    imageDicom " << (image.hasDicomIdentity() ? "1" : "0");
    if (image.hasDicomIdentity()) {
        output << " study ";
        write_optional_string(output, image.dicomIdentity().studyInstanceUid);
        output << " series ";
        write_optional_string(output, image.dicomIdentity().seriesInstanceUid);
        output << " frame ";
        write_optional_string(output, image.dicomIdentity().frameOfReferenceUid);
    }
    output << "\n";
    output << "    imageWindow " << format_double(image.windowCenter())
           << " " << format_double(image.windowWidth())
           << " rescale " << format_double(image.rescaleSlope())
           << " " << format_double(image.rescaleIntercept()) << "\n";
    return true;
}

bool write_payload_contour_group(std::ostream& output, const XQContourGroup& group)
{
    if (!group.id().is_valid()) {
        return false;
    }
    output << "    contourGroup " << group.id().serialize() << " sourcePath ";
    write_optional_node(output, group.hasSourcePathNode(), group.sourcePathNode());
    output << "\n";

    const std::vector<XQContour>& contours = group.contours();
    output << "    contours " << contours.size() << "\n";
    for (std::vector<XQContour>::const_iterator it = contours.begin(); it != contours.end(); ++it) {
        const char* typeToken = contour_type_text(it->type);
        if (!it->contourId.is_valid() || typeToken[0] == '\0') {
            return false;
        }
        output << "    contour " << it->contourId.serialize()
               << " arc " << format_double(it->pathArcLength)
               << " type " << typeToken
               << " closed " << (it->closed ? "1" : "0")
               << " points " << it->points.size() << "\n";
        output << "    frame"
               << " " << format_double(it->frame.origin.x)
               << " " << format_double(it->frame.origin.y)
               << " " << format_double(it->frame.origin.z)
               << " " << format_double(it->frame.normal.x)
               << " " << format_double(it->frame.normal.y)
               << " " << format_double(it->frame.normal.z)
               << " " << format_double(it->frame.xAxis.x)
               << " " << format_double(it->frame.xAxis.y)
               << " " << format_double(it->frame.xAxis.z)
               << " " << format_double(it->frame.yAxis.x)
               << " " << format_double(it->frame.yAxis.y)
               << " " << format_double(it->frame.yAxis.z) << "\n";
        for (std::vector<Point3>::const_iterator p = it->points.begin(); p != it->points.end(); ++p) {
            output << "    contourPoint " << format_double(p->x)
                   << " " << format_double(p->y)
                   << " " << format_double(p->z) << "\n";
        }
    }
    return true;
}

bool write_payload_vessel_profile(std::ostream& output, const VesselProfileV1& profile)
{
    if (!VesselProfileValidator::validate(profile).ok()) {
        return false;
    }
    output << "    profileVersion " << profile.contractVersion << "\n";
    output << "    profileFrame LPS length mm area mm2 frame ";
    write_optional_string(output, profile.frameOfReferenceId);
    output << " sourcePath " << profile.sourcePathNode.serialize() << "\n";

    output << "    profileEvidenceNodes " << profile.sourceEvidenceNodes.size();
    for (std::vector<NodeId>::const_iterator it = profile.sourceEvidenceNodes.begin();
         it != profile.sourceEvidenceNodes.end(); ++it) {
        output << " " << it->serialize();
    }
    output << "\n";

    output << "    profileExternal id ";
    write_optional_string(output, profile.externalEvidenceId);
    output << " fingerprint ";
    write_optional_string(output, profile.externalEvidenceFingerprint);
    output << "\n";

    output << "    derivation algorithm ";
    write_optional_string(output, profile.derivationStamp.algorithmId);
    output << " version ";
    write_optional_string(output, profile.derivationStamp.algorithmVersion);
    output << " parameters ";
    write_optional_string(output, profile.derivationStamp.parameterSummary);
    output << " seed " << (profile.derivationStamp.randomSeed.has_value() ? "1" : "0");
    if (profile.derivationStamp.randomSeed.has_value()) {
        output << " " << profile.derivationStamp.randomSeed.value();
    }
    output << "\n";

    output << "    derivationInputs " << profile.derivationStamp.inputs.size() << "\n";
    for (std::vector<DerivationInputStamp>::const_iterator it =
             profile.derivationStamp.inputs.begin();
         it != profile.derivationStamp.inputs.end(); ++it) {
        output << "    derivationInput " << it->nodeId.serialize()
               << " revision " << it->contentRevision << " asset ";
        write_optional_asset(output, it->assetId);
        output << " fingerprint ";
        write_optional_string(output, it->assetFingerprint);
        output << "\n";
    }

    output << "    samples " << profile.samples.size() << "\n";
    for (std::vector<VesselProfileSample>::const_iterator it = profile.samples.begin();
         it != profile.samples.end(); ++it) {
        const char* evidenceToken = evidence_kind_text(it->evidenceKind);
        const char* qualityToken = sample_quality_text(it->quality);
        if (evidenceToken[0] == '\0' || qualityToken[0] == '\0') {
            return false;
        }
        output << "    sample " << it->sampleId.serialize()
               << " arc " << format_double(it->arcLengthMm)
               << " position " << format_double(it->positionMm.x)
               << " " << format_double(it->positionMm.y)
               << " " << format_double(it->positionMm.z)
               << " tangent " << format_double(it->unitTangent.x)
               << " " << format_double(it->unitTangent.y)
               << " " << format_double(it->unitTangent.z)
               << " area " << format_double(it->areaMm2)
               << " evidence " << evidenceToken
               << " quality " << qualityToken
               << " source ";
        write_optional_node(output, it->sourceEvidenceNode.is_valid(), it->sourceEvidenceNode);
        output << "\n";
    }
    return true;
}

void write_payload_path(std::ostream& output, const XQPath& path)
{
    output << "    pathId " << path.id().serialize()
           << " interp " << (path.interpolation() == PathInterpolation::Spline ? "Spline" : "Polyline")
           << " sampleSpacing " << format_double(path.sampleSpacing())
           << " sourceImage ";
    write_optional_node(output, path.hasSourceImageNode(), path.sourceImageNode());
    output << "\n";

    const std::vector<PathControlPoint>& points = path.controlPoints();
    output << "    controlPoints " << points.size() << "\n";
    for (std::vector<PathControlPoint>::const_iterator it = points.begin(); it != points.end(); ++it) {
        output << "    cp " << format_double(it->position.x)
               << " " << format_double(it->position.y)
               << " " << format_double(it->position.z) << "\n";
    }
}

// segMask small-scalar text (dims / geometry / source image / labels); the
// voxel array itself is a blob (role "voxels").
void write_payload_seg_mask(std::ostream& output, const XQSegmentationMask& mask)
{
    output << "    maskDims " << mask.dimensionX()
           << " " << mask.dimensionY()
           << " " << mask.dimensionZ() << "\n";

    output << "    maskGeometry " << (mask.hasGeometry() ? "1" : "0");
    if (mask.hasGeometry()) {
        const ImageGeometry& g = mask.geometry();
        output << " spacing";
        for (int i = 0; i < 3; ++i) {
            output << " " << format_double(g.spacing[i]);
        }
        output << " origin";
        for (int i = 0; i < 3; ++i) {
            output << " " << format_double(g.origin[i]);
        }
        output << " direction";
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                output << " " << format_double(g.direction[r][c]);
            }
        }
        output << " coordSys " << coordinate_system_text(g.coordinateSystem);
    }
    output << "\n";

    output << "    maskSourceImage ";
    write_optional_node(output, mask.hasSourceImageNode(), mask.sourceImageNode());
    output << "\n";

    const std::vector<SegmentationLabel>& labels = mask.labels();
    output << "    labels " << labels.size() << "\n";
    for (std::vector<SegmentationLabel>::const_iterator it = labels.begin(); it != labels.end(); ++it) {
        output << "    label " << it->value << " ";
        write_optional_string(output, it->name);
        output << "\n";
    }
}

// Writes a ModelFace / MeshBoundaryFace "face" descriptor line body (kind,
// optional capId, name, loop/cell ids). Shared shape so surface faces and mesh
// boundary faces read the same on the trailing scalar fields.
void write_face_kind_cap_name(std::ostream& output, FaceKind kind,
                              const std::optional<int>& capId, const std::string& name)
{
    output << static_cast<int>(kind) << " "
           << (capId.has_value() ? "1" : "0") << " "
           << (capId.has_value() ? capId.value() : 0) << " ";
    write_optional_string(output, name);
}

void write_payload_surface(std::ostream& output, const XQSurfaceModel& model)
{
    output << "    surfaceId " << model.id().serialize()
           << " source " << model_source_text(model.source())
           << " sourceContour ";
    write_optional_node(output, model.hasSourceContourGroupNode(), model.sourceContourGroupNode());
    output << "\n";

    const PreservedVtpArrays& p = model.preservedArrays();
    output << "    preserved " << (p.hasGlobalNodeID ? "1" : "0")
           << " " << (p.hasGlobalElementID ? "1" : "0")
           << " " << (p.hasModelFaceID ? "1" : "0")
           << " " << (p.hasCapID ? "1" : "0") << "\n";

    output << "    hasTriGeom " << (model.hasTriangleGeometry() ? "1" : "0") << "\n";

    const std::vector<ModelFace>& faces = model.faces();
    output << "    faces " << faces.size() << "\n";
    for (std::vector<ModelFace>::const_iterator it = faces.begin(); it != faces.end(); ++it) {
        output << "    face " << it->faceId << " ";
        write_face_kind_cap_name(output, it->kind, it->capId, it->name);
        output << " " << it->boundaryLoopIds.size();
        for (std::vector<int>::const_iterator l = it->boundaryLoopIds.begin();
             l != it->boundaryLoopIds.end();
             ++l) {
            output << " " << *l;
        }
        output << "\n";
    }
}

void write_payload_mesh(std::ostream& output, const XQMesh& mesh)
{
    output << "    meshId " << mesh.id().serialize() << " sourceModel ";
    write_optional_node(output, mesh.hasSourceModelNode(), mesh.sourceModelNode());
    output << "\n";

    const PreservedMeshArrays& p = mesh.preservedArrays();
    output << "    meshPreserved " << (p.hasGlobalNodeID ? "1" : "0")
           << " " << (p.hasGlobalElementID ? "1" : "0")
           << " " << (p.hasModelFaceID ? "1" : "0")
           << " " << (p.hasCapID ? "1" : "0") << "\n";

    const MeshQualitySummary& q = mesh.quality();
    output << "    quality " << format_double(q.minQuality)
           << " " << format_double(q.maxQuality)
           << " " << format_double(q.meanQuality)
           << " " << q.elementCount << "\n";

    output << "    hasSurfTri " << (mesh.hasSurfaceTriangles() ? "1" : "0") << "\n";
    output << "    hasVolTet " << (mesh.hasVolumeTets() ? "1" : "0") << "\n";

    const std::vector<MeshRegion>& regions = mesh.regions();
    output << "    regions " << regions.size() << "\n";
    for (std::vector<MeshRegion>::const_iterator it = regions.begin(); it != regions.end(); ++it) {
        output << "    region " << it->regionId << " ";
        write_optional_string(output, it->name);
        output << "\n";
    }

    const std::vector<MeshBoundaryFace>& faces = mesh.boundaryFaces();
    output << "    meshFaces " << faces.size() << "\n";
    for (std::vector<MeshBoundaryFace>::const_iterator it = faces.begin(); it != faces.end(); ++it) {
        output << "    meshFace " << it->faceId << " ";
        write_face_kind_cap_name(output, it->kind, it->capId, it->name);
        output << " " << it->cellIds.size();
        for (std::vector<int>::const_iterator c = it->cellIds.begin(); c != it->cellIds.end(); ++c) {
            output << " " << *c;
        }
        output << " " << it->localFaces.size();
        for (std::vector<int>::const_iterator l = it->localFaces.begin(); l != it->localFaces.end(); ++l) {
            output << " " << *l;
        }
        output << "\n";
    }
}

void write_payload_sim_case(std::ostream& output, const XQSimulationCase& sim)
{
    output << "    caseId " << sim.id().serialize() << " sourceMesh ";
    write_optional_node(output, sim.hasSourceMeshNode(), sim.sourceMeshNode());
    output << "\n";

    const SolverParameters& s = sim.solverParameters();
    output << "    solver " << s.timeSteps << " " << format_double(s.timeStepSize) << "\n";

    const FluidProperties& f = sim.fluidProperties();
    output << "    fluid " << format_double(f.density) << " " << format_double(f.viscosity) << "\n";

    const RomSettings& r = sim.romSettings();
    output << "    rom centerline ";
    write_optional_node(output, r.centerlineNode.is_valid(), r.centerlineNode);
    output << " period " << format_double(r.period)
           << " numTimeSteps " << r.numTimeSteps
           << " dt " << format_double(r.dt)
           << " numCycles " << r.numCycles
           << " vesselProfile ";
    write_optional_node(
        output, r.vesselProfileNode.is_valid(), r.vesselProfileNode);
    output << " inlet " << r.inletFaceIds.size();
    for (std::vector<int>::const_iterator it = r.inletFaceIds.begin(); it != r.inletFaceIds.end(); ++it) {
        output << " " << *it;
    }
    output << " outlet " << r.outletFaceIds.size();
    for (std::vector<int>::const_iterator it = r.outletFaceIds.begin(); it != r.outletFaceIds.end(); ++it) {
        output << " " << *it;
    }
    output << "\n";

    if (sim.hasFlowSmokeProvenance()) {
        const FlowSmokeCaseProvenance& smoke = sim.flowSmokeProvenance();
        output << "    flowSmokeCase protocol ";
        write_optional_string(output, smoke.protocol.id);
        output << " version " << smoke.protocol.version << " label ";
        write_optional_string(output, smoke.protocol.label);
        output << " stations " << smoke.protocol.stationCount
               << " dt " << format_double(smoke.protocol.dtSeconds)
               << " steps " << smoke.protocol.numTimeSteps
               << " cycles " << smoke.protocol.numCycles
               << " sourceProfile " << smoke.sourceVesselProfileNode.serialize()
               << " sourceRevision " << smoke.sourceVesselProfileRevision
               << " assembler ";
        write_optional_string(output, smoke.assemblerId);
        output << " assemblerVersion ";
        write_optional_string(output, smoke.assemblerVersion);
        output << "\n";

        output << "    flowConversion length "
               << flowLengthUnitToToken(smoke.conversion.sourceLengthUnit)
               << " " << flowLengthUnitToToken(smoke.conversion.targetLengthUnit)
               << " " << format_double(smoke.conversion.lengthScale)
               << " area "
               << flowAreaUnitToToken(smoke.conversion.sourceAreaUnit)
               << " " << flowAreaUnitToToken(smoke.conversion.targetAreaUnit)
               << " " << format_double(smoke.conversion.areaScale) << "\n";

        output << "    flowStationMap " << smoke.stationMap.size() << "\n";
        for (const FlowStationSourceMapping& mapping : smoke.stationMap) {
            output << "    flowStation " << mapping.solverStationIndex
                   << " " << format_double(mapping.solverArcLengthCm)
                   << " " << mapping.leftSampleId.serialize()
                   << " " << mapping.rightSampleId.serialize()
                   << " " << format_double(mapping.leftWeight)
                   << " " << format_double(mapping.rightWeight) << "\n";
        }
    }

    const std::vector<BoundaryCondition>& bcs = sim.boundaryConditions();
    output << "    bcs " << bcs.size() << "\n";
    for (std::vector<BoundaryCondition>::const_iterator it = bcs.begin(); it != bcs.end(); ++it) {
        output << "    bc " << it->faceId
               << " " << static_cast<int>(it->type)
               << " " << format_double(it->value)
               << " rcr " << it->rcr.size();
        for (std::vector<double>::const_iterator d = it->rcr.begin(); d != it->rcr.end(); ++d) {
            output << " " << format_double(*d);
        }
        output << " waveform " << it->flowWaveform.size();
        for (std::vector<std::pair<double, double>>::const_iterator w = it->flowWaveform.begin();
             w != it->flowWaveform.end();
             ++w) {
            output << " " << format_double(w->first) << " " << format_double(w->second);
        }
        output << " waveformPeriod " << format_double(it->waveformPeriod) << "\n";
    }
}

// flowResult q/p/a matrices stay as main-document text rows for now (S4.2
// decision); only the geometry/mesh large arrays go to blobs in S4.
void write_payload_flow_result(std::ostream& output, const XQFlowResult& flow)
{
    output << "    flowSourceCase ";
    write_optional_node(output, flow.hasSourceCaseNode(), flow.sourceCaseNode());
    output << " converged " << (flow.converged() ? "1" : "0")
           << " maxCfl " << format_double(flow.maxCfl()) << "\n";

    if (flow.hasFlowSmokeProvenance()) {
        const FlowSmokeResultProvenance& smoke = flow.flowSmokeProvenance();
        output << "    flowSmokeResult protocol ";
        write_optional_string(output, smoke.protocol.id);
        output << " version " << smoke.protocol.version << " label ";
        write_optional_string(output, smoke.protocol.label);
        output << " stations " << smoke.protocol.stationCount
               << " dt " << format_double(smoke.protocol.dtSeconds)
               << " steps " << smoke.protocol.numTimeSteps
               << " cycles " << smoke.protocol.numCycles
               << " sourceProfile " << smoke.sourceVesselProfileNode.serialize()
               << " sourceRevision " << smoke.sourceVesselProfileRevision
               << " solver ";
        write_optional_string(output, smoke.solverId);
        output << " solverVersion ";
        write_optional_string(output, smoke.solverVersion);
        output << "\n";
    }

    const std::vector<double>& times = flow.times();
    output << "    times " << times.size();
    for (std::vector<double>::const_iterator it = times.begin(); it != times.end(); ++it) {
        output << " " << format_double(*it);
    }
    output << "\n";

    const std::vector<FlowSegment>& segments = flow.segments();
    output << "    segments " << segments.size() << "\n";
    for (std::vector<FlowSegment>::const_iterator it = segments.begin(); it != segments.end(); ++it) {
        output << "    seg " << it->segmentId
               << " " << format_double(it->arcLengthStart)
               << " " << format_double(it->arcLengthEnd)
               << " " << it->faceId << "\n";
    }

    // series: S rows x K columns, three matrices (q / p / a), each row one line.
    const std::vector<std::vector<double>>& q = flow.flowQ();
    const std::vector<std::vector<double>>& p = flow.pressureP();
    const std::vector<std::vector<double>>& a = flow.areaA();
    const std::size_t rows = q.size();
    const std::size_t cols = times.size();
    output << "    series " << rows << " " << cols << "\n";
    const std::vector<std::vector<double>>* mats[3] = {&q, &p, &a};
    const char* tags[3] = {"q", "p", "a"};
    for (int m = 0; m < 3; ++m) {
        for (std::size_t s = 0; s < rows; ++s) {
            output << "    " << tags[m];
            for (std::size_t c = 0; c < cols; ++c) {
                output << " " << format_double((*mats[m])[s][c]);
            }
            output << "\n";
        }
    }
}

void write_payload_ai_analysis(std::ostream& output, const XQAiAnalysis& ai)
{
    output << "    aiKind " << static_cast<int>(ai.kind())
           << " provenance " << static_cast<int>(ai.provenance())
           << " modelId ";
    write_optional_string(output, ai.modelId());
    output << " source ";
    write_optional_node(output, ai.hasSourceNode(), ai.sourceNode());
    output << " diagnostic ";
    write_optional_string(output, ai.diagnostic());
    output << "\n";

    const std::vector<NamedMetric>& metrics = ai.metrics();
    output << "    metrics " << metrics.size() << "\n";
    for (std::vector<NamedMetric>::const_iterator it = metrics.begin(); it != metrics.end(); ++it) {
        output << "    metric ";
        write_optional_string(output, it->name);
        output << " " << format_double(it->value) << " ";
        write_optional_string(output, it->unit);
        output << "\n";
    }

    const std::vector<Annotation>& annotations = ai.annotations();
    output << "    annotations " << annotations.size() << "\n";
    for (std::vector<Annotation>::const_iterator it = annotations.begin(); it != annotations.end(); ++it) {
        output << "    annotation ";
        write_optional_string(output, it->label);
        output << " " << format_double(it->arcLength)
               << " " << it->faceId
               << " " << format_double(it->score) << "\n";
    }
}

// Dispatches the payload text block by concrete type. Writes nothing (no
// "payload" line) when the payload carries no serializable small-scalar data
// (currently every supported kind has at least an id/path, so a present payload
// always yields a block). Returns true when a block was written.
bool write_payload_block(std::ostream& output, const std::shared_ptr<XQPayload>& payload)
{
    if (!payload) {
        return false;
    }
    switch (payload->domainType()) {
    case XQDomainType::Image:
    case XQDomainType::Path:
    case XQDomainType::ContourGroup:
    case XQDomainType::SegmentationMask:
    case XQDomainType::SurfaceModel:
    case XQDomainType::Mesh:
    case XQDomainType::SimulationCase:
    case XQDomainType::FlowResult:
    case XQDomainType::AiAnalysis:
    case XQDomainType::VesselProfile:
    case XQDomainType::Unknown:
        break;
    }

    if (std::shared_ptr<XQImageVolumePayload> image =
            std::dynamic_pointer_cast<XQImageVolumePayload>(payload)) {
        output << "  payload imageVolume\n";
        if (!write_payload_image(output, image->volume())) {
            return false;
        }
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQContourGroupPayload> contour =
            std::dynamic_pointer_cast<XQContourGroupPayload>(payload)) {
        output << "  payload contourGroup\n";
        if (!write_payload_contour_group(output, contour->group())) {
            return false;
        }
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQVesselProfilePayload> profile =
            std::dynamic_pointer_cast<XQVesselProfilePayload>(payload)) {
        output << "  payload vesselProfileV1\n";
        if (!write_payload_vessel_profile(output, profile->profile())) {
            return false;
        }
        output << "  endPayload\n";
        return true;
    }

    // source payload (parameterized domain): image / contour group / SV mask.
    if (std::shared_ptr<XQSourcePayload> source =
            std::dynamic_pointer_cast<XQSourcePayload>(payload)) {
        output << "  payload source\n";
        write_payload_source(output, *source);
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQPathPayload> path = std::dynamic_pointer_cast<XQPathPayload>(payload)) {
        output << "  payload path\n";
        write_payload_path(output, path->path());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQSegmentationMaskPayload> mask =
            std::dynamic_pointer_cast<XQSegmentationMaskPayload>(payload)) {
        output << "  payload segMask\n";
        write_payload_seg_mask(output, mask->mask());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQSurfaceModelPayload> surface =
            std::dynamic_pointer_cast<XQSurfaceModelPayload>(payload)) {
        output << "  payload surface\n";
        write_payload_surface(output, surface->model());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQMeshPayload> mesh = std::dynamic_pointer_cast<XQMeshPayload>(payload)) {
        output << "  payload mesh\n";
        write_payload_mesh(output, mesh->mesh());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQSimulationCasePayload> sim =
            std::dynamic_pointer_cast<XQSimulationCasePayload>(payload)) {
        output << "  payload simCase\n";
        write_payload_sim_case(output, sim->simulationCase());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQFlowResultPayload> flow =
            std::dynamic_pointer_cast<XQFlowResultPayload>(payload)) {
        output << "  payload flowResult\n";
        write_payload_flow_result(output, flow->result());
        output << "  endPayload\n";
        return true;
    }
    if (std::shared_ptr<XQAiAnalysisPayload> ai =
            std::dynamic_pointer_cast<XQAiAnalysisPayload>(payload)) {
        output << "  payload aiAnalysis\n";
        write_payload_ai_analysis(output, ai->analysis());
        output << "  endPayload\n";
        return true;
    }
    return false;
}

bool write_asset_record(std::ostream& output, const AssetRecord& record,
                        const std::shared_ptr<XQPayload>& payload)
{
    const char* categoryToken = asset_category_text(record.category);
    const char* kindToken = asset_kind_text(record.kind);
    if (categoryToken[0] == '\0' || kindToken[0] == '\0') {
        return false;
    }
    output << "asset " << record.id.serialize()
           << " category " << categoryToken
           << " kind " << kindToken
           << " name ";
    write_optional_string(output, record.displayName);
    output << "\n";

    // External locator (meaningful for ExternalSource; written for all so the
    // grammar is fixed-shape and the reader does not branch on category).
    output << "  external " << (record.category == AssetCategory::ExternalSource ? "1" : "0");
    output << " absPath ";
    write_optional_string(output, record.sourceAbsPath);
    output << " relPath ";
    write_optional_string(output, record.sourceRelPath);
    output << " dicom " << (record.hasDicom ? "1" : "0");
    if (record.hasDicom) {
        output << " study ";
        write_optional_string(output, record.dicom.studyInstanceUid);
        output << " series ";
        write_optional_string(output, record.dicom.seriesInstanceUid);
        output << " frame ";
        write_optional_string(output, record.dicom.frameOfReferenceUid);
    }
    output << " fingerprint ";
    write_optional_string(output, record.contentFingerprint);
    output << "\n";

    // Geometry metadata (registered for external imaging; voxels are NOT copied).
    output << "  geometry " << (record.hasGeometry ? "1" : "0");
    if (record.hasGeometry) {
        const ImageGeometry& g = record.geometry;
        const char* coordinateToken = coordinate_system_text(g.coordinateSystem);
        if (coordinateToken[0] == '\0') {
            return false;
        }
        output << " dims " << g.dimensions[0] << " " << g.dimensions[1] << " " << g.dimensions[2];
        output << " spacing";
        for (int i = 0; i < 3; ++i) {
            output << " " << format_double(g.spacing[i]);
        }
        output << " origin";
        for (int i = 0; i < 3; ++i) {
            output << " " << format_double(g.origin[i]);
        }
        output << " direction";
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                output << " " << format_double(g.direction[r][c]);
            }
        }
        output << " coordSys " << coordinateToken;
    }
    output << "\n";

    output << "  blobs " << record.blobs.size() << "\n";
    for (std::vector<std::pair<std::string, BufferRef>>::const_iterator it = record.blobs.begin();
         it != record.blobs.end();
         ++it) {
        output << "  ";
        write_blob_line(output, it->first, it->second);
    }

    // Optional entity payload text block (S4): small-scalar fields of the bound
    // node's payload. Large arrays already went to blobs above. An asset with no
    // bound payload (e.g. a hand-registered external image) writes no block.
    if (payload && !write_payload_block(output, payload)) {
        return false;
    }

    output << "endAsset\n";
    return output.good();
}

// Writes the whole 'assets' section: asset records, node->asset bindings, and
// asset-level lineage. An empty project still writes "assets 0" / "nodeAssets 0"
// / "relations 0" / "endAssets" so the section is always present in a 1.3 doc
// (so loading it back emits no "no asset data" diagnostic).
bool write_assets_section(std::ostream& output, const XQProject& project)
{
    const AssetRegistry& registry = project.assetRegistry();

    // Map each bound asset id to the payload of the node that carries it, so the
    // asset record can emit that payload's small-scalar text block. Built from
    // the scene; a hand-registered asset with no bound node maps to a null
    // payload and writes no block.
    std::map<AssetId, std::shared_ptr<XQPayload>> payload_by_asset;
    project.scene().visit_nodes(
        [&payload_by_asset](const XQDataNode& node) {
            if (node.hasAssetId() && node.payload()
                && payload_by_asset.find(node.assetId()) == payload_by_asset.end()) {
                payload_by_asset.emplace(node.assetId(), node.payload());
            }
        });

    output << "assets " << registry.assetCount() << "\n";
    bool recordsOk = true;
    registry.visit_assets(
        [&output, &payload_by_asset, &recordsOk](const AssetRecord& record) {
            if (!recordsOk) {
                return;
            }
            std::map<AssetId, std::shared_ptr<XQPayload>>::const_iterator found =
                payload_by_asset.find(record.id);
            const std::shared_ptr<XQPayload> payload =
                (found != payload_by_asset.end()) ? found->second : std::shared_ptr<XQPayload>();
            recordsOk = write_asset_record(output, record, payload);
        });
    if (!recordsOk) {
        return false;
    }

    // Node->Asset bindings live in their own records so the scene 'node' line
    // stays a 4-token row (design §3).
    std::vector<std::pair<std::string, std::string>> node_assets;
    project.scene().visit_nodes(
        [&node_assets](const XQDataNode& node) {
            if (node.hasAssetId()) {
                node_assets.push_back(
                    std::make_pair(node.id().serialize(), node.assetId().serialize()));
            }
        });
    output << "nodeAssets " << node_assets.size() << "\n";
    for (std::vector<std::pair<std::string, std::string>>::const_iterator it = node_assets.begin();
         it != node_assets.end();
         ++it) {
        output << "nodeAsset " << it->first << " " << it->second << "\n";
    }

    std::vector<std::pair<std::string, std::string>> asset_relations;
    registry.visit_relations(
        [&asset_relations](const AssetId& source, const AssetId& derived) {
            asset_relations.push_back(
                std::make_pair(source.serialize(), derived.serialize()));
        });
    output << "relations " << asset_relations.size() << "\n";
    for (std::vector<std::pair<std::string, std::string>>::const_iterator it = asset_relations.begin();
         it != asset_relations.end();
         ++it) {
        output << "assetRel " << it->first << " " << it->second << "\n";
    }

    output << "endAssets\n";
    return output.good();
}

// ---- asset derivation (S4.4): writer auto-derives blobs from payloads -----

// Returns the absolute "<stem>.assets" directory for a project file path, e.g.
// "/tmp/foo.xqproj" -> "/tmp/foo.assets". Used as the BlobStore root; the
// returned BufferRef.relPath stays relative to this root (S4.1).
std::string asset_dir_for(const std::string& projectFilePath)
{
    std::filesystem::path p(projectFilePath);
    std::filesystem::path dir = p.parent_path();
    std::string stem = p.stem().string(); // file name without the final extension
    std::filesystem::path assets = dir / (stem + ".assets");
    return assets.string();
}

// Flattens a triangle surface handle into (points F64x3, tris I32x3, faceId
// I32x1) element vectors and publishes each as a blob role under the record.
// Returns false on any put failure (caller aborts the whole save). Reads the
// geometry through the read-only Source contract (M9a): contiguous point /
// triangle / faceId spans, bulk-copied into the flat blob buffers.
bool put_triangle_geometry(BlobStore& store, AssetRecord* record,
                           const XQTriangleSurfaceGeometryHandle& geom,
                           const char* pointsRole, const char* trisRole, const char* faceIdRole)
{
    ResidentSurfaceSource source(
        std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
            std::shared_ptr<const void>(), &geom));
    GeometryLease<Point3> pointLease = source.acquire_points();
    TriangleLease triLease = source.acquire_triangles();
    const ReadSpan<Point3>& pts = pointLease.span();
    const TriangleView& triView = triLease.view();
    const ReadSpan<SourceTriangle>& triSpan = triView.triangles;
    const ReadSpan<int>& faceIdSpan = triView.faceIds;

    const std::size_t pointCount = pts.size();
    const std::size_t triCount = triSpan.size();

    // Point3 is packed double[3]; copy the contiguous span straight into the
    // flat F64x3 buffer.
    static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3]");
    std::vector<double> points(pointCount * 3);
    if (pointCount > 0) {
        std::memcpy(points.data(), pts.data(), pointCount * sizeof(Point3));
    }
    // SourceTriangle is std::array<int,3>; copy the contiguous span into I32x3.
    std::vector<int> tris(triCount * 3);
    if (triCount > 0) {
        std::memcpy(tris.data(), triSpan.data(), triCount * sizeof(SourceTriangle));
    }
    std::vector<int> faceIds(faceIdSpan.begin(), faceIdSpan.end());

    BufferRef pointsRef = {};
    BufferRef trisRef = {};
    BufferRef faceIdRef = {};
    const bool okPoints = store.put(BlobElementType::F64, 3, pointCount, points, &pointsRef);
    const bool okTris = store.put(BlobElementType::I32, 3, triCount, tris, &trisRef);
    const bool okFaceId = store.put(BlobElementType::I32, 1, triCount, faceIds, &faceIdRef);
    if (!okPoints || !okTris || !okFaceId) {
        return false;
    }
    record->blobs.push_back(std::make_pair(std::string(pointsRole), pointsRef));
    record->blobs.push_back(std::make_pair(std::string(trisRole), trisRef));
    record->blobs.push_back(std::make_pair(std::string(faceIdRole), faceIdRef));

    // Emit SegmentedMerkle sidecars alongside every geometry blob so that
    // MappedGeometrySource can use SegmentedMerkle mode for these assets.
    // Sidecar failures are soft-errors: FullVerify (now anchored to
    // BufferRef.sha256) still detects tampering without a sidecar; missing
    // sidecars only prevent the segmented-lazy path.
    const std::uint64_t kGeomSegBytes = 1u << 20; // 1 MiB segments
    auto emitSidecar = [&store, kGeomSegBytes](const BufferRef& ref) {
        if (ref.relPath.empty()) {
            return;
        }
        const std::string blobAbs = store.rootDir() + "/" + ref.relPath;
        write_sidecar_for_file(blobAbs, kGeomSegBytes, blobAbs + ".merkle");
    };
    emitSidecar(pointsRef);
    emitSidecar(trisRef);
    emitSidecar(faceIdRef);
    return true;
}

// Publishes the large arrays of one node's payload as blobs into `record`.
// Only segMask / surface / mesh carry large arrays; others have none and return
// true with no blob written. Returns false on any put failure.
bool put_payload_blobs(BlobStore& store, AssetRecord* record,
                       const std::shared_ptr<XQPayload>& payload)
{
    if (std::shared_ptr<XQSegmentationMaskPayload> maskPayload =
            std::dynamic_pointer_cast<XQSegmentationMaskPayload>(payload)) {
        const XQSegmentationMask& mask = maskPayload->mask();
        const std::vector<std::uint8_t>& voxels = mask.voxels();
        BufferRef ref = {};
        const bool ok = store.put(BlobElementType::U8, 1, voxels.size(), voxels, &ref);
        if (!ok) {
            return false;
        }
        record->blobs.push_back(std::make_pair(std::string("voxels"), ref));
        return true;
    }
    if (std::shared_ptr<XQSurfaceModelPayload> surfacePayload =
            std::dynamic_pointer_cast<XQSurfaceModelPayload>(payload)) {
        const XQSurfaceModel& model = surfacePayload->model();
        if (model.hasTriangleGeometry()) {
            return put_triangle_geometry(store, record, *model.triangleGeometry(),
                                         "points", "tris", "faceId");
        }
        return true;
    }
    if (std::shared_ptr<XQMeshPayload> meshPayload =
            std::dynamic_pointer_cast<XQMeshPayload>(payload)) {
        const XQMesh& mesh = meshPayload->mesh();
        if (mesh.hasSurfaceTriangles()) {
            if (!put_triangle_geometry(store, record, *mesh.surfaceTriangles(),
                                       "surfPoints", "surfTris", "surfFaceId")) {
                return false;
            }
        }
        if (mesh.hasVolumeTets()) {
            // Read the tet mesh through the Source contract (M9a): contiguous
            // point + tet spans, bulk-copied into the flat blob buffers.
            ResidentTetSource source(mesh.volumeTets());
            GeometryLease<Point3> pointLease = source.acquire_points();
            GeometryLease<SourceTet> tetLease = source.acquire_tetrahedra();
            const ReadSpan<Point3>& ptSpan = pointLease.span();
            const ReadSpan<SourceTet>& tetSpan = tetLease.span();
            const std::size_t pointCount = ptSpan.size();
            const std::size_t tetCount = tetSpan.size();

            static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3]");
            std::vector<double> points(pointCount * 3);
            if (pointCount > 0) {
                std::memcpy(points.data(), ptSpan.data(), pointCount * sizeof(Point3));
            }
            // SourceTet is std::array<int,4>; copy the contiguous span into I32x4.
            std::vector<int> tets(tetCount * 4);
            if (tetCount > 0) {
                std::memcpy(tets.data(), tetSpan.data(), tetCount * sizeof(SourceTet));
            }
            BufferRef volPointsRef = {};
            BufferRef tetsRef = {};
            const bool okPoints = store.put(BlobElementType::F64, 3, pointCount, points, &volPointsRef);
            const bool okTets = store.put(BlobElementType::I32, 4, tetCount, tets, &tetsRef);
            if (!okPoints || !okTets) {
                return false;
            }
            record->blobs.push_back(std::make_pair(std::string("volPoints"), volPointsRef));
            record->blobs.push_back(std::make_pair(std::string("tets"), tetsRef));

            // Emit SegmentedMerkle sidecars for tet geometry blobs (mirrors the
            // surface-side sidecar emission in put_triangle_geometry).
            const std::uint64_t kGeomSegBytes = 1u << 20;
            auto emitSidecar = [&store, kGeomSegBytes](const BufferRef& ref) {
                if (ref.relPath.empty()) return;
                const std::string blobAbs = store.rootDir() + "/" + ref.relPath;
                write_sidecar_for_file(blobAbs, kGeomSegBytes, blobAbs + ".merkle");
            };
            emitSidecar(volPointsRef);
            emitSidecar(tetsRef);
        }
        return true;
    }
    // source / path / simCase / flowResult / aiAnalysis: no large arrays.
    return true;
}

// Maps a payload's domain to the asset kind it derives.
bool asset_kind_for_payload(const std::shared_ptr<XQPayload>& payload, AssetKind* out)
{
    return payload != nullptr && assetKindForDomain(payload->domainType(), out);
}

bool assign_auto_derived_fingerprint(
    const std::shared_ptr<XQPayload>& payload,
    AssetRecord* record)
{
    if (payload == nullptr || record == nullptr
        || !record->contentFingerprint.empty()) {
        return false;
    }

    std::ostringstream canonical;
    if (!write_payload_block(canonical, payload)) {
        return false;
    }
    for (const std::pair<std::string, BufferRef>& blob : record->blobs) {
        write_blob_line(canonical, blob.first, blob.second);
    }
    const std::string bytes = canonical.str();
    record->contentFingerprint = std::string("xq-payload-v1:sha256:")
        + Sha256::hashHex(bytes.data(), bytes.size());
    return !record->contentFingerprint.empty();
}

struct BoundPayloadSignature {
    XQDomainType domain = XQDomainType::Unknown;
    AssetKind kind = AssetKind::Surface;
    std::string serializedBlock;
};

// Schema 1.3 stores a canonical payload block per AssetId, while several scene
// nodes may legally share that asset. Validate the whole node/payload/binding
// graph before writing blobs so the writer cannot publish an archive that its
// own strict reader must reject. The only automatic kind repair is the known
// schema-1.2 ContourGroup bug (Image -> Contour).
bool validate_and_normalize_payload_bindings(XQProject* work)
{
    if (work == nullptr) {
        return false;
    }

    AssetRegistry& registry = work->assetRegistry();
    std::map<AssetId, BoundPayloadSignature> signature_by_asset;
    std::map<AssetId, std::vector<NodeId>> nodes_by_asset;
    bool valid = true;

    work->scene().visit_nodes(
        [&registry, &signature_by_asset, &nodes_by_asset, &valid](const XQDataNode& node) {
            if (!valid) {
                return;
            }

            if (node.hasAssetId()) {
                if (registry.find(node.assetId()) == nullptr) {
                    valid = false;
                    return;
                }
                nodes_by_asset[node.assetId()].push_back(node.id());
            }

            const std::shared_ptr<XQPayload>& payload = node.payload();
            if (!payload) {
                return;
            }
            if (node.domainType() != payload->domainType()
                || node.domain_type() != domainTypeToString(payload->domainType())) {
                valid = false;
                return;
            }

            AssetKind expectedKind = AssetKind::Surface;
            std::ostringstream payloadBlock;
            if (!asset_kind_for_payload(payload, &expectedKind)
                || !write_payload_block(payloadBlock, payload)
                || !payloadBlock.good()) {
                valid = false;
                return;
            }

            if (!node.hasAssetId()) {
                return;
            }

            BoundPayloadSignature signature;
            signature.domain = payload->domainType();
            signature.kind = expectedKind;
            signature.serializedBlock = payloadBlock.str();
            std::map<AssetId, BoundPayloadSignature>::const_iterator existing =
                signature_by_asset.find(node.assetId());
            if (existing == signature_by_asset.end()) {
                signature_by_asset.emplace(node.assetId(), signature);
                return;
            }
            if (existing->second.domain != signature.domain
                || existing->second.kind != signature.kind
                || existing->second.serializedBlock != signature.serializedBlock) {
                valid = false;
            }
        });

    if (!valid) {
        return false;
    }

    for (std::map<AssetId, BoundPayloadSignature>::const_iterator signature =
             signature_by_asset.begin();
         signature != signature_by_asset.end(); ++signature) {
        AssetRecord* record = registry.find(signature->first);
        if (record == nullptr) {
            return false;
        }
        if (record->kind != signature->second.kind) {
            if (signature->second.domain == XQDomainType::ContourGroup
                && record->kind == AssetKind::Image) {
                record->kind = AssetKind::Contour;
            } else {
                return false;
            }
        }

        const std::map<AssetId, std::vector<NodeId>>::const_iterator bound =
            nodes_by_asset.find(signature->first);
        if (bound == nodes_by_asset.end()) {
            return false;
        }
        for (std::vector<NodeId>::const_iterator nodeId = bound->second.begin();
             nodeId != bound->second.end(); ++nodeId) {
            const XQDataNode* node = work->scene().find(*nodeId);
            if (node == nullptr
                || node->domain_type() != domainTypeToString(signature->second.domain)) {
                return false;
            }
        }
    }

    return true;
}

// Derives an asset per payload-bearing node on the work copy: createAsset,
// publish its large arrays as blobs, fingerprint the canonical payload + blob
// references, then bind the node via setAssetId. A node that already has an
// asset id is left untouched (its asset was set up by a caller).
// The relative paths of every blob this call publishes are appended to
// `derivedRelPaths` so save() can self-check exactly the blobs it created (and
// not hand-registered asset blobs whose files a caller never produced).
// Returns false on any blob put failure (D7: caller aborts the save).
bool derive_payload_assets(XQProject* work, const std::string& projectFilePath,
                           std::vector<std::string>* derivedRelPaths)
{
    const std::string assetsDir = asset_dir_for(projectFilePath);
    BlobStore store(assetsDir);

    // visit_nodes is const; collect the ids to derive first, then mutate via
    // the non-const scene().find() (S4.8).
    std::vector<NodeId> to_derive;
    work->scene().visit_nodes(
        [&to_derive](const XQDataNode& node) {
            if (node.payload() && !node.hasAssetId()) {
                to_derive.push_back(node.id());
            }
        });

    AssetRegistry& registry = work->assetRegistry();
    for (std::vector<NodeId>::const_iterator it = to_derive.begin(); it != to_derive.end(); ++it) {
        XQDataNode* node = work->scene().find(*it);
        if (node == 0 || !node->payload()) {
            continue;
        }
        AssetKind kind = AssetKind::Surface;
        if (!asset_kind_for_payload(node->payload(), &kind)) {
            return false; // Never report success after dropping a typed payload.
        }
        const AssetId assetId = registry.createAsset(AssetCategory::Derived, kind);
        AssetRecord* record = registry.find(assetId);
        if (record == 0) {
            return false;
        }
        const std::size_t blobsBefore = record->blobs.size();
        if (!put_payload_blobs(store, record, node->payload())) {
            return false;
        }
        if (!assign_auto_derived_fingerprint(node->payload(), record)) {
            return false;
        }
        for (std::size_t b = blobsBefore; b < record->blobs.size(); ++b) {
            derivedRelPaths->push_back(record->blobs[b].second.relPath);
        }
        node->setAssetId(assetId);
    }
    return true;
}

bool mirror_scene_asset_lineage(XQProject* work)
{
    if (work == nullptr) {
        return false;
    }
    AssetRegistry& registry = work->assetRegistry();
    bool valid = true;
    work->scene().visit_nodes(
        [&registry, &valid](const XQDataNode& node) {
            if (node.hasAssetId() && registry.find(node.assetId()) == nullptr) {
                valid = false;
            }
        });
    if (!valid) {
        return false;
    }

    work->scene().visit_derived_relations(
        [work, &registry, &valid](const NodeId& sourceId, const NodeId& derivedId) {
            if (!valid) {
                return;
            }
            const XQDataNode* source = work->scene().find(sourceId);
            const XQDataNode* derived = work->scene().find(derivedId);
            if (source == nullptr || derived == nullptr) {
                valid = false;
                return;
            }
            if (!source->hasAssetId() || !derived->hasAssetId()
                || source->assetId() == derived->assetId()) {
                return;
            }
            if (!registry.hasRelation(source->assetId(), derived->assetId())
                && !registry.addRelation(source->assetId(), derived->assetId())) {
                valid = false;
            }
        });
    return valid;
}

} // namespace

XQProjectWriter::Status XQProjectWriter::save(const XQProject& project,
                                              const std::string& projectFilePath)
{
    // Work on a deep copy so the caller's const project is never mutated. The
    // copy carries scene + asset registry by value (verified in S3); asset
    // derivation (createAsset / setAssetId) happens on it (S4.0/S4.4).
    XQProject work = project;
    if (!validate_and_normalize_payload_bindings(&work)) {
        return Status::WriteError;
    }
    std::vector<std::string> derivedRelPaths;
    if (!derive_payload_assets(&work, projectFilePath, &derivedRelPaths)) {
        return Status::WriteError; // a blob put failed; leave no partial archive.
    }
    if (!mirror_scene_asset_lineage(&work)) {
        return Status::WriteError;
    }

    std::vector<NodeRecord> nodes;
    bool nodesValid = true;
    work.scene().visit_nodes(
        [&nodes, &nodesValid](const XQDataNode& node) {
            NodeRecord record = {};
            record.id = node.id().serialize();
            record.domain_type = encode_field(node.domain_type());
            record.display_name = encode_field(node.display_name());
            record.scale_slot = node.hasScaleSlot()
                ? scaleSlotToToken(node.scaleSlot().value())
                : "-";
            if (record.scale_slot.empty()) {
                nodesValid = false;
                return;
            }
            record.content_revision = node.contentRevision();
            nodes.push_back(record);
        });
    if (!nodesValid) {
        return Status::WriteError;
    }

    std::vector<RelationRecord> relations;
    work.scene().visit_derived_relations(
        [&relations](const NodeId& source, const NodeId& derived) {
            RelationRecord record = {};
            record.source = source.serialize();
            record.derived = derived.serialize();
            relations.push_back(record);
        });

    std::vector<StaleRecord> stale_nodes;
    work.scene().visit_stale_nodes(
        [&stale_nodes](const NodeId& node, XQScene::StaleReason reason) {
            if (reason != XQScene::StaleReason::None) {
                StaleRecord record = {};
                record.id = node.serialize();
                record.reason = reason;
                stale_nodes.push_back(record);
            }
        });

    // Write the main document to a temporary file first; replace the real path
    // atomically only after every referenced blob is verified present (D7).
    const std::string tmpPath = projectFilePath + ".tmp";
    {
        std::ofstream output(tmpPath.c_str(), std::ios::out | std::ios::trunc);
        if (!output.good()) {
            return Status::FileOpenError;
        }

        output << kMagic << " schemaVersion " << kSchemaVersion << "\n";
        output << "writerVersion " << kWriterVersion << "\n";
        output << "minimumReaderVersion " << kMinimumReaderVersion << "\n";
        output << "createdWith " << kCreatedWith << "\n";
        output << "projectId " << kProjectId << "\n";
        output << "scene\n";
        output << "nodes " << nodes.size() << "\n";
        for (std::vector<NodeRecord>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
            output << "node " << it->id << " " << it->domain_type << " " << it->display_name
                   << " scale " << it->scale_slot
                   << " revision " << it->content_revision << "\n";
        }
        output << "relations " << relations.size() << "\n";
        for (std::vector<RelationRecord>::const_iterator it = relations.begin();
             it != relations.end();
             ++it) {
            output << "derived " << it->source << " " << it->derived << "\n";
        }
        output << "stale " << stale_nodes.size() << "\n";
        for (std::vector<StaleRecord>::const_iterator it = stale_nodes.begin();
             it != stale_nodes.end();
             ++it) {
            output << "staleNode " << it->id << " " << stale_reason_text(it->reason) << "\n";
        }
        output << "endScene\n";
        if (!write_assets_section(output, work)) {
            output.close();
            std::error_code remove_ec;
            std::filesystem::remove(tmpPath, remove_ec);
            return Status::WriteError;
        }
        output << "provenance\n";
        output << "records 1\n";
        output << "record L0-synthetic save " << kWriterVersion << " " << stale_nodes.size() << "\n";
        output << "endProvenance\n";
        output << "diagnostics 0\n";
        output << "end\n";

        const bool stream_ok = output.good();
        if (!stream_ok) {
            std::error_code remove_ec;
            std::filesystem::remove(tmpPath, remove_ec);
            return Status::WriteError;
        }
    }

    // Self-check (D7): every blob this save published must exist on disk. We
    // verify only the blobs writer derived this run (not hand-registered asset
    // blob refs, whose files a caller may legitimately never have produced).
    // Side-effect calls take their result into a variable first, never inside an
    // assert (memory feedback-no-sideeffect-in-assert).
    const std::string assetsDir = asset_dir_for(projectFilePath);
    std::filesystem::path assetsRoot(assetsDir);
    bool blobs_present = true;
    for (std::vector<std::string>::const_iterator it = derivedRelPaths.begin();
         it != derivedRelPaths.end();
         ++it) {
        std::filesystem::path blobPath = assetsRoot / *it;
        std::error_code exists_ec;
        const bool exists = std::filesystem::exists(blobPath, exists_ec);
        if (!exists) {
            blobs_present = false;
        }
    }
    if (!blobs_present) {
        std::error_code remove_ec;
        std::filesystem::remove(tmpPath, remove_ec);
        return Status::WriteError;
    }

    // Atomic replace: rename temp over the real path.
    std::error_code rename_ec;
    std::filesystem::rename(tmpPath, projectFilePath, rename_ec);
    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(tmpPath, remove_ec);
        return Status::WriteError;
    }

    return Status::Ok;
}

} // namespace xq
