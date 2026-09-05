#ifndef XQ_CORE_XQ_SURFACE_MODEL_H
#define XQ_CORE_XQ_SURFACE_MODEL_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

using ModelId = NodeId;

enum class FaceKind {
    Unknown,
    Wall,
    Cap,
    Inlet,
    Outlet,
};

enum class ModelSource {
    Unknown,
    Loaded,
    Generated,
};

class SurfaceGeometryHandle {
public:
    SurfaceGeometryHandle();

    bool is_valid() const;
    std::size_t pointCount() const;
    std::size_t cellCount() const;
    void setCounts(std::size_t points, std::size_t cells);

private:
    std::size_t pointCount_;
    std::size_t cellCount_;
    bool valid_;
};

struct ModelFace {
    int faceId;
    std::string name;
    FaceKind kind;
    std::optional<int> capId;
    std::vector<int> boundaryLoopIds;
};

struct PreservedVtpArrays {
    bool hasGlobalNodeID = false;
    bool hasGlobalElementID = false;
    bool hasModelFaceID = false;
    bool hasCapID = false;
};

class XQSurfaceModel {
public:
    XQSurfaceModel();

    void setId(ModelId id);
    ModelId id() const;

    void setGeometry(std::shared_ptr<SurfaceGeometryHandle> g);
    std::shared_ptr<SurfaceGeometryHandle> geometry() const;

    // Real generated triangle geometry (loft/cap output). Optional and separate
    // from the count-only handle above, which the reader path keeps using; a
    // generated model carries this one, a loaded model carries the handle.
    void setTriangleGeometry(std::shared_ptr<XQTriangleSurfaceGeometryHandle> g);
    std::shared_ptr<XQTriangleSurfaceGeometryHandle> triangleGeometry() const;
    bool hasTriangleGeometry() const;

    void setSource(ModelSource s);
    ModelSource source() const;

    void setSourceContourGroupNode(const NodeId& n);
    bool hasSourceContourGroupNode() const;
    NodeId sourceContourGroupNode() const;

    void addFace(const ModelFace& f);
    const std::vector<ModelFace>& faces() const;
    bool faceById(int faceId, ModelFace* out) const;

    void setPreservedArrays(const PreservedVtpArrays& a);
    const PreservedVtpArrays& preservedArrays() const;

private:
    ModelId id_;
    std::shared_ptr<SurfaceGeometryHandle> geometry_;
    std::shared_ptr<XQTriangleSurfaceGeometryHandle> triangleGeometry_;
    std::vector<ModelFace> faces_;
    ModelSource source_;
    bool hasSourceContour_;
    NodeId sourceContourGroupNode_;
    PreservedVtpArrays preservedArrays_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SURFACE_MODEL_H
