#ifndef XQ_CORE_XQ_MESH_H
#define XQ_CORE_XQ_MESH_H

#include "core/NodeId.h"
#include "core/XQSurfaceModel.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

using MeshId = NodeId;

class VolumeMeshHandle {
public:
    VolumeMeshHandle();

    bool is_valid() const;
    std::size_t pointCount() const;
    std::size_t cellCount() const;
    void setCounts(std::size_t points, std::size_t cells);

private:
    std::size_t pointCount_;
    std::size_t cellCount_;
    bool valid_;
};

class SurfaceMeshHandle {
public:
    SurfaceMeshHandle();

    bool is_valid() const;
    std::size_t pointCount() const;
    std::size_t cellCount() const;
    void setCounts(std::size_t points, std::size_t cells);

private:
    std::size_t pointCount_;
    std::size_t cellCount_;
    bool valid_;
};

struct MeshRegion {
    int regionId;
    std::string name;
};

struct MeshBoundaryFace {
    int faceId;
    std::string name;
    FaceKind kind;
    std::optional<int> capId;
    std::vector<int> cellIds;
    std::vector<int> localFaces; // parallel to cellIds: which local face (0..3) of cellIds[k] is this boundary face; empty when unknown (star fallback)
};

struct MeshQualitySummary {
    double minQuality = 0.0;
    double maxQuality = 0.0;
    double meanQuality = 0.0;
    std::size_t elementCount = 0;
};

struct PreservedMeshArrays {
    bool hasGlobalNodeID = false;
    bool hasGlobalElementID = false;
    bool hasModelFaceID = false;
    bool hasCapID = false;
};

class XQMesh {
public:
    XQMesh();

    void setId(MeshId id);
    MeshId id() const;

    void setVolumeGrid(std::shared_ptr<VolumeMeshHandle> v);
    std::shared_ptr<VolumeMeshHandle> volumeGrid() const;

    void setSurfaceMesh(std::shared_ptr<SurfaceMeshHandle> s);
    std::shared_ptr<SurfaceMeshHandle> surfaceMesh() const;

    // Real generated surface mesh geometry (triangles, with per-triangle face
    // ids). Optional and separate from the count-only handle above, which the
    // reader path keeps using; a generated mesh carries this one.
    void setSurfaceTriangles(std::shared_ptr<XQTriangleSurfaceGeometryHandle> t);
    std::shared_ptr<XQTriangleSurfaceGeometryHandle> surfaceTriangles() const;
    bool hasSurfaceTriangles() const;

    // Real generated tetrahedral volume mesh geometry. Optional and separate
    // from the count-only volume handle above.
    void setVolumeTets(std::shared_ptr<XQTetVolumeMeshHandle> v);
    std::shared_ptr<XQTetVolumeMeshHandle> volumeTets() const;
    bool hasVolumeTets() const;

    void addRegion(const MeshRegion& r);
    const std::vector<MeshRegion>& regions() const;

    void addBoundaryFace(const MeshBoundaryFace& f);
    const std::vector<MeshBoundaryFace>& boundaryFaces() const;
    bool boundaryFaceById(int faceId, MeshBoundaryFace* out) const;

    void setSourceModelNode(const NodeId& n);
    bool hasSourceModelNode() const;
    NodeId sourceModelNode() const;

    void setQuality(const MeshQualitySummary& q);
    const MeshQualitySummary& quality() const;

    void setPreservedArrays(const PreservedMeshArrays& a);
    const PreservedMeshArrays& preservedArrays() const;

private:
    MeshId id_;
    std::shared_ptr<VolumeMeshHandle> volumeGrid_;
    std::shared_ptr<SurfaceMeshHandle> surfaceMesh_;
    std::shared_ptr<XQTriangleSurfaceGeometryHandle> surfaceTriangles_;
    std::shared_ptr<XQTetVolumeMeshHandle> volumeTets_;
    std::vector<MeshRegion> regions_;
    std::vector<MeshBoundaryFace> boundaryFaces_;
    bool hasSourceModel_;
    NodeId sourceModelNode_;
    MeshQualitySummary quality_;
    PreservedMeshArrays preservedArrays_;
};

} // namespace xq

#endif // XQ_CORE_XQ_MESH_H
