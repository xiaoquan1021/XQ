#include <core/Diagnostics.h>
#include <core/NodeId.h>

#include <iostream>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

} // namespace

int main()
{
    {
        xq::Diagnostic diagnostic(xq::DiagnosticSeverity::Warning,
                                  "PROJECT_CONFIG_WARNING",
                                  "project configuration requires review");

        if (diagnostic.severity() != xq::DiagnosticSeverity::Warning) {
            return fail("project-level diagnostic stores severity");
        }
        if (diagnostic.code() != "PROJECT_CONFIG_WARNING") {
            return fail("project-level diagnostic stores code");
        }
        if (diagnostic.message() != "project configuration requires review") {
            return fail("project-level diagnostic stores message");
        }
        if (diagnostic.has_node()) {
            return fail("project-level diagnostic has no node");
        }
        if (!diagnostic.is_project_level()) {
            return fail("project-level diagnostic reports project level");
        }
    }

    {
        const xq::NodeId node_id(701);
        xq::Diagnostic diagnostic(xq::DiagnosticSeverity::Error,
                                  "NODE_RELATION_STALE",
                                  "node relation is stale",
                                  node_id);

        if (!diagnostic.has_node()) {
            return fail("node-level diagnostic has node");
        }
        if (diagnostic.node().value() != node_id.value()) {
            return fail("node-level diagnostic stores node id");
        }
        if (diagnostic.is_project_level()) {
            return fail("node-level diagnostic is not project level");
        }
        if (diagnostic.severity() != xq::DiagnosticSeverity::Error) {
            return fail("node-level diagnostic stores severity");
        }
        if (diagnostic.code() != "NODE_RELATION_STALE") {
            return fail("node-level diagnostic stores code");
        }
        if (diagnostic.message() != "node relation is stale") {
            return fail("node-level diagnostic stores message");
        }
    }

    {
        xq::Diagnostic info(xq::DiagnosticSeverity::Info,
                            "INFO_CODE",
                            "informational diagnostic");
        xq::Diagnostic warning(xq::DiagnosticSeverity::Warning,
                               "WARNING_CODE",
                               "warning diagnostic");
        xq::Diagnostic error(xq::DiagnosticSeverity::Error,
                             "ERROR_CODE",
                             "error diagnostic");

        if (info.severity() != xq::DiagnosticSeverity::Info) {
            return fail("info severity is stored");
        }
        if (warning.severity() != xq::DiagnosticSeverity::Warning) {
            return fail("warning severity is stored");
        }
        if (error.severity() != xq::DiagnosticSeverity::Error) {
            return fail("error severity is stored");
        }
    }

    {
        xq::Provenance provenance("importer",
                                  "normalize",
                                  "v1");

        if (provenance.source() != "importer") {
            return fail("provenance stores source");
        }
        if (provenance.operation() != "normalize") {
            return fail("provenance stores operation");
        }
        if (provenance.version() != "v1") {
            return fail("provenance stores version");
        }
        if (provenance.stale_relation_evidence() != 0) {
            return fail("provenance stale evidence starts at zero");
        }

        provenance.set_stale_relation_evidence(3);
        if (provenance.stale_relation_evidence() != 3) {
            return fail("provenance stores stale evidence");
        }
    }

    return 0;
}
