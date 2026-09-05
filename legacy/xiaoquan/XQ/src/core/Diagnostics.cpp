#include "core/Diagnostics.h"

namespace xq {

Diagnostic::Diagnostic(DiagnosticSeverity severity,
                       const std::string& code,
                       const std::string& message)
    : severity_(severity)
    , code_(code)
    , message_(message)
    , has_node_(false)
    , node_(NodeId::invalid())
{
}

Diagnostic::Diagnostic(DiagnosticSeverity severity,
                       const std::string& code,
                       const std::string& message,
                       const NodeId& node)
    : severity_(severity)
    , code_(code)
    , message_(message)
    , has_node_(true)
    , node_(node)
{
}

DiagnosticSeverity Diagnostic::severity() const
{
    return severity_;
}

const std::string& Diagnostic::code() const
{
    return code_;
}

const std::string& Diagnostic::message() const
{
    return message_;
}

bool Diagnostic::has_node() const
{
    return has_node_;
}

const NodeId& Diagnostic::node() const
{
    return node_;
}

bool Diagnostic::is_project_level() const
{
    return !has_node_;
}

Provenance::Provenance(const std::string& source,
                       const std::string& operation,
                       const std::string& version)
    : source_(source)
    , operation_(operation)
    , version_(version)
    , stale_relation_evidence_(0)
{
}

const std::string& Provenance::source() const
{
    return source_;
}

const std::string& Provenance::operation() const
{
    return operation_;
}

const std::string& Provenance::version() const
{
    return version_;
}

std::size_t Provenance::stale_relation_evidence() const
{
    return stale_relation_evidence_;
}

void Provenance::set_stale_relation_evidence(std::size_t count)
{
    stale_relation_evidence_ = count;
}

} // namespace xq
