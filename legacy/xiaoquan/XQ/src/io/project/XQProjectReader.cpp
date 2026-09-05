#include "io/project/XQProjectReader.h"

#include "core/XQAiAnalysisPayload.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolume.h"
#include "core/XQMeshPayload.h"
#include "core/XQPathPayload.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQSimulationCasePayload.h"
#include "core/XQSourcePayload.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"
#include "core/io/PathSafety.h"
#include "io/blob/BlobStore.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

const char* const kMagic = "XQ_NATIVE_PROJECT";
const unsigned int kSupportedMajorVersion = 1;

// Reader capability version (D2). A document whose declared
// minimumReaderVersion exceeds this is rejected with UnsupportedVersion, so a
// 1.2 document (assets section is mandatory-to-read) is refused by any reader
// older than 1.2 instead of silently dropping entities.
const unsigned int kReaderMajor = 1;
const unsigned int kReaderMinor = 2;

struct SchemaVersion {
    unsigned int major;
    unsigned int minor;
};

struct ParsedNode {
    NodeId id;
    std::string domain_type;
    std::string display_name;
};

struct ParsedRelation {
    NodeId source;
    NodeId derived;
};

struct ParsedStaleNode {
    NodeId id;
    XQScene::StaleReason reason;
};

void add_diagnostic(std::vector<Diagnostic>* diagnostics,
                    DiagnosticSeverity severity,
                    const std::string& code,
                    const std::string& message)
{
    if (diagnostics != 0) {
        diagnostics->push_back(Diagnostic(severity, code, message));
    }
}

bool split_line(const std::string& line, std::vector<std::string>* tokens)
{
    if (tokens == 0) {
        return false;
    }

    tokens->clear();
    std::istringstream input(line);
    std::string token;
    while (input >> token) {
        tokens->push_back(token);
    }
    return !tokens->empty();
}

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

bool decode_field(const std::string& text, std::string* out)
{
    if (out == 0) {
        return false;
    }

    std::string decoded;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '%') {
            decoded.push_back(text[i]);
            continue;
        }
        if (i + 2 >= text.size()) {
            return false;
        }
        const int high = hex_value(text[i + 1]);
        const int low = hex_value(text[i + 2]);
        if (high < 0 || low < 0) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }

    *out = decoded;
    return true;
}

bool parse_size(const std::string& text, std::size_t* out)
{
    if (out == 0 || text.empty()) {
        return false;
    }

    std::size_t value = 0;
    const std::size_t max_value = std::numeric_limits<std::size_t>::max();
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (*it < '0' || *it > '9') {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(*it - '0');
        if (value > (max_value - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }

    *out = value;
    return true;
}

bool parse_unsigned_component(const std::string& text, unsigned int* out)
{
    if (out == 0 || text.empty()) {
        return false;
    }

    unsigned int value = 0;
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (*it < '0' || *it > '9') {
            return false;
        }
        const unsigned int digit = static_cast<unsigned int>(*it - '0');
        if (value > (std::numeric_limits<unsigned int>::max() - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }

    *out = value;
    return true;
}

bool parse_schema_version(const std::string& version, SchemaVersion* out)
{
    if (out == 0 || version.empty()) {
        return false;
    }

    const std::size_t dot = version.find('.');
    SchemaVersion parsed = {};
    if (dot == std::string::npos) {
        if (!parse_unsigned_component(version, &parsed.major)) {
            return false;
        }
        parsed.minor = 0;
        *out = parsed;
        return true;
    }

    if (version.find('.', dot + 1) != std::string::npos
        || dot == 0
        || dot + 1 >= version.size()) {
        return false;
    }
    if (!parse_unsigned_component(version.substr(0, dot), &parsed.major)
        || !parse_unsigned_component(version.substr(dot + 1), &parsed.minor)) {
        return false;
    }

    *out = parsed;
    return true;
}

bool is_legacy_v10(const SchemaVersion& version)
{
    return version.major == 1 && version.minor == 0;
}

bool line_is_single_token(const std::vector<std::string>& lines,
                          std::size_t index,
                          const std::string& expected)
{
    if (index >= lines.size()) {
        return false;
    }

    std::vector<std::string> tokens;
    if (!split_line(lines[index], &tokens)) {
        return false;
    }
    return tokens.size() == 1 && tokens[0] == expected;
}

// Peeks whether the line at `index` begins with `expected` (without consuming
// it). Used for section headers that carry a count token, e.g. "assets <N>".
bool line_starts_with_token(const std::vector<std::string>& lines,
                            std::size_t index,
                            const std::string& expected)
{
    if (index >= lines.size()) {
        return false;
    }

    std::vector<std::string> tokens;
    if (!split_line(lines[index], &tokens)) {
        return false;
    }
    return !tokens.empty() && tokens[0] == expected;
}

bool parse_stale_reason(const std::string& text, XQScene::StaleReason* out)
{
    if (out == 0) {
        return false;
    }
    if (text == "SourceChanged") {
        *out = XQScene::StaleReason::SourceChanged;
        return true;
    }
    if (text == "None") {
        *out = XQScene::StaleReason::None;
        return true;
    }
    return false;
}

bool read_line_tokens(const std::vector<std::string>& lines,
                      std::size_t* index,
                      std::vector<std::string>* tokens)
{
    if (index == 0 || tokens == 0 || *index >= lines.size()) {
        return false;
    }

    if (!split_line(lines[*index], tokens)) {
        return false;
    }
    ++(*index);
    return true;
}

bool expect_single_token_line(const std::vector<std::string>& lines,
                              std::size_t* index,
                              const std::string& expected)
{
    std::vector<std::string> tokens;
    if (!read_line_tokens(lines, index, &tokens)) {
        return false;
    }
    return tokens.size() == 1 && tokens[0] == expected;
}

bool expect_field_line(const std::vector<std::string>& lines,
                       std::size_t* index,
                       const std::string& name,
                       std::string* value)
{
    std::vector<std::string> tokens;
    if (!read_line_tokens(lines, index, &tokens)) {
        return false;
    }
    if (tokens.size() != 2 || tokens[0] != name) {
        return false;
    }
    if (value != 0) {
        *value = tokens[1];
    }
    return true;
}

bool expect_count_line(const std::vector<std::string>& lines,
                       std::size_t* index,
                       const std::string& name,
                       std::size_t* count)
{
    std::string value;
    return expect_field_line(lines, index, name, &value) && parse_size(value, count);
}

bool parse_provenance_section(const std::vector<std::string>& lines,
                              std::size_t* index,
                              std::vector<std::string>* tokens,
                              std::size_t* provenance_count)
{
    if (index == 0 || tokens == 0 || provenance_count == 0) {
        return false;
    }

    if (!expect_single_token_line(lines, index, "provenance")
        || !expect_count_line(lines, index, "records", provenance_count)) {
        return false;
    }
    for (std::size_t i = 0; i < *provenance_count; ++i) {
        if (!read_line_tokens(lines, index, tokens)
            || tokens->size() != 5
            || (*tokens)[0] != "record") {
            return false;
        }
    }
    if (!expect_single_token_line(lines, index, "endProvenance")) {
        return false;
    }

    return true;
}

bool parse_node_line(const std::vector<std::string>& tokens, ParsedNode* out)
{
    if (out == 0 || tokens.size() != 4 || tokens[0] != "node") {
        return false;
    }

    ParsedNode node = {};
    if (!NodeId::deserialize(tokens[1], &node.id)
        || !node.id.is_valid()
        || !decode_field(tokens[2], &node.domain_type)
        || !decode_field(tokens[3], &node.display_name)) {
        return false;
    }

    *out = node;
    return true;
}

bool parse_relation_line(const std::vector<std::string>& tokens, ParsedRelation* out)
{
    if (out == 0 || tokens.size() != 3 || tokens[0] != "derived") {
        return false;
    }

    ParsedRelation relation = {};
    if (!NodeId::deserialize(tokens[1], &relation.source)
        || !NodeId::deserialize(tokens[2], &relation.derived)
        || !relation.source.is_valid()
        || !relation.derived.is_valid()) {
        return false;
    }

    *out = relation;
    return true;
}

bool parse_stale_node_line(const std::vector<std::string>& tokens, ParsedStaleNode* out)
{
    if (out == 0 || tokens.size() != 3 || tokens[0] != "staleNode") {
        return false;
    }

    ParsedStaleNode stale_node = {};
    if (!NodeId::deserialize(tokens[1], &stale_node.id)
        || !stale_node.id.is_valid()
        || !parse_stale_reason(tokens[2], &stale_node.reason)) {
        return false;
    }

    *out = stale_node;
    return true;
}

// ---- assets section (schema 1.2) ----------------------------------------

struct ParsedBlob {
    std::string role;
    BufferRef ref;
};

struct ParsedAsset {
    AssetId id;
    AssetCategory category = AssetCategory::Derived;
    AssetKind kind = AssetKind::Surface;
    bool hasName = false;
    std::string displayName;
    bool external = false;
    bool hasAbsPath = false;
    std::string absPath;
    bool hasRelPath = false;
    std::string relPath;
    bool hasDicom = false;
    DicomSeriesIdentity dicom;
    bool hasFingerprint = false;
    std::string fingerprint;
    bool hasGeometry = false;
    ImageGeometry geometry{};
    std::vector<ParsedBlob> blobs;

    // Optional entity payload text block (S4). When present, payloadKind names
    // the block ("source"/"path"/"segMask"/...) and payloadLines holds its body
    // lines (between "payload" and "endPayload") as token vectors, rebuilt into
    // a concrete payload in pass two.
    bool hasPayload = false;
    std::string payloadKind;
    std::vector<std::vector<std::string>> payloadLines;
};

struct ParsedNodeAsset {
    NodeId node;
    AssetId asset;
};

struct ParsedAssetRelation {
    AssetId source;
    AssetId derived;
};

bool parse_asset_category(const std::string& text, AssetCategory* out)
{
    if (text == "ExternalSource") {
        *out = AssetCategory::ExternalSource;
    } else if (text == "ManagedOriginal") {
        *out = AssetCategory::ManagedOriginal;
    } else if (text == "ManagedCanonical") {
        *out = AssetCategory::ManagedCanonical;
    } else if (text == "Derived") {
        *out = AssetCategory::Derived;
    } else {
        return false;
    }
    return true;
}

bool parse_asset_kind(const std::string& text, AssetKind* out)
{
    if (text == "Image") {
        *out = AssetKind::Image;
    } else if (text == "SegmentationMask") {
        *out = AssetKind::SegmentationMask;
    } else if (text == "Surface") {
        *out = AssetKind::Surface;
    } else if (text == "Mesh") {
        *out = AssetKind::Mesh;
    } else if (text == "SimulationCase") {
        *out = AssetKind::SimulationCase;
    } else if (text == "FlowResult") {
        *out = AssetKind::FlowResult;
    } else if (text == "AiAnalysis") {
        *out = AssetKind::AiAnalysis;
    } else if (text == "Path") {
        *out = AssetKind::Path;
    } else if (text == "Contour") {
        *out = AssetKind::Contour;
    } else {
        return false;
    }
    return true;
}

bool parse_coordinate_system(const std::string& text, ImageCoordinateSystem* out)
{
    if (text == "LPS") {
        *out = ImageCoordinateSystem::LPS;
    } else if (text == "RAS") {
        *out = ImageCoordinateSystem::RAS;
    } else {
        return false;
    }
    return true;
}

bool parse_double(const std::string& text, double* out)
{
    if (out == 0 || text.empty()) {
        return false;
    }
    std::istringstream stream(text);
    double value = 0.0;
    stream >> value;
    if (!stream || !stream.eof()) {
        return false;
    }
    *out = value;
    return true;
}

bool parse_int(const std::string& text, int* out)
{
    if (out == 0 || text.empty()) {
        return false;
    }
    std::istringstream stream(text);
    int value = 0;
    stream >> value;
    if (!stream || !stream.eof()) {
        return false;
    }
    *out = value;
    return true;
}

bool parse_u64(const std::string& text, std::uint64_t* out)
{
    if (out == 0 || text.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    const std::uint64_t max_value = std::numeric_limits<std::uint64_t>::max();
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (*it < '0' || *it > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(*it - '0');
        if (value > (max_value - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }
    *out = value;
    return true;
}

// Reads "<present 0|1> [<encoded>]" starting at *cursor. On present=1 consumes
// the encoded token and decodes it; on present=0 leaves *value empty.
bool parse_optional_string(const std::vector<std::string>& tokens,
                           std::size_t* cursor,
                           bool* present,
                           std::string* value)
{
    if (cursor == 0 || present == 0 || value == 0 || *cursor >= tokens.size()) {
        return false;
    }
    const std::string& flag = tokens[*cursor];
    ++(*cursor);
    if (flag == "0") {
        *present = false;
        value->clear();
        return true;
    }
    if (flag != "1") {
        return false;
    }
    if (*cursor >= tokens.size()) {
        return false;
    }
    if (!decode_field(tokens[*cursor], value)) {
        return false;
    }
    ++(*cursor);
    *present = true;
    return true;
}

// Expects the literal token `name` at *cursor and advances past it.
bool expect_keyword(const std::vector<std::string>& tokens,
                    std::size_t* cursor,
                    const char* name)
{
    if (cursor == 0 || *cursor >= tokens.size() || tokens[*cursor] != name) {
        return false;
    }
    ++(*cursor);
    return true;
}

// Parses the "blob <role> <relPath> <byteCount> <sha256> <formatVersion>
// <endianness> <elementType> <components> <elementCount>" line (10 tokens).
bool parse_blob_line(const std::vector<std::string>& tokens, ParsedBlob* out)
{
    if (out == 0 || tokens.size() != 10 || tokens[0] != "blob") {
        return false;
    }

    ParsedBlob blob = {};
    if (!decode_field(tokens[1], &blob.role)
        || !decode_field(tokens[2], &blob.ref.relPath)) {
        return false;
    }
    if (!parse_u64(tokens[3], &blob.ref.byteCount)) {
        return false;
    }
    if (!decode_field(tokens[4], &blob.ref.sha256)) {
        return false;
    }

    std::uint64_t format_version = 0;
    std::uint64_t endianness = 0;
    std::uint64_t element_type = 0;
    std::uint64_t components = 0;
    if (!parse_u64(tokens[5], &format_version)
        || !parse_u64(tokens[6], &endianness)
        || !parse_u64(tokens[7], &element_type)
        || !parse_u64(tokens[8], &components)
        || !parse_u64(tokens[9], &blob.ref.elementCount)) {
        return false;
    }
    if (format_version > std::numeric_limits<std::uint32_t>::max()
        || endianness > std::numeric_limits<std::uint8_t>::max()
        || components > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    if (element_type < static_cast<std::uint64_t>(BlobElementType::F64)
        || element_type > static_cast<std::uint64_t>(BlobElementType::I32)) {
        return false;
    }
    blob.ref.formatVersion = static_cast<std::uint32_t>(format_version);
    blob.ref.endianness = static_cast<std::uint8_t>(endianness);
    blob.ref.elementType = static_cast<BlobElementType>(element_type);
    blob.ref.components = static_cast<std::uint16_t>(components);

    *out = blob;
    return true;
}

// Parses a single asset record (the "asset ..." line through "endAsset").
bool parse_asset(const std::vector<std::string>& lines, std::size_t* index, ParsedAsset* out)
{
    std::vector<std::string> tokens;

    // asset <id> category <Cat> kind <Kind> name <present 0|1> [<encoded>]
    if (!read_line_tokens(lines, index, &tokens)
        || tokens.size() < 8
        || tokens[0] != "asset"
        || tokens[2] != "category"
        || tokens[4] != "kind"
        || tokens[6] != "name") {
        return false;
    }
    ParsedAsset asset = {};
    if (!AssetId::deserialize(tokens[1], &asset.id) || !asset.id.is_valid()) {
        return false;
    }
    if (!parse_asset_category(tokens[3], &asset.category)
        || !parse_asset_kind(tokens[5], &asset.kind)) {
        return false;
    }
    {
        std::size_t cursor = 7;
        if (!parse_optional_string(tokens, &cursor, &asset.hasName, &asset.displayName)
            || cursor != tokens.size()) {
            return false;
        }
    }

    // external <0|1> absPath <opt> relPath <opt> dicom <0|1> [study <opt> series
    // <opt> frame <opt>] fingerprint <opt>
    if (!read_line_tokens(lines, index, &tokens) || tokens.empty() || tokens[0] != "external") {
        return false;
    }
    {
        std::size_t cursor = 1;
        if (cursor >= tokens.size()) {
            return false;
        }
        if (tokens[cursor] == "1") {
            asset.external = true;
        } else if (tokens[cursor] == "0") {
            asset.external = false;
        } else {
            return false;
        }
        ++cursor;
        bool ignored = false;
        if (!expect_keyword(tokens, &cursor, "absPath")
            || !parse_optional_string(tokens, &cursor, &asset.hasAbsPath, &asset.absPath)
            || !expect_keyword(tokens, &cursor, "relPath")
            || !parse_optional_string(tokens, &cursor, &asset.hasRelPath, &asset.relPath)
            || !expect_keyword(tokens, &cursor, "dicom")) {
            return false;
        }
        if (cursor >= tokens.size()) {
            return false;
        }
        if (tokens[cursor] == "1") {
            asset.hasDicom = true;
        } else if (tokens[cursor] == "0") {
            asset.hasDicom = false;
        } else {
            return false;
        }
        ++cursor;
        if (asset.hasDicom) {
            if (!expect_keyword(tokens, &cursor, "study")
                || !parse_optional_string(tokens, &cursor, &ignored, &asset.dicom.studyInstanceUid)
                || !expect_keyword(tokens, &cursor, "series")
                || !parse_optional_string(tokens, &cursor, &ignored, &asset.dicom.seriesInstanceUid)
                || !expect_keyword(tokens, &cursor, "frame")
                || !parse_optional_string(tokens, &cursor, &ignored, &asset.dicom.frameOfReferenceUid)) {
                return false;
            }
        }
        if (!expect_keyword(tokens, &cursor, "fingerprint")
            || !parse_optional_string(tokens, &cursor, &asset.hasFingerprint, &asset.fingerprint)
            || cursor != tokens.size()) {
            return false;
        }
    }

    // geometry <0|1> [dims 3i spacing 3d origin 3d direction 9d coordSys LPS|RAS]
    if (!read_line_tokens(lines, index, &tokens) || tokens.empty() || tokens[0] != "geometry") {
        return false;
    }
    {
        std::size_t cursor = 1;
        if (cursor >= tokens.size()) {
            return false;
        }
        if (tokens[cursor] == "1") {
            asset.hasGeometry = true;
        } else if (tokens[cursor] == "0") {
            asset.hasGeometry = false;
        } else {
            return false;
        }
        ++cursor;
        if (asset.hasGeometry) {
            ImageGeometry& g = asset.geometry;
            if (!expect_keyword(tokens, &cursor, "dims")) {
                return false;
            }
            for (int i = 0; i < 3; ++i) {
                if (cursor >= tokens.size() || !parse_int(tokens[cursor], &g.dimensions[i])) {
                    return false;
                }
                ++cursor;
            }
            if (!expect_keyword(tokens, &cursor, "spacing")) {
                return false;
            }
            for (int i = 0; i < 3; ++i) {
                if (cursor >= tokens.size() || !parse_double(tokens[cursor], &g.spacing[i])) {
                    return false;
                }
                ++cursor;
            }
            if (!expect_keyword(tokens, &cursor, "origin")) {
                return false;
            }
            for (int i = 0; i < 3; ++i) {
                if (cursor >= tokens.size() || !parse_double(tokens[cursor], &g.origin[i])) {
                    return false;
                }
                ++cursor;
            }
            if (!expect_keyword(tokens, &cursor, "direction")) {
                return false;
            }
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    if (cursor >= tokens.size() || !parse_double(tokens[cursor], &g.direction[r][c])) {
                        return false;
                    }
                    ++cursor;
                }
            }
            if (!expect_keyword(tokens, &cursor, "coordSys")
                || cursor >= tokens.size()
                || !parse_coordinate_system(tokens[cursor], &g.coordinateSystem)) {
                return false;
            }
            ++cursor;
        }
        if (cursor != tokens.size()) {
            return false;
        }
    }

    // blobs <B> followed by B blob lines.
    std::size_t blob_count = 0;
    if (!expect_count_line(lines, index, "blobs", &blob_count)) {
        return false;
    }
    asset.blobs.reserve(blob_count);
    std::set<std::string> blob_roles;
    for (std::size_t i = 0; i < blob_count; ++i) {
        if (!read_line_tokens(lines, index, &tokens)) {
            return false;
        }
        ParsedBlob blob = {};
        if (!parse_blob_line(tokens, &blob)) {
            return false;
        }
        // Roles must be unique within an asset (§6 ASSET_REFERENCE_INVALID).
        if (!blob_roles.insert(blob.role).second) {
            return false;
        }
        asset.blobs.push_back(blob);
    }

    // Optional entity payload block: "payload <kind>" ... "endPayload". Peeked
    // with line_starts_with_token (first-token rule, memory
    // xq-section-header-peek-needs-first-token) so a hand-written asset with no
    // payload block (only "endAsset" next) parses cleanly.
    if (line_starts_with_token(lines, *index, "payload")) {
        if (!read_line_tokens(lines, index, &tokens)
            || tokens.size() != 2
            || tokens[0] != "payload") {
            return false;
        }
        asset.hasPayload = true;
        asset.payloadKind = tokens[1];
        while (!line_is_single_token(lines, *index, "endPayload")) {
            if (*index >= lines.size()) {
                return false; // unterminated payload block
            }
            if (!read_line_tokens(lines, index, &tokens)) {
                return false;
            }
            asset.payloadLines.push_back(tokens);
        }
        if (!expect_single_token_line(lines, index, "endPayload")) {
            return false;
        }
    }

    if (!expect_single_token_line(lines, index, "endAsset")) {
        return false;
    }

    *out = asset;
    return true;
}

// ---- entity payload rebuild (S4) ----------------------------------------
//
// Pass two rebuilds a concrete XQPayload from an asset's payload text block
// (small scalars) plus its blobs (large arrays, fetched + verified via the
// BlobStore). The text layout mirrors XQProjectWriter's write_payload_* exactly.

// Cursor-based token readers over a single tokenized line.
bool tok_keyword(const std::vector<std::string>& t, std::size_t* c, const char* name)
{
    if (*c >= t.size() || t[*c] != name) {
        return false;
    }
    ++(*c);
    return true;
}

bool tok_int(const std::vector<std::string>& t, std::size_t* c, int* out)
{
    if (*c >= t.size() || !parse_int(t[*c], out)) {
        return false;
    }
    ++(*c);
    return true;
}

bool tok_double(const std::vector<std::string>& t, std::size_t* c, double* out)
{
    if (*c >= t.size() || !parse_double(t[*c], out)) {
        return false;
    }
    ++(*c);
    return true;
}

bool tok_size(const std::vector<std::string>& t, std::size_t* c, std::size_t* out)
{
    if (*c >= t.size() || !parse_size(t[*c], out)) {
        return false;
    }
    ++(*c);
    return true;
}

// Reads "<0|1>" into a bool.
bool tok_flag(const std::vector<std::string>& t, std::size_t* c, bool* out)
{
    if (*c >= t.size()) {
        return false;
    }
    if (t[*c] == "1") {
        *out = true;
    } else if (t[*c] == "0") {
        *out = false;
    } else {
        return false;
    }
    ++(*c);
    return true;
}

// Reads the writer's optional-string form "<present 0|1> [<encoded>]".
bool tok_optional_string(const std::vector<std::string>& t, std::size_t* c,
                         bool* present, std::string* value)
{
    return parse_optional_string(t, c, present, value);
}

// Reads an optional NodeId written as "<id>" or "-" (no binding).
bool tok_optional_node(const std::vector<std::string>& t, std::size_t* c,
                       bool* has, NodeId* node)
{
    if (*c >= t.size()) {
        return false;
    }
    if (t[*c] == "-") {
        *has = false;
        ++(*c);
        return true;
    }
    if (!NodeId::deserialize(t[*c], node) || !node->is_valid()) {
        return false;
    }
    *has = true;
    ++(*c);
    return true;
}

// Fetches and verifies a blob by role; decodes into the typed vector. Sets
// *status to a structured BlobStore status; returns false on any failure.
struct BlobErrorCode {
    static const char* of(BlobStore::Status s)
    {
        switch (s) {
        case BlobStore::Status::MissingBlob:
            return "ASSET_BLOB_MISSING";
        case BlobStore::Status::TruncatedBlob:
            return "ASSET_BLOB_TRUNCATED";
        case BlobStore::Status::ByteCountMismatch:
            return "ASSET_BLOB_BYTECOUNT_MISMATCH";
        case BlobStore::Status::ChecksumMismatch:
            return "ASSET_BLOB_CHECKSUM_MISMATCH";
        case BlobStore::Status::InvalidMetadata:
            return "ASSET_BLOB_INVALID_METADATA";
        case BlobStore::Status::Ok:
            return "";
        }
        return "";
    }
};

const ParsedBlob* find_blob(const ParsedAsset& asset, const char* role)
{
    for (std::vector<ParsedBlob>::const_iterator it = asset.blobs.begin();
         it != asset.blobs.end();
         ++it) {
        if (it->role == role) {
            return &(*it);
        }
    }
    return 0;
}

bool is_hex_sha256(const std::string& text)
{
    if (text.size() != 64) {
        return false;
    }
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (hex_value(*it) < 0) {
            return false;
        }
    }
    return true;
}

bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out)
{
    if (out == 0) {
        return false;
    }
    if (a != 0 && b > (std::numeric_limits<std::uint64_t>::max)() / a) {
        return false;
    }
    *out = a * b;
    return true;
}

bool checked_ref_byte_count(const BufferRef& ref, std::uint64_t* out)
{
    if (ref.formatVersion != 1 || ref.endianness != 0 || ref.components == 0
        || !isConfinedRelativePath(ref.relPath) || !is_hex_sha256(ref.sha256)) {
        return false;
    }
    const std::size_t width = BlobStore::byteWidth(ref.elementType);
    if (width == 0) {
        return false;
    }
    std::uint64_t scalars = 0;
    std::uint64_t bytes = 0;
    if (!checked_mul_u64(ref.elementCount, ref.components, &scalars)
        || !checked_mul_u64(scalars, static_cast<std::uint64_t>(width), &bytes)) {
        return false;
    }
    if (ref.byteCount != bytes) {
        return false;
    }
    if (out != 0) {
        *out = bytes;
    }
    return true;
}

struct ExpectedBlobSchema {
    BlobElementType type;
    std::uint16_t components;
};

bool expected_blob_schema(const std::string& role, ExpectedBlobSchema* out)
{
    ExpectedBlobSchema schema = {};
    if (role == "points" || role == "surfPoints" || role == "volPoints") {
        schema.type = BlobElementType::F64;
        schema.components = 3;
    } else if (role == "tris" || role == "surfTris") {
        schema.type = BlobElementType::I32;
        schema.components = 3;
    } else if (role == "faceId" || role == "surfFaceId") {
        schema.type = BlobElementType::I32;
        schema.components = 1;
    } else if (role == "tets") {
        schema.type = BlobElementType::I32;
        schema.components = 4;
    } else if (role == "voxels") {
        schema.type = BlobElementType::U8;
        schema.components = 1;
    } else {
        return false;
    }

    if (out != 0) {
        *out = schema;
    }
    return true;
}

bool validate_blob_role_schema(const ParsedBlob& blob)
{
    ExpectedBlobSchema expected = {};
    if (!expected_blob_schema(blob.role, &expected)) {
        return false;
    }
    return blob.ref.elementType == expected.type
        && blob.ref.components == expected.components
        && checked_ref_byte_count(blob.ref, 0);
}

bool validate_complete_group(const ParsedAsset& asset,
                             const char* aRole,
                             const char* bRole,
                             const char* cRole)
{
    const ParsedBlob* b = find_blob(asset, bRole);
    const ParsedBlob* c = cRole != 0 ? find_blob(asset, cRole) : 0;
    (void)aRole;
    if (c != 0 && b == 0) {
        return false;
    }
    if (c != 0 && c->ref.elementCount != b->ref.elementCount) {
        return false;
    }
    return true;
}

bool validate_asset_blob_schemas(const ParsedAsset& asset)
{
    std::set<std::string> roles;
    for (std::vector<ParsedBlob>::const_iterator it = asset.blobs.begin();
         it != asset.blobs.end();
         ++it) {
        if (!roles.insert(it->role).second || !validate_blob_role_schema(*it)) {
            return false;
        }
    }

    return validate_complete_group(asset, "points", "tris", "faceId")
        && validate_complete_group(asset, "surfPoints", "surfTris", "surfFaceId")
        && validate_complete_group(asset, "volPoints", "tets", 0);
}

bool checked_voxel_count_from_dims(const int dims[3], std::uint64_t* out)
{
    if (out == 0 || dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0) {
        return false;
    }
    std::uint64_t xy = 0;
    return checked_mul_u64(static_cast<std::uint64_t>(dims[0]),
                           static_cast<std::uint64_t>(dims[1]), &xy)
        && checked_mul_u64(xy, static_cast<std::uint64_t>(dims[2]), out);
}

// Rebuilds a triangle surface handle from three blob roles (points/tris/faceId).
// Returns false (with *blobStatus set) on any blob fetch/verify error.
bool rebuild_triangle_geometry(BlobStore& store, const ParsedAsset& asset,
                               const char* pointsRole, const char* trisRole,
                               const char* faceIdRole,
                               std::shared_ptr<XQTriangleSurfaceGeometryHandle>* out,
                               BlobStore::Status* blobStatus)
{
    const ParsedBlob* pointsBlob = find_blob(asset, pointsRole);
    const ParsedBlob* trisBlob = find_blob(asset, trisRole);
    const ParsedBlob* faceIdBlob = find_blob(asset, faceIdRole);
    if (pointsBlob == 0 || trisBlob == 0 || faceIdBlob == 0) {
        *blobStatus = BlobStore::Status::MissingBlob;
        return false;
    }

    std::vector<double> points;
    std::vector<int> tris;
    std::vector<int> faceIds;
    const BlobStore::Status sp = store.get(pointsBlob->ref, &points);
    if (sp != BlobStore::Status::Ok) {
        *blobStatus = sp;
        return false;
    }
    const BlobStore::Status st = store.get(trisBlob->ref, &tris);
    if (st != BlobStore::Status::Ok) {
        *blobStatus = st;
        return false;
    }
    const BlobStore::Status sf = store.get(faceIdBlob->ref, &faceIds);
    if (sf != BlobStore::Status::Ok) {
        *blobStatus = sf;
        return false;
    }

    auto handle = std::make_shared<XQTriangleSurfaceGeometryHandle>();
    const std::size_t pointCount = points.size() / 3;
    for (std::size_t i = 0; i < pointCount; ++i) {
        Point3 p = {points[i * 3], points[i * 3 + 1], points[i * 3 + 2]};
        handle->addPoint(p);
    }
    const std::size_t triCount = tris.size() / 3;
    for (std::size_t i = 0; i < triCount; ++i) {
        const int faceId = (i < faceIds.size()) ? faceIds[i] : 0;
        handle->addTriangle(tris[i * 3], tris[i * 3 + 1], tris[i * 3 + 2], faceId);
    }
    *out = handle;
    return true;
}

// Result of rebuilding one payload: Ok, a structural text error, or a blob
// fetch/verify error (which maps to a structured ASSET_BLOB_* diagnostic).
enum class PayloadRebuild {
    Ok,
    TextError,
    BlobError
};

bool find_payload_line(const ParsedAsset& asset, const char* head,
                       const std::vector<std::string>** out)
{
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (!it->empty() && (*it)[0] == head) {
            *out = &(*it);
            return true;
        }
    }
    return false;
}

PayloadRebuild rebuild_source(const ParsedAsset& asset, std::shared_ptr<XQPayload>* out)
{
    const std::vector<std::string>* line = 0;
    if (!find_payload_line(asset, "source", &line) || line->size() < 2) {
        return PayloadRebuild::TextError;
    }
    // source <domainToken> <present 0|1> [<encoded path>]
    XQDomainType domain = XQDomainType::Unknown;
    const std::string& token = (*line)[1];
    if (token == "image") {
        domain = XQDomainType::Image;
    } else if (token == "contour_group") {
        domain = XQDomainType::ContourGroup;
    } else if (token == "segmentation_mask") {
        domain = XQDomainType::SegmentationMask;
    } else if (token == "path") {
        domain = XQDomainType::Path;
    } else {
        domain = XQDomainType::Unknown;
    }
    std::size_t c = 2;
    bool present = false;
    std::string path;
    if (!tok_optional_string(*line, &c, &present, &path) || c != line->size()) {
        return PayloadRebuild::TextError;
    }
    *out = std::make_shared<XQSourcePayload>(domain, path);
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_path(const ParsedAsset& asset, std::shared_ptr<XQPayload>* out)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "pathId", &head)) {
        return PayloadRebuild::TextError;
    }
    // pathId <id> interp <Polyline|Spline> sampleSpacing <d> sourceImage <id|->
    std::size_t c = 1;
    NodeId pathId;
    if (c >= head->size() || !NodeId::deserialize((*head)[c], &pathId)) {
        return PayloadRebuild::TextError;
    }
    ++c;
    std::string interpText;
    double spacing = 0.0;
    bool hasSrc = false;
    NodeId srcImage;
    if (!tok_keyword(*head, &c, "interp") || c >= head->size()) {
        return PayloadRebuild::TextError;
    }
    interpText = (*head)[c];
    ++c;
    if (!tok_keyword(*head, &c, "sampleSpacing") || !tok_double(*head, &c, &spacing)
        || !tok_keyword(*head, &c, "sourceImage") || !tok_optional_node(*head, &c, &hasSrc, &srcImage)
        || c != head->size()) {
        return PayloadRebuild::TextError;
    }
    PathInterpolation interp = PathInterpolation::Polyline;
    if (interpText == "Spline") {
        interp = PathInterpolation::Spline;
    } else if (interpText != "Polyline") {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* cpHead = 0;
    if (!find_payload_line(asset, "controlPoints", &cpHead) || cpHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t cpCount = 0;
    if (!parse_size((*cpHead)[1], &cpCount)) {
        return PayloadRebuild::TextError;
    }
    std::vector<PathControlPoint> controlPoints;
    controlPoints.reserve(cpCount);
    std::size_t seen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "cp") {
            continue;
        }
        if (it->size() != 4) {
            return PayloadRebuild::TextError;
        }
        Point3 p = {};
        if (!parse_double((*it)[1], &p.x) || !parse_double((*it)[2], &p.y)
            || !parse_double((*it)[3], &p.z)) {
            return PayloadRebuild::TextError;
        }
        PathControlPoint cp = {};
        cp.position = p;
        controlPoints.push_back(cp);
        ++seen;
    }
    if (seen != cpCount) {
        return PayloadRebuild::TextError;
    }

    XQPath path;
    path.setId(pathId);
    path.setInterpolation(interp);
    if (hasSrc) {
        path.setSourceImageNode(srcImage);
    }
    path.setControlPoints(controlPoints);
    if (spacing > 0.0) {
        // A hostile spacing (tiny value vs huge arc length) must fail the load,
        // not silently hang. resample enforces a sample-count ceiling.
        if (path.resample(spacing) != XQPath::ResampleStatus::Ok) {
            return PayloadRebuild::TextError;
        }
    }
    *out = std::make_shared<XQPathPayload>(std::move(path));
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_seg_mask(BlobStore& store, const ParsedAsset& asset,
                                std::shared_ptr<XQPayload>* out, BlobStore::Status* blobStatus)
{
    const std::vector<std::string>* dimsLine = 0;
    if (!find_payload_line(asset, "maskDims", &dimsLine) || dimsLine->size() != 4) {
        return PayloadRebuild::TextError;
    }
    int dims[3] = {0, 0, 0};
    if (!parse_int((*dimsLine)[1], &dims[0]) || !parse_int((*dimsLine)[2], &dims[1])
        || !parse_int((*dimsLine)[3], &dims[2])) {
        return PayloadRebuild::TextError;
    }
    std::uint64_t expectedVoxels = 0;
    if (!checked_voxel_count_from_dims(dims, &expectedVoxels)) {
        return PayloadRebuild::TextError;
    }

    XQSegmentationMask mask(dims);

    const std::vector<std::string>* geomLine = 0;
    if (!find_payload_line(asset, "maskGeometry", &geomLine) || geomLine->size() < 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t c = 1;
    bool hasGeom = false;
    if (!tok_flag(*geomLine, &c, &hasGeom)) {
        return PayloadRebuild::TextError;
    }
    if (hasGeom) {
        ImageGeometry g = {};
        g.dimensions[0] = dims[0];
        g.dimensions[1] = dims[1];
        g.dimensions[2] = dims[2];
        if (!tok_keyword(*geomLine, &c, "spacing")) {
            return PayloadRebuild::TextError;
        }
        for (int i = 0; i < 3; ++i) {
            if (!tok_double(*geomLine, &c, &g.spacing[i])) {
                return PayloadRebuild::TextError;
            }
        }
        if (!tok_keyword(*geomLine, &c, "origin")) {
            return PayloadRebuild::TextError;
        }
        for (int i = 0; i < 3; ++i) {
            if (!tok_double(*geomLine, &c, &g.origin[i])) {
                return PayloadRebuild::TextError;
            }
        }
        if (!tok_keyword(*geomLine, &c, "direction")) {
            return PayloadRebuild::TextError;
        }
        for (int r = 0; r < 3; ++r) {
            for (int col = 0; col < 3; ++col) {
                if (!tok_double(*geomLine, &c, &g.direction[r][col])) {
                    return PayloadRebuild::TextError;
                }
            }
        }
        if (!tok_keyword(*geomLine, &c, "coordSys") || c >= geomLine->size()
            || !parse_coordinate_system((*geomLine)[c], &g.coordinateSystem)) {
            return PayloadRebuild::TextError;
        }
        ++c;
        mask.setGeometry(g);
    }
    if (c != geomLine->size()) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* srcLine = 0;
    if (!find_payload_line(asset, "maskSourceImage", &srcLine) || srcLine->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t sc = 1;
    bool hasSrc = false;
    NodeId srcNode;
    if (!tok_optional_node(*srcLine, &sc, &hasSrc, &srcNode) || sc != srcLine->size()) {
        return PayloadRebuild::TextError;
    }
    if (hasSrc) {
        mask.setSourceImageNode(srcNode);
    }

    const std::vector<std::string>* labelsHead = 0;
    if (!find_payload_line(asset, "labels", &labelsHead) || labelsHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t labelCount = 0;
    if (!parse_size((*labelsHead)[1], &labelCount)) {
        return PayloadRebuild::TextError;
    }
    std::vector<SegmentationLabel> labels;
    labels.reserve(labelCount);
    std::size_t labelsSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "label") {
            continue;
        }
        std::size_t lc = 1;
        int value = 0;
        bool namePresent = false;
        std::string name;
        if (!tok_int(*it, &lc, &value)
            || !tok_optional_string(*it, &lc, &namePresent, &name)
            || lc != it->size()) {
            return PayloadRebuild::TextError;
        }
        SegmentationLabel label = {};
        label.value = value;
        label.name = name;
        labels.push_back(label);
        ++labelsSeen;
    }
    if (labelsSeen != labelCount) {
        return PayloadRebuild::TextError;
    }
    mask.setLabels(labels);

    // voxels blob.
    const ParsedBlob* voxelsBlob = find_blob(asset, "voxels");
    if (voxelsBlob == 0) {
        *blobStatus = BlobStore::Status::MissingBlob;
        return PayloadRebuild::BlobError;
    }
    if (voxelsBlob->ref.elementCount != expectedVoxels) {
        *blobStatus = BlobStore::Status::InvalidMetadata;
        return PayloadRebuild::BlobError;
    }
    std::vector<std::uint8_t> voxels;
    const BlobStore::Status sv = store.get(voxelsBlob->ref, &voxels);
    if (sv != BlobStore::Status::Ok) {
        *blobStatus = sv;
        return PayloadRebuild::BlobError;
    }
    for (std::size_t i = 0; i < voxels.size(); ++i) {
        mask.setLabelAt(i, voxels[i]);
    }

    *out = std::make_shared<XQSegmentationMaskPayload>(std::move(mask));
    return PayloadRebuild::Ok;
}

// Parses the shared "face descriptor" tail: kind, optional cap, name, written by
// write_face_kind_cap_name. Advances *c past the name.
bool parse_face_kind_cap_name(const std::vector<std::string>& t, std::size_t* c,
                              FaceKind* kind, std::optional<int>* capId, std::string* name)
{
    int kindInt = 0;
    if (!tok_int(t, c, &kindInt) || kindInt < 0
        || kindInt > static_cast<int>(FaceKind::Outlet)) {
        return false;
    }
    *kind = static_cast<FaceKind>(kindInt);
    bool hasCap = false;
    int capValue = 0;
    if (!tok_flag(t, c, &hasCap) || !tok_int(t, c, &capValue)) {
        return false;
    }
    if (hasCap) {
        *capId = capValue;
    } else {
        capId->reset();
    }
    bool namePresent = false;
    if (!tok_optional_string(t, c, &namePresent, name)) {
        return false;
    }
    return true;
}

PayloadRebuild rebuild_surface(BlobStore& store, const ParsedAsset& asset,
                               std::shared_ptr<XQPayload>* out, BlobStore::Status* blobStatus,
                               bool lazyGeometry)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "surfaceId", &head)) {
        return PayloadRebuild::TextError;
    }
    // surfaceId <id> source <Unknown|Loaded|Generated> sourceContour <id|->
    std::size_t c = 1;
    NodeId modelId;
    if (c >= head->size() || !NodeId::deserialize((*head)[c], &modelId)) {
        return PayloadRebuild::TextError;
    }
    ++c;
    if (!tok_keyword(*head, &c, "source") || c >= head->size()) {
        return PayloadRebuild::TextError;
    }
    ModelSource source = ModelSource::Unknown;
    if ((*head)[c] == "Loaded") {
        source = ModelSource::Loaded;
    } else if ((*head)[c] == "Generated") {
        source = ModelSource::Generated;
    } else if ((*head)[c] != "Unknown") {
        return PayloadRebuild::TextError;
    }
    ++c;
    bool hasContour = false;
    NodeId contourNode;
    if (!tok_keyword(*head, &c, "sourceContour")
        || !tok_optional_node(*head, &c, &hasContour, &contourNode) || c != head->size()) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* preservedLine = 0;
    if (!find_payload_line(asset, "preserved", &preservedLine) || preservedLine->size() != 5) {
        return PayloadRebuild::TextError;
    }
    PreservedVtpArrays preserved = {};
    std::size_t pc = 1;
    if (!tok_flag(*preservedLine, &pc, &preserved.hasGlobalNodeID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasGlobalElementID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasModelFaceID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasCapID)) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* triLine = 0;
    if (!find_payload_line(asset, "hasTriGeom", &triLine) || triLine->size() != 2) {
        return PayloadRebuild::TextError;
    }
    bool hasTri = false;
    std::size_t tc = 1;
    if (!tok_flag(*triLine, &tc, &hasTri)) {
        return PayloadRebuild::TextError;
    }

    XQSurfaceModel model;
    model.setId(modelId);
    model.setSource(source);
    if (hasContour) {
        model.setSourceContourGroupNode(contourNode);
    }
    model.setPreservedArrays(preserved);

    bool stampGeometryAssetId = false;
    if (hasTri) {
        // Lazy opt-in (M9b-E): when the caller asked for lazyGeometry and the
        // asset actually carries the surface geometry blobs, skip the whole-blob
        // store.get materialization and instead stamp the payload with the
        // asset's id; the services layer resolves it on demand. Otherwise (eager
        // default, or blobs missing) materialize the resident handle as before.
        const bool blobsPresent = find_blob(asset, "points") != 0
            && find_blob(asset, "tris") != 0 && find_blob(asset, "faceId") != 0;
        if (lazyGeometry && blobsPresent) {
            stampGeometryAssetId = true; // geometry handle left empty
        } else {
            std::shared_ptr<XQTriangleSurfaceGeometryHandle> handle;
            if (!rebuild_triangle_geometry(store, asset, "points", "tris", "faceId",
                                           &handle, blobStatus)) {
                return PayloadRebuild::BlobError;
            }
            model.setTriangleGeometry(handle);
        }
    }

    const std::vector<std::string>* facesHead = 0;
    if (!find_payload_line(asset, "faces", &facesHead) || facesHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t faceCount = 0;
    if (!parse_size((*facesHead)[1], &faceCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t facesSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "face") {
            continue;
        }
        std::size_t fc = 1;
        ModelFace face = {};
        if (!tok_int(*it, &fc, &face.faceId)
            || !parse_face_kind_cap_name(*it, &fc, &face.kind, &face.capId, &face.name)) {
            return PayloadRebuild::TextError;
        }
        std::size_t loopCount = 0;
        if (!tok_size(*it, &fc, &loopCount)) {
            return PayloadRebuild::TextError;
        }
        for (std::size_t l = 0; l < loopCount; ++l) {
            int loopId = 0;
            if (!tok_int(*it, &fc, &loopId)) {
                return PayloadRebuild::TextError;
            }
            face.boundaryLoopIds.push_back(loopId);
        }
        if (fc != it->size()) {
            return PayloadRebuild::TextError;
        }
        model.addFace(face);
        ++facesSeen;
    }
    if (facesSeen != faceCount) {
        return PayloadRebuild::TextError;
    }

    auto surfacePayload = std::make_shared<XQSurfaceModelPayload>(std::move(model));
    if (stampGeometryAssetId) {
        surfacePayload->setGeometryAssetId(asset.id);
    }
    *out = surfacePayload;
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_mesh(BlobStore& store, const ParsedAsset& asset,
                            std::shared_ptr<XQPayload>* out, BlobStore::Status* blobStatus,
                            bool lazyGeometry)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "meshId", &head) || head->size() < 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t c = 1;
    NodeId meshId;
    if (c >= head->size() || !NodeId::deserialize((*head)[c], &meshId)) {
        return PayloadRebuild::TextError;
    }
    ++c;
    bool hasSrcModel = false;
    NodeId srcModel;
    if (!tok_keyword(*head, &c, "sourceModel")
        || !tok_optional_node(*head, &c, &hasSrcModel, &srcModel) || c != head->size()) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* preservedLine = 0;
    if (!find_payload_line(asset, "meshPreserved", &preservedLine) || preservedLine->size() != 5) {
        return PayloadRebuild::TextError;
    }
    PreservedMeshArrays preserved = {};
    std::size_t pc = 1;
    if (!tok_flag(*preservedLine, &pc, &preserved.hasGlobalNodeID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasGlobalElementID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasModelFaceID)
        || !tok_flag(*preservedLine, &pc, &preserved.hasCapID)) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* qualityLine = 0;
    if (!find_payload_line(asset, "quality", &qualityLine) || qualityLine->size() != 5) {
        return PayloadRebuild::TextError;
    }
    MeshQualitySummary quality = {};
    std::size_t qc = 1;
    std::size_t elementCount = 0;
    if (!tok_double(*qualityLine, &qc, &quality.minQuality)
        || !tok_double(*qualityLine, &qc, &quality.maxQuality)
        || !tok_double(*qualityLine, &qc, &quality.meanQuality)
        || !tok_size(*qualityLine, &qc, &elementCount)) {
        return PayloadRebuild::TextError;
    }
    quality.elementCount = elementCount;

    const std::vector<std::string>* surfTriLine = 0;
    const std::vector<std::string>* volTetLine = 0;
    if (!find_payload_line(asset, "hasSurfTri", &surfTriLine) || surfTriLine->size() != 2
        || !find_payload_line(asset, "hasVolTet", &volTetLine) || volTetLine->size() != 2) {
        return PayloadRebuild::TextError;
    }
    bool hasSurfTri = false;
    bool hasVolTet = false;
    std::size_t stc = 1;
    std::size_t vtc = 1;
    if (!tok_flag(*surfTriLine, &stc, &hasSurfTri) || !tok_flag(*volTetLine, &vtc, &hasVolTet)) {
        return PayloadRebuild::TextError;
    }

    XQMesh mesh;
    mesh.setId(meshId);
    if (hasSrcModel) {
        mesh.setSourceModelNode(srcModel);
    }
    mesh.setPreservedArrays(preserved);
    mesh.setQuality(quality);

    // Lazy opt-in (M9b-E): one geometryAssetId covers both the surface and the
    // volume geometry blobs of this mesh asset. When lazyGeometry is on and the
    // present geometry blobs exist, skip the whole-blob store.get materialization
    // and stamp the payload; services resolves on demand.
    bool stampGeometryAssetId = false;

    if (hasSurfTri) {
        const bool surfBlobsPresent = find_blob(asset, "surfPoints") != 0
            && find_blob(asset, "surfTris") != 0 && find_blob(asset, "surfFaceId") != 0;
        if (lazyGeometry && surfBlobsPresent) {
            stampGeometryAssetId = true; // surface handle left empty
        } else {
            std::shared_ptr<XQTriangleSurfaceGeometryHandle> surfHandle;
            if (!rebuild_triangle_geometry(store, asset, "surfPoints", "surfTris", "surfFaceId",
                                           &surfHandle, blobStatus)) {
                return PayloadRebuild::BlobError;
            }
            mesh.setSurfaceTriangles(surfHandle);
        }
    }
    if (hasVolTet) {
        const ParsedBlob* volPointsBlob = find_blob(asset, "volPoints");
        const ParsedBlob* tetsBlob = find_blob(asset, "tets");
        if (volPointsBlob == 0 || tetsBlob == 0) {
            *blobStatus = BlobStore::Status::MissingBlob;
            return PayloadRebuild::BlobError;
        }
        if (lazyGeometry) {
            stampGeometryAssetId = true; // volume handle left empty
        } else {
            std::vector<double> points;
            std::vector<int> tets;
            const BlobStore::Status sp = store.get(volPointsBlob->ref, &points);
            if (sp != BlobStore::Status::Ok) {
                *blobStatus = sp;
                return PayloadRebuild::BlobError;
            }
            const BlobStore::Status stt = store.get(tetsBlob->ref, &tets);
            if (stt != BlobStore::Status::Ok) {
                *blobStatus = stt;
                return PayloadRebuild::BlobError;
            }
            auto volHandle = std::make_shared<XQTetVolumeMeshHandle>();
            const std::size_t pointCount = points.size() / 3;
            for (std::size_t i = 0; i < pointCount; ++i) {
                Point3 p = {points[i * 3], points[i * 3 + 1], points[i * 3 + 2]};
                volHandle->addPoint(p);
            }
            const std::size_t tetCount = tets.size() / 4;
            for (std::size_t i = 0; i < tetCount; ++i) {
                volHandle->addTet(tets[i * 4], tets[i * 4 + 1], tets[i * 4 + 2], tets[i * 4 + 3]);
            }
            mesh.setVolumeTets(volHandle);
        }
    }

    const std::vector<std::string>* regionsHead = 0;
    if (!find_payload_line(asset, "regions", &regionsHead) || regionsHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t regionCount = 0;
    if (!parse_size((*regionsHead)[1], &regionCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t regionsSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "region") {
            continue;
        }
        std::size_t rc = 1;
        MeshRegion region = {};
        bool namePresent = false;
        if (!tok_int(*it, &rc, &region.regionId)
            || !tok_optional_string(*it, &rc, &namePresent, &region.name) || rc != it->size()) {
            return PayloadRebuild::TextError;
        }
        mesh.addRegion(region);
        ++regionsSeen;
    }
    if (regionsSeen != regionCount) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* meshFacesHead = 0;
    if (!find_payload_line(asset, "meshFaces", &meshFacesHead) || meshFacesHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t meshFaceCount = 0;
    if (!parse_size((*meshFacesHead)[1], &meshFaceCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t meshFacesSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "meshFace") {
            continue;
        }
        std::size_t fc = 1;
        MeshBoundaryFace face = {};
        if (!tok_int(*it, &fc, &face.faceId)
            || !parse_face_kind_cap_name(*it, &fc, &face.kind, &face.capId, &face.name)) {
            return PayloadRebuild::TextError;
        }
        std::size_t cellCount = 0;
        if (!tok_size(*it, &fc, &cellCount)) {
            return PayloadRebuild::TextError;
        }
        for (std::size_t k = 0; k < cellCount; ++k) {
            int cellId = 0;
            if (!tok_int(*it, &fc, &cellId)) {
                return PayloadRebuild::TextError;
            }
            face.cellIds.push_back(cellId);
        }
        std::size_t localCount = 0;
        if (!tok_size(*it, &fc, &localCount)) {
            return PayloadRebuild::TextError;
        }
        for (std::size_t k = 0; k < localCount; ++k) {
            int local = 0;
            if (!tok_int(*it, &fc, &local)) {
                return PayloadRebuild::TextError;
            }
            face.localFaces.push_back(local);
        }
        if (fc != it->size()) {
            return PayloadRebuild::TextError;
        }
        mesh.addBoundaryFace(face);
        ++meshFacesSeen;
    }
    if (meshFacesSeen != meshFaceCount) {
        return PayloadRebuild::TextError;
    }

    auto meshPayload = std::make_shared<XQMeshPayload>(std::move(mesh));
    if (stampGeometryAssetId) {
        meshPayload->setGeometryAssetId(asset.id);
    }
    *out = meshPayload;
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_sim_case(const ParsedAsset& asset, std::shared_ptr<XQPayload>* out)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "caseId", &head) || head->size() < 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t c = 1;
    NodeId caseId;
    if (c >= head->size() || !NodeId::deserialize((*head)[c], &caseId)) {
        return PayloadRebuild::TextError;
    }
    ++c;
    bool hasSrcMesh = false;
    NodeId srcMesh;
    if (!tok_keyword(*head, &c, "sourceMesh")
        || !tok_optional_node(*head, &c, &hasSrcMesh, &srcMesh) || c != head->size()) {
        return PayloadRebuild::TextError;
    }

    XQSimulationCase sim;
    sim.setId(caseId);
    if (hasSrcMesh) {
        sim.setSourceMeshNode(srcMesh);
    }

    const std::vector<std::string>* solverLine = 0;
    if (!find_payload_line(asset, "solver", &solverLine) || solverLine->size() != 3) {
        return PayloadRebuild::TextError;
    }
    SolverParameters solver = {};
    std::size_t sc = 1;
    if (!tok_int(*solverLine, &sc, &solver.timeSteps)
        || !tok_double(*solverLine, &sc, &solver.timeStepSize)) {
        return PayloadRebuild::TextError;
    }
    sim.setSolverParameters(solver);

    const std::vector<std::string>* fluidLine = 0;
    if (!find_payload_line(asset, "fluid", &fluidLine) || fluidLine->size() != 3) {
        return PayloadRebuild::TextError;
    }
    FluidProperties fluid = {};
    std::size_t fc = 1;
    if (!tok_double(*fluidLine, &fc, &fluid.density)
        || !tok_double(*fluidLine, &fc, &fluid.viscosity)) {
        return PayloadRebuild::TextError;
    }
    sim.setFluidProperties(fluid);

    const std::vector<std::string>* romLine = 0;
    if (!find_payload_line(asset, "rom", &romLine)) {
        return PayloadRebuild::TextError;
    }
    // rom centerline <id|-> period <d> numTimeSteps <n> dt <d> numCycles <n>
    //     inlet <k> <id...> outlet <k> <id...>
    RomSettings rom = {};
    std::size_t rc = 1;
    bool hasCenter = false;
    NodeId center;
    if (!tok_keyword(*romLine, &rc, "centerline")
        || !tok_optional_node(*romLine, &rc, &hasCenter, &center)) {
        return PayloadRebuild::TextError;
    }
    rom.centerlineNode = hasCenter ? center : NodeId::invalid();
    int numTimeSteps = 0;
    int numCycles = 0;
    if (!tok_keyword(*romLine, &rc, "period") || !tok_double(*romLine, &rc, &rom.period)
        || !tok_keyword(*romLine, &rc, "numTimeSteps") || !tok_int(*romLine, &rc, &numTimeSteps)
        || !tok_keyword(*romLine, &rc, "dt") || !tok_double(*romLine, &rc, &rom.dt)
        || !tok_keyword(*romLine, &rc, "numCycles") || !tok_int(*romLine, &rc, &numCycles)) {
        return PayloadRebuild::TextError;
    }
    rom.numTimeSteps = numTimeSteps;
    rom.numCycles = numCycles;
    std::size_t inletCount = 0;
    if (!tok_keyword(*romLine, &rc, "inlet") || !tok_size(*romLine, &rc, &inletCount)) {
        return PayloadRebuild::TextError;
    }
    for (std::size_t i = 0; i < inletCount; ++i) {
        int id = 0;
        if (!tok_int(*romLine, &rc, &id)) {
            return PayloadRebuild::TextError;
        }
        rom.inletFaceIds.push_back(id);
    }
    std::size_t outletCount = 0;
    if (!tok_keyword(*romLine, &rc, "outlet") || !tok_size(*romLine, &rc, &outletCount)) {
        return PayloadRebuild::TextError;
    }
    for (std::size_t i = 0; i < outletCount; ++i) {
        int id = 0;
        if (!tok_int(*romLine, &rc, &id)) {
            return PayloadRebuild::TextError;
        }
        rom.outletFaceIds.push_back(id);
    }
    if (rc != romLine->size()) {
        return PayloadRebuild::TextError;
    }
    sim.setRomSettings(rom);

    const std::vector<std::string>* bcsHead = 0;
    if (!find_payload_line(asset, "bcs", &bcsHead) || bcsHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t bcCount = 0;
    if (!parse_size((*bcsHead)[1], &bcCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t bcSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "bc") {
            continue;
        }
        // bc <faceId> <typeInt> <value> rcr <k> <d...> waveform <m> <t q ...>
        //    waveformPeriod <d>
        std::size_t bc = 1;
        BoundaryCondition cond = {};
        int typeInt = 0;
        if (!tok_int(*it, &bc, &cond.faceId) || !tok_int(*it, &bc, &typeInt)
            || typeInt < 0 || typeInt > static_cast<int>(BoundaryConditionType::InletFlowWaveform)
            || !tok_double(*it, &bc, &cond.value)) {
            return PayloadRebuild::TextError;
        }
        cond.type = static_cast<BoundaryConditionType>(typeInt);
        std::size_t rcrCount = 0;
        if (!tok_keyword(*it, &bc, "rcr") || !tok_size(*it, &bc, &rcrCount)) {
            return PayloadRebuild::TextError;
        }
        for (std::size_t k = 0; k < rcrCount; ++k) {
            double v = 0.0;
            if (!tok_double(*it, &bc, &v)) {
                return PayloadRebuild::TextError;
            }
            cond.rcr.push_back(v);
        }
        std::size_t waveCount = 0;
        if (!tok_keyword(*it, &bc, "waveform") || !tok_size(*it, &bc, &waveCount)) {
            return PayloadRebuild::TextError;
        }
        for (std::size_t k = 0; k < waveCount; ++k) {
            double t = 0.0;
            double q = 0.0;
            if (!tok_double(*it, &bc, &t) || !tok_double(*it, &bc, &q)) {
                return PayloadRebuild::TextError;
            }
            cond.flowWaveform.push_back(std::make_pair(t, q));
        }
        if (!tok_keyword(*it, &bc, "waveformPeriod") || !tok_double(*it, &bc, &cond.waveformPeriod)
            || bc != it->size()) {
            return PayloadRebuild::TextError;
        }
        sim.addBoundaryCondition(cond);
        ++bcSeen;
    }
    if (bcSeen != bcCount) {
        return PayloadRebuild::TextError;
    }

    *out = std::make_shared<XQSimulationCasePayload>(std::move(sim));
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_flow_result(const ParsedAsset& asset, std::shared_ptr<XQPayload>* out)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "flowSourceCase", &head)) {
        return PayloadRebuild::TextError;
    }
    // flowSourceCase <id|-> converged <0|1> maxCfl <d>
    std::size_t c = 1;
    bool hasSrcCase = false;
    NodeId srcCase;
    bool converged = false;
    double maxCfl = 0.0;
    if (!tok_optional_node(*head, &c, &hasSrcCase, &srcCase)
        || !tok_keyword(*head, &c, "converged") || !tok_flag(*head, &c, &converged)
        || !tok_keyword(*head, &c, "maxCfl") || !tok_double(*head, &c, &maxCfl)
        || c != head->size()) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* timesLine = 0;
    if (!find_payload_line(asset, "times", &timesLine) || timesLine->size() < 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t tc = 1;
    std::size_t timeCount = 0;
    if (!tok_size(*timesLine, &tc, &timeCount)) {
        return PayloadRebuild::TextError;
    }
    std::vector<double> times;
    times.reserve(timeCount);
    for (std::size_t i = 0; i < timeCount; ++i) {
        double v = 0.0;
        if (!tok_double(*timesLine, &tc, &v)) {
            return PayloadRebuild::TextError;
        }
        times.push_back(v);
    }
    if (tc != timesLine->size()) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* segHead = 0;
    if (!find_payload_line(asset, "segments", &segHead) || segHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t segCount = 0;
    if (!parse_size((*segHead)[1], &segCount)) {
        return PayloadRebuild::TextError;
    }
    std::vector<FlowSegment> segments;
    segments.reserve(segCount);
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "seg") {
            continue;
        }
        std::size_t sc = 1;
        FlowSegment seg = {};
        if (!tok_int(*it, &sc, &seg.segmentId)
            || !tok_double(*it, &sc, &seg.arcLengthStart)
            || !tok_double(*it, &sc, &seg.arcLengthEnd)
            || !tok_int(*it, &sc, &seg.faceId) || sc != it->size()) {
            return PayloadRebuild::TextError;
        }
        segments.push_back(seg);
    }
    if (segments.size() != segCount) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* seriesHead = 0;
    if (!find_payload_line(asset, "series", &seriesHead) || seriesHead->size() != 3) {
        return PayloadRebuild::TextError;
    }
    std::size_t rows = 0;
    std::size_t cols = 0;
    if (!parse_size((*seriesHead)[1], &rows) || !parse_size((*seriesHead)[2], &cols)) {
        return PayloadRebuild::TextError;
    }
    // q rows, then p rows, then a rows -- in order, each tagged.
    std::vector<std::vector<double>> q;
    std::vector<std::vector<double>> p;
    std::vector<std::vector<double>> a;
    std::vector<std::vector<double>>* mats[3] = {&q, &p, &a};
    const char* tags[3] = {"q", "p", "a"};
    int matIndex = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty()) {
            continue;
        }
        const std::string& h = (*it)[0];
        if (h != "q" && h != "p" && h != "a") {
            continue;
        }
        // Rows arrive grouped q* then p* then a*; advance matIndex when the tag
        // changes. Validate the tag matches the expected matrix.
        while (matIndex < 3 && tags[matIndex] != h) {
            ++matIndex;
        }
        if (matIndex >= 3) {
            return PayloadRebuild::TextError;
        }
        std::vector<double> row;
        row.reserve(cols);
        for (std::size_t k = 0; k < cols; ++k) {
            double v = 0.0;
            if (1 + k >= it->size() || !parse_double((*it)[1 + k], &v)) {
                return PayloadRebuild::TextError;
            }
            row.push_back(v);
        }
        if (it->size() != 1 + cols) {
            return PayloadRebuild::TextError;
        }
        mats[matIndex]->push_back(row);
    }
    if (q.size() != rows || p.size() != rows || a.size() != rows) {
        return PayloadRebuild::TextError;
    }

    XQFlowResult flow;
    if (hasSrcCase) {
        flow.setSourceCaseNode(srcCase);
    }
    flow.setTimes(times);
    for (std::vector<FlowSegment>::const_iterator it = segments.begin(); it != segments.end(); ++it) {
        flow.addSegment(*it);
    }
    flow.setSeries(q, p, a);
    flow.setConverged(converged);
    flow.setMaxCfl(maxCfl);
    *out = std::make_shared<XQFlowResultPayload>(std::move(flow));
    return PayloadRebuild::Ok;
}

PayloadRebuild rebuild_ai_analysis(const ParsedAsset& asset, std::shared_ptr<XQPayload>* out)
{
    const std::vector<std::string>* head = 0;
    if (!find_payload_line(asset, "aiKind", &head)) {
        return PayloadRebuild::TextError;
    }
    // aiKind <int> provenance <int> modelId <opt> source <id|-> diagnostic <opt>
    std::size_t c = 1;
    int kindInt = 0;
    int provInt = 0;
    if (!tok_int(*head, &c, &kindInt)
        || kindInt < 0 || kindInt > static_cast<int>(AnalysisKind::SurrogatePrediction)
        || !tok_keyword(*head, &c, "provenance") || !tok_int(*head, &c, &provInt)
        || provInt < 0 || provInt > static_cast<int>(AnalysisProvenance::SurrogatePredicted)) {
        return PayloadRebuild::TextError;
    }
    bool modelPresent = false;
    std::string modelId;
    bool hasSource = false;
    NodeId source;
    bool diagPresent = false;
    std::string diagnostic;
    if (!tok_keyword(*head, &c, "modelId")
        || !tok_optional_string(*head, &c, &modelPresent, &modelId)
        || !tok_keyword(*head, &c, "source") || !tok_optional_node(*head, &c, &hasSource, &source)
        || !tok_keyword(*head, &c, "diagnostic")
        || !tok_optional_string(*head, &c, &diagPresent, &diagnostic) || c != head->size()) {
        return PayloadRebuild::TextError;
    }

    XQAiAnalysis ai;
    ai.setKind(static_cast<AnalysisKind>(kindInt));
    ai.setProvenance(static_cast<AnalysisProvenance>(provInt));
    ai.setModelId(modelId);
    if (hasSource) {
        ai.setSourceNode(source);
    }
    ai.setDiagnostic(diagnostic);

    const std::vector<std::string>* metricsHead = 0;
    if (!find_payload_line(asset, "metrics", &metricsHead) || metricsHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t metricCount = 0;
    if (!parse_size((*metricsHead)[1], &metricCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t metricsSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "metric") {
            continue;
        }
        std::size_t mc = 1;
        NamedMetric metric = {};
        bool namePresent = false;
        bool unitPresent = false;
        if (!tok_optional_string(*it, &mc, &namePresent, &metric.name)
            || !tok_double(*it, &mc, &metric.value)
            || !tok_optional_string(*it, &mc, &unitPresent, &metric.unit) || mc != it->size()) {
            return PayloadRebuild::TextError;
        }
        ai.addMetric(metric);
        ++metricsSeen;
    }
    if (metricsSeen != metricCount) {
        return PayloadRebuild::TextError;
    }

    const std::vector<std::string>* annHead = 0;
    if (!find_payload_line(asset, "annotations", &annHead) || annHead->size() != 2) {
        return PayloadRebuild::TextError;
    }
    std::size_t annCount = 0;
    if (!parse_size((*annHead)[1], &annCount)) {
        return PayloadRebuild::TextError;
    }
    std::size_t annSeen = 0;
    for (std::vector<std::vector<std::string>>::const_iterator it = asset.payloadLines.begin();
         it != asset.payloadLines.end();
         ++it) {
        if (it->empty() || (*it)[0] != "annotation") {
            continue;
        }
        std::size_t ac = 1;
        Annotation ann = {};
        bool labelPresent = false;
        if (!tok_optional_string(*it, &ac, &labelPresent, &ann.label)
            || !tok_double(*it, &ac, &ann.arcLength)
            || !tok_int(*it, &ac, &ann.faceId)
            || !tok_double(*it, &ac, &ann.score) || ac != it->size()) {
            return PayloadRebuild::TextError;
        }
        ai.addAnnotation(ann);
        ++annSeen;
    }
    if (annSeen != annCount) {
        return PayloadRebuild::TextError;
    }

    *out = std::make_shared<XQAiAnalysisPayload>(std::move(ai));
    return PayloadRebuild::Ok;
}

// Dispatches payload rebuild by the block's kind token. *blobStatus is set only
// when the return is BlobError.
PayloadRebuild rebuild_payload(BlobStore& store, const ParsedAsset& asset,
                               std::shared_ptr<XQPayload>* out, BlobStore::Status* blobStatus,
                               bool lazyGeometry)
{
    if (asset.payloadKind == "source") {
        return rebuild_source(asset, out);
    }
    if (asset.payloadKind == "path") {
        return rebuild_path(asset, out);
    }
    if (asset.payloadKind == "segMask") {
        return rebuild_seg_mask(store, asset, out, blobStatus);
    }
    if (asset.payloadKind == "surface") {
        return rebuild_surface(store, asset, out, blobStatus, lazyGeometry);
    }
    if (asset.payloadKind == "mesh") {
        return rebuild_mesh(store, asset, out, blobStatus, lazyGeometry);
    }
    if (asset.payloadKind == "simCase") {
        return rebuild_sim_case(asset, out);
    }
    if (asset.payloadKind == "flowResult") {
        return rebuild_flow_result(asset, out);
    }
    if (asset.payloadKind == "aiAnalysis") {
        return rebuild_ai_analysis(asset, out);
    }
    return PayloadRebuild::TextError;
}

// First pass: parse the whole assets section into memory without validating
// cross-references (those are checked in the second pass).
bool parse_assets_section(const std::vector<std::string>& lines,
                          std::size_t* index,
                          std::vector<ParsedAsset>* assets,
                          std::vector<ParsedNodeAsset>* node_assets,
                          std::vector<ParsedAssetRelation>* asset_relations)
{
    std::size_t asset_count = 0;
    if (!expect_count_line(lines, index, "assets", &asset_count)) {
        return false;
    }
    assets->reserve(asset_count);
    for (std::size_t i = 0; i < asset_count; ++i) {
        ParsedAsset asset = {};
        if (!parse_asset(lines, index, &asset) || !validate_asset_blob_schemas(asset)) {
            return false;
        }
        assets->push_back(asset);
    }

    std::size_t node_asset_count = 0;
    if (!expect_count_line(lines, index, "nodeAssets", &node_asset_count)) {
        return false;
    }
    std::vector<std::string> tokens;
    node_assets->reserve(node_asset_count);
    for (std::size_t i = 0; i < node_asset_count; ++i) {
        if (!read_line_tokens(lines, index, &tokens)
            || tokens.size() != 3
            || tokens[0] != "nodeAsset") {
            return false;
        }
        ParsedNodeAsset binding = {};
        if (!NodeId::deserialize(tokens[1], &binding.node)
            || !binding.node.is_valid()
            || !AssetId::deserialize(tokens[2], &binding.asset)
            || !binding.asset.is_valid()) {
            return false;
        }
        node_assets->push_back(binding);
    }

    std::size_t relation_count = 0;
    if (!expect_count_line(lines, index, "relations", &relation_count)) {
        return false;
    }
    asset_relations->reserve(relation_count);
    for (std::size_t i = 0; i < relation_count; ++i) {
        if (!read_line_tokens(lines, index, &tokens)
            || tokens.size() != 3
            || tokens[0] != "assetRel") {
            return false;
        }
        ParsedAssetRelation relation = {};
        if (!AssetId::deserialize(tokens[1], &relation.source)
            || !relation.source.is_valid()
            || !AssetId::deserialize(tokens[2], &relation.derived)
            || !relation.derived.is_valid()) {
            return false;
        }
        asset_relations->push_back(relation);
    }

    if (!expect_single_token_line(lines, index, "endAssets")) {
        return false;
    }
    return true;
}

bool parse_project(const std::vector<std::string>& lines,
                   const std::string& projectFilePath,
                   XQProject* project,
                   std::vector<Diagnostic>* diagnostics,
                   XQProjectReader::Status* status,
                   bool lazyGeometry)
{
    if (project == 0 || status == 0) {
        return false;
    }

    std::size_t index = 0;
    std::vector<std::string> tokens;
    if (!read_line_tokens(lines, &index, &tokens)
        || tokens.size() != 3
        || tokens[0] != kMagic
        || tokens[1] != "schemaVersion") {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    SchemaVersion schema_version = {};
    if (!parse_schema_version(tokens[2], &schema_version)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }
    if (schema_version.major > kSupportedMajorVersion) {
        add_diagnostic(diagnostics,
                       DiagnosticSeverity::Error,
                       "PROJECT_UNSUPPORTED_SCHEMA_VERSION",
                       "project schema major version is not supported");
        *status = XQProjectReader::Status::UnsupportedVersion;
        return false;
    }

    std::string writer_version;
    std::string minimum_reader_version;
    std::string created_with;
    std::string project_id;
    if (!expect_field_line(lines, &index, "writerVersion", &writer_version)
        || !expect_field_line(lines, &index, "minimumReaderVersion", &minimum_reader_version)
        || !expect_field_line(lines, &index, "createdWith", &created_with)
        || !expect_field_line(lines, &index, "projectId", &project_id)
        || !expect_single_token_line(lines, &index, "scene")) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    // D2: honor the document's declared minimumReaderVersion. A document that
    // demands a newer reader than this build is refused rather than read
    // partially (a 1.2 doc with a mandatory assets section is refused by a
    // < 1.2 reader).
    SchemaVersion minimum_reader = {};
    if (!parse_schema_version(minimum_reader_version, &minimum_reader)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }
    if (minimum_reader.major > kReaderMajor
        || (minimum_reader.major == kReaderMajor && minimum_reader.minor > kReaderMinor)) {
        add_diagnostic(diagnostics,
                       DiagnosticSeverity::Error,
                       "PROJECT_UNSUPPORTED_SCHEMA_VERSION",
                       "project requires a newer reader than this build supports");
        *status = XQProjectReader::Status::UnsupportedVersion;
        return false;
    }

    std::size_t node_count = 0;
    if (!expect_count_line(lines, &index, "nodes", &node_count)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    std::vector<ParsedNode> nodes;
    nodes.reserve(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        if (!read_line_tokens(lines, &index, &tokens)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        ParsedNode node = {};
        if (!parse_node_line(tokens, &node)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        nodes.push_back(node);
    }

    std::size_t relation_count = 0;
    if (!expect_count_line(lines, &index, "relations", &relation_count)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    std::vector<ParsedRelation> relations;
    relations.reserve(relation_count);
    for (std::size_t i = 0; i < relation_count; ++i) {
        if (!read_line_tokens(lines, &index, &tokens)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        ParsedRelation relation = {};
        if (!parse_relation_line(tokens, &relation)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        relations.push_back(relation);
    }

    std::size_t stale_count = 0;
    if (!expect_count_line(lines, &index, "stale", &stale_count)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    std::vector<ParsedStaleNode> stale_nodes;
    stale_nodes.reserve(stale_count);
    for (std::size_t i = 0; i < stale_count; ++i) {
        if (!read_line_tokens(lines, &index, &tokens)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        ParsedStaleNode stale_node = {};
        if (!parse_stale_node_line(tokens, &stale_node)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        stale_nodes.push_back(stale_node);
    }

    std::size_t provenance_count = 0;
    std::size_t diagnostic_count = 0;
    if (!expect_single_token_line(lines, &index, "endScene")) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    // schema 1.2: an 'assets' section follows endScene. Parse it fully into
    // memory first (pass one); cross-references are validated and rebuilt in
    // pass two after the scene nodes exist. A document with no assets section is
    // an older one and loads with an Info diagnostic (back-compat, prd §7).
    std::vector<ParsedAsset> parsed_assets;
    std::vector<ParsedNodeAsset> parsed_node_assets;
    std::vector<ParsedAssetRelation> parsed_asset_relations;
    const bool has_assets_section = line_starts_with_token(lines, index, "assets");
    if (has_assets_section) {
        if (!parse_assets_section(
                lines, &index, &parsed_assets, &parsed_node_assets, &parsed_asset_relations)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    }

    const bool has_provenance_section = line_is_single_token(lines, index, "provenance");
    if (has_provenance_section) {
        if (!parse_provenance_section(lines, &index, &tokens, &provenance_count)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    } else if (!is_legacy_v10(schema_version)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    if (!expect_count_line(lines, &index, "diagnostics", &diagnostic_count)) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }
    if (diagnostic_count != 0) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }
    if (!expect_single_token_line(lines, &index, "end") || index != lines.size()) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    XQProject parsed;
    std::set<NodeId> node_ids;
    for (std::vector<ParsedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (!node_ids.insert(it->id).second) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        if (parsed.scene().insert(XQDataNode(it->id, it->domain_type, it->display_name))
            != XQScene::InsertResult::Inserted) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    }

    std::set<NodeId> stale_node_ids;
    for (std::vector<ParsedStaleNode>::const_iterator it = stale_nodes.begin();
         it != stale_nodes.end();
         ++it) {
        if (it->reason == XQScene::StaleReason::None) {
            continue;
        }
        if (parsed.scene().find(it->id) == 0 || !stale_node_ids.insert(it->id).second) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    }

    std::vector<ParsedRelation> delayed_relations;
    std::set<NodeId> stale_sources;
    std::set<std::pair<NodeId, NodeId>> seen_relations;
    for (std::vector<ParsedRelation>::const_iterator it = relations.begin();
         it != relations.end();
         ++it) {
        if (seen_relations.find(std::make_pair(it->source, it->derived)) != seen_relations.end()) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        seen_relations.insert(std::make_pair(it->source, it->derived));

        if (stale_node_ids.find(it->derived) != stale_node_ids.end()) {
            if (parsed.scene().link_derived(it->source, it->derived)
                != XQScene::RelationResult::Linked) {
                *status = XQProjectReader::Status::ParseError;
                return false;
            }
            stale_sources.insert(it->source);
        } else {
            delayed_relations.push_back(*it);
        }
    }

    for (std::set<NodeId>::const_iterator it = stale_sources.begin(); it != stale_sources.end(); ++it) {
        parsed.scene().mark_source_changed(*it);
    }

    for (std::vector<ParsedRelation>::const_iterator it = delayed_relations.begin();
         it != delayed_relations.end();
         ++it) {
        if (parsed.scene().link_derived(it->source, it->derived)
            != XQScene::RelationResult::Linked) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    }

    for (std::set<NodeId>::const_iterator it = stale_node_ids.begin();
         it != stale_node_ids.end();
         ++it) {
        if (!parsed.scene().is_stale(*it)) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
    }

    // Pass two: rebuild the asset registry and validate every cross-reference
    // now that the scene nodes exist. Any structural problem (duplicate asset
    // id, binding/relation pointing at a missing id, a node bound twice) fails
    // the whole load with a structured error (§6 ASSET_REFERENCE_INVALID).
    AssetRegistry& registry = parsed.assetRegistry();
    for (std::vector<ParsedAsset>::const_iterator it = parsed_assets.begin();
         it != parsed_assets.end();
         ++it) {
        if (!registry.registerAsset(it->id, it->category, it->kind)) {
            add_diagnostic(diagnostics,
                           DiagnosticSeverity::Error,
                           "ASSET_REFERENCE_INVALID",
                           "duplicate or invalid asset id in archive");
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        AssetRecord* record = registry.find(it->id);
        if (record == 0) {
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        record->displayName = it->hasName ? it->displayName : std::string();
        record->sourceAbsPath = it->hasAbsPath ? it->absPath : std::string();
        record->sourceRelPath = it->hasRelPath ? it->relPath : std::string();
        record->hasDicom = it->hasDicom;
        record->dicom = it->dicom;
        record->contentFingerprint = it->hasFingerprint ? it->fingerprint : std::string();
        record->hasGeometry = it->hasGeometry;
        record->geometry = it->geometry;
        for (std::vector<ParsedBlob>::const_iterator b = it->blobs.begin();
             b != it->blobs.end();
             ++b) {
            record->blobs.push_back(std::make_pair(b->role, b->ref));
        }
    }

    std::set<NodeId> bound_nodes;
    for (std::vector<ParsedNodeAsset>::const_iterator it = parsed_node_assets.begin();
         it != parsed_node_assets.end();
         ++it) {
        XQDataNode* node = parsed.scene().find(it->node);
        if (node == 0
            || registry.find(it->asset) == 0
            || !bound_nodes.insert(it->node).second) {
            add_diagnostic(diagnostics,
                           DiagnosticSeverity::Error,
                           "ASSET_REFERENCE_INVALID",
                           "node->asset binding references a missing id or binds a node twice");
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        node->setAssetId(it->asset);
    }

    for (std::vector<ParsedAssetRelation>::const_iterator it = parsed_asset_relations.begin();
         it != parsed_asset_relations.end();
         ++it) {
        if (registry.find(it->source) == 0 || registry.find(it->derived) == 0) {
            add_diagnostic(diagnostics,
                           DiagnosticSeverity::Error,
                           "ASSET_REFERENCE_INVALID",
                           "asset relation references a missing asset id");
            *status = XQProjectReader::Status::ParseError;
            return false;
        }
        registry.addRelation(it->source, it->derived);
    }

    // Rebuild entity payloads (S4): for each asset that carries a payload block,
    // fetch+verify its blobs and reconstruct the concrete payload, then attach
    // it to the node bound to that asset. A blob fetch/verify failure fails the
    // whole load with a structured ASSET_BLOB_* error (D8: no partial project).
    if (has_assets_section && !parsed_assets.empty()) {
        std::map<AssetId, NodeId> node_by_asset;
        for (std::vector<ParsedNodeAsset>::const_iterator it = parsed_node_assets.begin();
             it != parsed_node_assets.end();
             ++it) {
            node_by_asset[it->asset] = it->node;
        }

        std::filesystem::path projectPath(projectFilePath);
        const std::string stem = projectPath.stem().string();
        std::filesystem::path assetsDir = projectPath.parent_path() / (stem + ".assets");
        BlobStore store(assetsDir.string());

        for (std::vector<ParsedAsset>::const_iterator it = parsed_assets.begin();
             it != parsed_assets.end();
             ++it) {
            if (!it->hasPayload) {
                continue;
            }
            std::shared_ptr<XQPayload> payload;
            BlobStore::Status blobStatus = BlobStore::Status::Ok;
            const PayloadRebuild result = rebuild_payload(store, *it, &payload, &blobStatus,
                                                          lazyGeometry);
            if (result == PayloadRebuild::BlobError) {
                add_diagnostic(diagnostics,
                               DiagnosticSeverity::Error,
                               BlobErrorCode::of(blobStatus),
                               "asset blob could not be loaded or verified");
                *status = XQProjectReader::Status::ParseError;
                return false;
            }
            if (result != PayloadRebuild::Ok || !payload) {
                add_diagnostic(diagnostics,
                               DiagnosticSeverity::Error,
                               "ASSET_PAYLOAD_INVALID",
                               "asset payload block could not be parsed");
                *status = XQProjectReader::Status::ParseError;
                return false;
            }
            std::map<AssetId, NodeId>::const_iterator bound = node_by_asset.find(it->id);
            if (bound == node_by_asset.end()) {
                continue; // asset with a payload but no bound node: nothing to attach.
            }
            XQDataNode* node = parsed.scene().find(bound->second);
            if (node == 0) {
                *status = XQProjectReader::Status::ParseError;
                return false;
            }
            node->setPayload(payload->domainType(), payload);
        }
    }

    if (parsed.open() != XQProject::LifecycleResult::Ok) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    // Older archives have no assets section: load succeeds, registry is empty,
    // and we emit an Info diagnostic so callers know no asset data was present
    // (prd §7; new 1.2 docs always carry an assets section, even if empty).
    if (!has_assets_section) {
        add_diagnostic(diagnostics,
                       DiagnosticSeverity::Info,
                       "PROJECT_NO_ASSET_DATA",
                       "project predates asset data; loaded with an empty asset registry");
    }

    *project = parsed;
    *status = XQProjectReader::Status::Ok;
    return true;
}

} // namespace

XQProjectReader::Status XQProjectReader::load(const std::string& projectFilePath,
                                              XQProjectReadResult* out)
{
    return load(projectFilePath, out, XQProjectReadOptions{});
}

XQProjectReader::Status XQProjectReader::load(const std::string& projectFilePath,
                                              XQProjectReadResult* out,
                                              const XQProjectReadOptions& options)
{
    if (out == 0) {
        return Status::ParseError;
    }

    XQProjectReadResult result = {};
    std::ifstream input(projectFilePath.c_str());
    if (!input.good()) {
        add_diagnostic(&result.diagnostics,
                       DiagnosticSeverity::Error,
                       "PROJECT_FILE_NOT_FOUND",
                       "project file could not be opened");
        *out = result;
        return Status::FileNotFound;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        lines.push_back(line);
    }

    Status status = Status::ParseError;
    if (!parse_project(lines, projectFilePath, &result.project, &result.diagnostics, &status,
                       options.lazyGeometry)) {
        if (result.diagnostics.empty() && status == Status::ParseError) {
            add_diagnostic(&result.diagnostics,
                           DiagnosticSeverity::Error,
                           "PROJECT_PARSE_ERROR",
                           "project file did not match the native project format");
        }
        *out = result;
        return status;
    }

    *out = result;
    return Status::Ok;
}

} // namespace xq
