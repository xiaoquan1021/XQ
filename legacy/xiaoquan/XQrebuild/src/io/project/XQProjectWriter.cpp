#include "io/project/XQProjectWriter.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace xq {
namespace {

const char* const kMagic = "XQ_NATIVE_PROJECT";
const char* const kSchemaVersion = "1.1";
const char* const kWriterVersion = "XQ-M8-001";
const char* const kMinimumReaderVersion = "1.0";
const char* const kCreatedWith = "XQrebuild";
const char* const kProjectId = "synthetic-l0-project";

struct NodeRecord {
    std::string id;
    std::string domain_type;
    std::string display_name;
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

} // namespace

XQProjectWriter::Status XQProjectWriter::save(const XQProject& project,
                                              const std::string& projectFilePath)
{
    std::vector<NodeRecord> nodes;
    project.scene().visit_nodes(
        [&nodes](const XQDataNode& node) {
            NodeRecord record = {};
            record.id = node.id().serialize();
            record.domain_type = encode_field(node.domain_type());
            record.display_name = encode_field(node.display_name());
            nodes.push_back(record);
        });

    std::vector<RelationRecord> relations;
    project.scene().visit_derived_relations(
        [&relations](const NodeId& source, const NodeId& derived) {
            RelationRecord record = {};
            record.source = source.serialize();
            record.derived = derived.serialize();
            relations.push_back(record);
        });

    std::vector<StaleRecord> stale_nodes;
    project.scene().visit_stale_nodes(
        [&stale_nodes](const NodeId& node, XQScene::StaleReason reason) {
            if (reason != XQScene::StaleReason::None) {
                StaleRecord record = {};
                record.id = node.serialize();
                record.reason = reason;
                stale_nodes.push_back(record);
            }
        });

    std::ofstream output(projectFilePath.c_str(), std::ios::out | std::ios::trunc);
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
        output << "node " << it->id << " " << it->domain_type << " " << it->display_name << "\n";
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
    output << "provenance\n";
    output << "records 1\n";
    output << "record L0-synthetic save " << kWriterVersion << " " << stale_nodes.size() << "\n";
    output << "endProvenance\n";
    output << "diagnostics 0\n";
    output << "end\n";

    if (!output.good()) {
        return Status::WriteError;
    }

    return Status::Ok;
}

} // namespace xq
