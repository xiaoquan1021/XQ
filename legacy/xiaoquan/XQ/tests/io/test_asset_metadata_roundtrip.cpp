// S3 acceptance: schema 1.2 asset-metadata round-trip (no entity payload yet).
// Covers: AssetRegistry records (External + Derived) with BufferRef blob refs,
// node->asset bindings, and asset-level lineage surviving save->load field for
// field; backward compatibility with an older no-assets document (Info
// diagnostic); strict minimumReaderVersion rejection (D2); and reference
// integrity failures loading as a structured error.

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQImageVolume.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/asset/AssetId.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>
#include <core/asset/BufferRef.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::filesystem::path temp_project_path(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

bool write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    output << text;
    return output.good();
}

bool buffer_ref_equal(const xq::BufferRef& a, const xq::BufferRef& b)
{
    return a.relPath == b.relPath
        && a.byteCount == b.byteCount
        && a.sha256 == b.sha256
        && a.formatVersion == b.formatVersion
        && a.endianness == b.endianness
        && a.elementType == b.elementType
        && a.components == b.components
        && a.elementCount == b.elementCount;
}

// Builds an External imaging asset record (no voxels copied) plus a Derived
// surface asset that carries blob references, binds them to scene nodes, and
// records lineage external -> derived.
xq::AssetId g_image_asset;
xq::AssetId g_surface_asset;
xq::BufferRef g_points_ref;
xq::BufferRef g_tris_ref;

void populate_project(xq::XQProject* project)
{
    xq::AssetRegistry& registry = project->assetRegistry();

    g_image_asset = registry.createAsset(
        xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::AssetRecord* image = registry.find(g_image_asset);
    image->displayName = "OSMSC0090 CT";
    image->sourceAbsPath = "C:/data/OSMSC0090-cm.vti";
    image->sourceRelPath = "images/OSMSC0090-cm.vti";
    image->hasDicom = true;
    image->dicom.studyInstanceUid = "1.2.840.113619.2.55.3.1";
    image->dicom.seriesInstanceUid = "1.2.840.113619.2.55.3.2";
    image->dicom.frameOfReferenceUid = "1.2.840.113619.2.55.3.3";
    image->contentFingerprint = "deadbeefcafef00d";
    image->hasGeometry = true;
    image->geometry.dimensions[0] = 256;
    image->geometry.dimensions[1] = 256;
    image->geometry.dimensions[2] = 120;
    image->geometry.spacing[0] = 0.5;
    image->geometry.spacing[1] = 0.5;
    image->geometry.spacing[2] = 0.625;
    image->geometry.origin[0] = -12.25;
    image->geometry.origin[1] = 33.5;
    image->geometry.origin[2] = -100.125;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            image->geometry.direction[r][c] = (r == c) ? 1.0 : 0.0;
        }
    }
    image->geometry.direction[0][1] = 0.0078125; // an off-diagonal to exercise precision
    image->geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    g_surface_asset = registry.createAsset(
        xq::AssetCategory::Derived, xq::AssetKind::Surface);
    xq::AssetRecord* surface = registry.find(g_surface_asset);
    surface->displayName = "Aorta surface";

    g_points_ref.relPath = "proj.assets/blobs/ab/abcdef.bin";
    g_points_ref.byteCount = 96;
    g_points_ref.sha256 = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    g_points_ref.formatVersion = 1;
    g_points_ref.endianness = 0;
    g_points_ref.elementType = xq::BlobElementType::F64;
    g_points_ref.components = 3;
    g_points_ref.elementCount = 4;
    surface->blobs.push_back(std::make_pair(std::string("points"), g_points_ref));

    g_tris_ref.relPath = "proj.assets/blobs/cd/cdef01.bin";
    g_tris_ref.byteCount = 48;
    g_tris_ref.sha256 = "cdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789ab";
    g_tris_ref.formatVersion = 1;
    g_tris_ref.endianness = 0;
    g_tris_ref.elementType = xq::BlobElementType::I32;
    g_tris_ref.components = 3;
    g_tris_ref.elementCount = 4;
    surface->blobs.push_back(std::make_pair(std::string("tris"), g_tris_ref));

    // Lineage: image -> surface.
    registry.addRelation(g_image_asset, g_surface_asset);

    // Scene nodes bound to assets.
    project->scene().insert(xq::XQDataNode(xq::NodeId(7001), "image.volume", "Volume"));
    project->scene().insert(xq::XQDataNode(xq::NodeId(7002), "surface.model", "Surface"));
    project->scene().find(xq::NodeId(7001))->setAssetId(g_image_asset);
    project->scene().find(xq::NodeId(7002))->setAssetId(g_surface_asset);
}

int test_asset_metadata_roundtrip()
{
    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open source project", __LINE__);
    }
    populate_project(&project);

    const std::filesystem::path path = temp_project_path("xq_s3_asset_metadata.xqproj");
    std::filesystem::remove(path);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save project with assets", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("load project with assets", __LINE__);
    }
    if (!result.diagnostics.empty()) {
        return fail("round-trip of a 1.2 doc with assets emits no diagnostics", __LINE__);
    }

    const xq::AssetRegistry& registry = result.project.assetRegistry();
    if (registry.assetCount() != 2) {
        return fail("two assets restored", __LINE__);
    }
    if (registry.relationCount() != 1) {
        return fail("one asset relation restored", __LINE__);
    }

    // External image asset, field for field.
    const xq::AssetRecord* image = registry.find(g_image_asset);
    if (image == nullptr) {
        return fail("image asset id restored", __LINE__);
    }
    if (image->category != xq::AssetCategory::ExternalSource
        || image->kind != xq::AssetKind::Image) {
        return fail("image asset category/kind", __LINE__);
    }
    if (image->displayName != "OSMSC0090 CT"
        || image->sourceAbsPath != "C:/data/OSMSC0090-cm.vti"
        || image->sourceRelPath != "images/OSMSC0090-cm.vti") {
        return fail("image asset names/paths", __LINE__);
    }
    if (!image->hasDicom
        || image->dicom.studyInstanceUid != "1.2.840.113619.2.55.3.1"
        || image->dicom.seriesInstanceUid != "1.2.840.113619.2.55.3.2"
        || image->dicom.frameOfReferenceUid != "1.2.840.113619.2.55.3.3") {
        return fail("image asset dicom identity", __LINE__);
    }
    if (image->contentFingerprint != "deadbeefcafef00d") {
        return fail("image asset content fingerprint", __LINE__);
    }
    if (!image->hasGeometry) {
        return fail("image asset has geometry", __LINE__);
    }
    if (image->geometry.dimensions[0] != 256
        || image->geometry.dimensions[1] != 256
        || image->geometry.dimensions[2] != 120) {
        return fail("image geometry dims", __LINE__);
    }
    if (image->geometry.spacing[0] != 0.5
        || image->geometry.spacing[1] != 0.5
        || image->geometry.spacing[2] != 0.625) {
        return fail("image geometry spacing", __LINE__);
    }
    if (image->geometry.origin[0] != -12.25
        || image->geometry.origin[1] != 33.5
        || image->geometry.origin[2] != -100.125) {
        return fail("image geometry origin", __LINE__);
    }
    if (image->geometry.direction[0][0] != 1.0
        || image->geometry.direction[0][1] != 0.0078125
        || image->geometry.direction[1][1] != 1.0
        || image->geometry.direction[2][2] != 1.0) {
        return fail("image geometry direction", __LINE__);
    }
    if (image->geometry.coordinateSystem != xq::ImageCoordinateSystem::LPS) {
        return fail("image geometry coordinate system", __LINE__);
    }
    if (!image->blobs.empty()) {
        return fail("external image carries no blobs", __LINE__);
    }

    // Derived surface asset with two blob references.
    const xq::AssetRecord* surface = registry.find(g_surface_asset);
    if (surface == nullptr) {
        return fail("surface asset id restored", __LINE__);
    }
    if (surface->category != xq::AssetCategory::Derived
        || surface->kind != xq::AssetKind::Surface) {
        return fail("surface asset category/kind", __LINE__);
    }
    if (surface->displayName != "Aorta surface") {
        return fail("surface asset display name", __LINE__);
    }
    if (surface->hasDicom || surface->hasGeometry
        || !surface->sourceAbsPath.empty() || !surface->sourceRelPath.empty()
        || !surface->contentFingerprint.empty()) {
        return fail("derived surface has no external locator", __LINE__);
    }
    if (surface->blobs.size() != 2) {
        return fail("surface asset has two blob refs", __LINE__);
    }
    if (surface->blobs[0].first != "points"
        || !buffer_ref_equal(surface->blobs[0].second, g_points_ref)) {
        return fail("surface points blob ref", __LINE__);
    }
    if (surface->blobs[1].first != "tris"
        || !buffer_ref_equal(surface->blobs[1].second, g_tris_ref)) {
        return fail("surface tris blob ref", __LINE__);
    }

    // Lineage edge.
    bool found_relation = false;
    registry.visit_relations(
        [&found_relation](const xq::AssetId& source, const xq::AssetId& derived) {
            if (source == g_image_asset && derived == g_surface_asset) {
                found_relation = true;
            }
        });
    if (!found_relation) {
        return fail("asset lineage image->surface restored", __LINE__);
    }

    // Node->asset bindings.
    const xq::XQDataNode* image_node = result.project.scene().find(xq::NodeId(7001));
    const xq::XQDataNode* surface_node = result.project.scene().find(xq::NodeId(7002));
    if (image_node == nullptr || surface_node == nullptr) {
        return fail("bound nodes restored", __LINE__);
    }
    if (!image_node->hasAssetId() || image_node->assetId() != g_image_asset) {
        return fail("image node bound to image asset", __LINE__);
    }
    if (!surface_node->hasAssetId() || surface_node->assetId() != g_surface_asset) {
        return fail("surface node bound to surface asset", __LINE__);
    }

    return 0;
}

int test_empty_project_writes_empty_assets_section()
{
    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open empty project", __LINE__);
    }

    const std::filesystem::path path = temp_project_path("xq_s3_empty_assets.xqproj");
    std::filesystem::remove(path);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save empty project", __LINE__);
    }

    std::string saved;
    {
        std::ifstream input(path.c_str());
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line.erase(line.size() - 1);
            }
            saved += line;
            saved += "\n";
        }
    }
    // An empty project still emits the section so a 1.2 doc round-trips with no
    // diagnostics.
    if (saved.find("\nassets 0\n") == std::string::npos) {
        std::filesystem::remove(path);
        return fail("empty project writes 'assets 0'", __LINE__);
    }
    if (saved.find("\nendAssets\n") == std::string::npos) {
        std::filesystem::remove(path);
        return fail("empty project writes endAssets", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);
    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("load empty 1.2 project", __LINE__);
    }
    if (!result.diagnostics.empty()) {
        return fail("empty 1.2 project round-trip emits no diagnostics", __LINE__);
    }
    if (result.project.assetRegistry().assetCount() != 0) {
        return fail("empty project has no assets", __LINE__);
    }
    return 0;
}

int test_legacy_no_assets_section_info_diagnostic()
{
    // A hand-written 1.1 document with no assets section loads successfully and
    // reports PROJECT_NO_ASSET_DATA (prd §7).
    const std::filesystem::path path = temp_project_path("xq_s3_legacy_no_assets.xqproj");
    const std::string text =
        "XQ_NATIVE_PROJECT schemaVersion 1.1\n"
        "writerVersion XQ-M8-001\n"
        "minimumReaderVersion 1.0\n"
        "createdWith XQrebuild\n"
        "projectId legacy-no-assets\n"
        "scene\n"
        "nodes 1\n"
        "node 8001 image.volume Volume\n"
        "relations 0\n"
        "stale 0\n"
        "endScene\n"
        "provenance\n"
        "records 1\n"
        "record synthetic load XQ-M8-001 0\n"
        "endProvenance\n"
        "diagnostics 0\n"
        "end\n";
    std::filesystem::remove(path);
    if (!write_text_file(path, text)) {
        return fail("write legacy no-assets document", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("legacy no-assets document loads", __LINE__);
    }
    if (result.diagnostics.size() != 1
        || result.diagnostics[0].severity() != xq::DiagnosticSeverity::Info
        || result.diagnostics[0].code() != "PROJECT_NO_ASSET_DATA") {
        return fail("legacy no-assets document reports PROJECT_NO_ASSET_DATA", __LINE__);
    }
    if (result.project.assetRegistry().assetCount() != 0) {
        return fail("legacy document has an empty asset registry", __LINE__);
    }
    return 0;
}

int test_minimum_reader_version_rejected()
{
    // D2 strict: a document demanding a newer reader than this build is refused.
    const std::filesystem::path path = temp_project_path("xq_s3_min_reader_future.xqproj");
    const std::string text =
        "XQ_NATIVE_PROJECT schemaVersion 1.3\n"
        "writerVersion XQ-future\n"
        "minimumReaderVersion 1.3\n"
        "createdWith XQrebuild\n"
        "projectId future-min-reader\n"
        "scene\n"
        "nodes 0\n"
        "relations 0\n"
        "stale 0\n"
        "endScene\n"
        "assets 0\n"
        "nodeAssets 0\n"
        "relations 0\n"
        "endAssets\n"
        "provenance\n"
        "records 1\n"
        "record synthetic load XQ-future 0\n"
        "endProvenance\n"
        "diagnostics 0\n"
        "end\n";
    std::filesystem::remove(path);
    if (!write_text_file(path, text)) {
        return fail("write future-min-reader document", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::UnsupportedVersion) {
        return fail("future minimumReaderVersion is rejected", __LINE__);
    }
    if (result.diagnostics.empty()
        || result.diagnostics[0].code() != "PROJECT_UNSUPPORTED_SCHEMA_VERSION") {
        return fail("future minimumReaderVersion rejection is structured", __LINE__);
    }
    return 0;
}

int test_dangling_node_asset_binding_fails()
{
    // A node->asset binding pointing at an asset id that was never declared
    // fails the whole load (§6 ASSET_REFERENCE_INVALID).
    const std::filesystem::path path = temp_project_path("xq_s3_dangling_binding.xqproj");
    const std::string text =
        "XQ_NATIVE_PROJECT schemaVersion 1.2\n"
        "writerVersion XQ-M9-001\n"
        "minimumReaderVersion 1.2\n"
        "createdWith XQrebuild\n"
        "projectId dangling-binding\n"
        "scene\n"
        "nodes 1\n"
        "node 9001 surface.model Surface\n"
        "relations 0\n"
        "stale 0\n"
        "endScene\n"
        "assets 1\n"
        "asset 1 category Derived kind Surface name 0\n"
        "  external 0 absPath 0 relPath 0 dicom 0 fingerprint 0\n"
        "  geometry 0\n"
        "  blobs 0\n"
        "endAsset\n"
        "nodeAssets 1\n"
        "nodeAsset 9001 999\n" // 999 is not a declared asset
        "relations 0\n"
        "endAssets\n"
        "provenance\n"
        "records 1\n"
        "record synthetic load XQ-M9-001 0\n"
        "endProvenance\n"
        "diagnostics 0\n"
        "end\n";
    std::filesystem::remove(path);
    if (!write_text_file(path, text)) {
        return fail("write dangling-binding document", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status == xq::XQProjectReader::Status::Ok) {
        return fail("dangling node->asset binding fails the load", __LINE__);
    }
    if (result.diagnostics.empty()
        || result.diagnostics[0].code() != "ASSET_REFERENCE_INVALID") {
        return fail("dangling binding reports ASSET_REFERENCE_INVALID", __LINE__);
    }
    return 0;
}

} // namespace

int main()
{
    int result = test_asset_metadata_roundtrip();
    if (result != 0) {
        return result;
    }

    result = test_empty_project_writes_empty_assets_section();
    if (result != 0) {
        return result;
    }

    result = test_legacy_no_assets_section_info_diagnostic();
    if (result != 0) {
        return result;
    }

    result = test_minimum_reader_version_rejected();
    if (result != 0) {
        return result;
    }

    result = test_dangling_node_asset_binding_fails();
    if (result != 0) {
        return result;
    }

    return 0;
}
