#pragma once

#include <xqModulePythonApiExports.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Descriptor for one DataStorage node, returned by list_nodes / find_node.
// Does NOT hold long-lived raw pointers to DataStorage or DataNode.
struct XQMODULEPYTHONAPI_EXPORT xq_NodeDescriptor
{
    std::string name;
    std::string type;   // class name of the node's data, e.g. "xq_Model"
    std::string stage;  // pipeline stage string, e.g. "Model"
};

// Result type for operations that can succeed or fail with a diagnostic.
struct XQMODULEPYTHONAPI_EXPORT xq_ApiResult
{
    bool ok = false;
    std::string diagnostic;

    // Node descriptors (for list/find/resolve operations).
    std::vector<xq_NodeDescriptor> nodes;

    // Single string value (for version / resolve_upstream).
    std::string value;
};

// C++ service contract for the XQ Python API layer.
//
// In Phase 1 this is a pure-C++ skeleton: no pybind11 or Python runtime
// is required.  The IsAvailable() method returns false, signalling that
// Python scripting is not enabled.  All other methods operate on the
// MITK DataStorage directly and are safe to call regardless of Python
// availability.
//
// When pybind11 and a matching Python ABI become available, a future
// phase can subclass or wrap this service to expose it to Python.
class XQMODULEPYTHONAPI_EXPORT xq_PythonApiService
{
public:
    xq_PythonApiService();
    explicit xq_PythonApiService(mitk::DataStorage* ds);
    ~xq_PythonApiService();

    // --- Availability ---

    // Returns true when a Python runtime (pybind11 + matching ABI) is
    // loaded and ready.  Currently always returns false.
    [[nodiscard]] bool IsAvailable() const;

    // Human-readable reason why IsAvailable() returns its current value.
    [[nodiscard]] std::string GetAvailabilityDiagnostic() const;

    // --- Core API ---

    // version() — returns build / version information.
    [[nodiscard]] xq_ApiResult Version() const;

    // list_nodes() — returns descriptors for all nodes directly under the
    // DataStorage root (category folders excluded).
    [[nodiscard]] xq_ApiResult ListNodes() const;

    // find_node(name) — returns the descriptor for the first node whose
    // name matches exactly, or !ok with diagnostic.
    [[nodiscard]] xq_ApiResult FindNode(std::string_view name) const;

    // resolve_upstream(node_name, stage) — follows xq.source.* properties
    // to find the name of the upstream node at the given pipeline stage.
    [[nodiscard]] xq_ApiResult ResolveUpstream(std::string_view nodeName,
                                                std::string_view stageName) const;

    // project.open(path) — skeleton; always returns !ok with
    // "unsupported" diagnostic until WorkspaceManager integration.
    [[nodiscard]] xq_ApiResult ProjectOpen(std::string_view path) const;

    // project.save(path) — skeleton; always returns !ok with
    // "unsupported" diagnostic until WorkspaceManager integration.
    [[nodiscard]] xq_ApiResult ProjectSave(std::string_view path) const;

    // --- DataStorage access (internal) ---
    void SetDataStorage(mitk::DataStorage* ds);

private:
    mitk::DataStorage* m_DataStorage = nullptr;
    bool m_Available = false;
    std::string m_AvailabilityDiag =
        "Python API unavailable in this build: the xq Python extension module "
        "was not built because pybind11 and a matching Python 3.11 ABI are not linked. "
        "The C++ inspection service remains available for tests and internal callers.";
};
