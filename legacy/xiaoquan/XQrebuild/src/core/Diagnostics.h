#ifndef XQ_CORE_DIAGNOSTICS_H
#define XQ_CORE_DIAGNOSTICS_H

#include "core/NodeId.h"

#include <cstddef>
#include <string>

namespace xq {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

// Associated target: either a specific node or project-level with no node.
class Diagnostic {
public:
    Diagnostic(DiagnosticSeverity severity,
               const std::string& code,
               const std::string& message);
    Diagnostic(DiagnosticSeverity severity,
               const std::string& code,
               const std::string& message,
               const NodeId& node);

    DiagnosticSeverity severity() const;
    const std::string& code() const;
    const std::string& message() const;
    bool has_node() const;
    const NodeId& node() const;
    bool is_project_level() const;

private:
    DiagnosticSeverity severity_;
    std::string code_;
    std::string message_;
    bool has_node_;
    NodeId node_;
};

// Provenance record for technical data source, operation, version, and stale evidence.
class Provenance {
public:
    Provenance(const std::string& source,
               const std::string& operation,
               const std::string& version);

    const std::string& source() const;
    const std::string& operation() const;
    const std::string& version() const;

    std::size_t stale_relation_evidence() const;
    void set_stale_relation_evidence(std::size_t count);

private:
    std::string source_;
    std::string operation_;
    std::string version_;
    std::size_t stale_relation_evidence_;
};

} // namespace xq

#endif // XQ_CORE_DIAGNOSTICS_H
