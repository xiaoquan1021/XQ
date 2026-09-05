#include "io/project/XQProjectReader.h"

#include <cstddef>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

const char* const kMagic = "XQ_NATIVE_PROJECT";
const unsigned int kSupportedMajorVersion = 1;

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

bool parse_project(const std::vector<std::string>& lines,
                   XQProject* project,
                   std::vector<Diagnostic>* diagnostics,
                   XQProjectReader::Status* status)
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

    std::string ignored;
    if (!expect_field_line(lines, &index, "writerVersion", &ignored)
        || !expect_field_line(lines, &index, "minimumReaderVersion", &ignored)
        || !expect_field_line(lines, &index, "createdWith", &ignored)
        || !expect_field_line(lines, &index, "projectId", &ignored)
        || !expect_single_token_line(lines, &index, "scene")) {
        *status = XQProjectReader::Status::ParseError;
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

    if (parsed.open() != XQProject::LifecycleResult::Ok) {
        *status = XQProjectReader::Status::ParseError;
        return false;
    }

    *project = parsed;
    *status = XQProjectReader::Status::Ok;
    return true;
}

} // namespace

XQProjectReader::Status XQProjectReader::load(const std::string& projectFilePath,
                                              XQProjectReadResult* out)
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
    if (!parse_project(lines, &result.project, &result.diagnostics, &status)) {
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
