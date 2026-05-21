#pragma once

#include <xqModulePythonApiExports.h>

#include <mitkBaseData.h>
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

    // add_* — create real XQ BaseData nodes that use the same xq.pipeline.*
    // and xq.source.* metadata contracts as GUI-created nodes. Geometry-heavy
    // contents are not fabricated: empty model/mesh containers stay empty
    // until a real geometry/mesh payload is supplied by a future binding.
    [[nodiscard]] xq_ApiResult AddPath(std::string_view name,
                                       std::string_view sourceImage = "") const;
    [[nodiscard]] xq_ApiResult AddSegmentation(std::string_view name,
                                               std::string_view sourcePath = "",
                                               std::string_view sourceImage = "") const;
    [[nodiscard]] xq_ApiResult AddModel(std::string_view name,
                                        std::string_view sourceContourGroups = "",
                                        std::string_view sourcePath = "") const;
    [[nodiscard]] xq_ApiResult AddMesh(std::string_view name,
                                       std::string_view sourceModel = "") const;
    [[nodiscard]] xq_ApiResult AddSimulation(std::string_view name,
                                             std::string_view sourceMesh = "",
                                             std::string_view sourceModel = "") const;
    [[nodiscard]] xq_ApiResult AddROMSimulation(std::string_view name,
                                                std::string_view sourceMesh = "",
                                                std::string_view sourceModel = "",
                                                std::string_view sourcePath = "") const;
    [[nodiscard]] xq_ApiResult AddMultiPhysics(std::string_view name,
                                               std::string_view sourceMesh = "",
                                               std::string_view sourceModel = "") const;

    [[nodiscard]] xq_ApiResult ReadPath(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadSegmentation(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadModel(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadMesh(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadSimulation(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadROMSimulation(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadMultiPhysics(std::string_view name) const;
    [[nodiscard]] xq_ApiResult ReadGeometry(std::string_view modelName) const;

    // project.open(path) — opens an existing .xqproj through the same
    // WorkspaceManager path used by the GUI. This does not require a Python
    // runtime; it is part of the safe C++ automation service.
    [[nodiscard]] xq_ApiResult ProjectOpen(std::string_view path) const;

    // project.save(path) — saves the current DataStorage into an existing
    // project directory through WorkspaceManager.
    [[nodiscard]] xq_ApiResult ProjectSave(std::string_view path) const;

    // --- DataStorage access (internal) ---
    void SetDataStorage(mitk::DataStorage* ds);

private:
    [[nodiscard]] mitk::DataNode::Pointer FindNodePointer(std::string_view name) const;
    [[nodiscard]] xq_NodeDescriptor DescribeNode(const mitk::DataNode::Pointer& node) const;
    [[nodiscard]] xq_ApiResult ReadTypedNode(std::string_view name,
                                             std::string_view expectedClass,
                                             std::string_view expectedStage) const;
    [[nodiscard]] xq_ApiResult AddPipelineNode(std::string_view name,
                                               std::string_view stageName,
                                               std::string_view parentStageName,
                                               mitk::BaseData::Pointer data,
                                               const std::vector<std::pair<std::string, std::string>>& sources) const;

    mitk::DataStorage* m_DataStorage = nullptr;
    bool m_Available = false;
    std::string m_AvailabilityDiag =
        "Python API unavailable in this build: the xq Python extension module "
        "was not built because pybind11 and a matching Python 3.11 ABI are not linked. "
        "The C++ inspection service remains available for tests and internal callers.";
};
