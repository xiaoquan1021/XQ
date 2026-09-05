#ifndef XQ_CORE_XQ_MESH_PAYLOAD_H
#define XQ_CORE_XQ_MESH_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQMesh.h"
#include "core/XQPayload.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/asset/AssetId.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's mesh (surface triangles + tet volume cells + boundary-face
// metadata + quality summary + source model binding). Surface/volume meshes
// produced by the meshing services and any future reader-loaded meshes share
// this one payload type, mirroring the surface-model payload design.
//
// clone() deep-copies the held XQMesh. The mesh's value members (regions,
// boundary faces, quality, source binding) copy by value; its real surface /
// volume geometry is held through shared_ptrs, so clone() additionally copies
// those handles' contents into fresh handles. This keeps edit-style commands
// from sharing mutable geometry between the original and the clone.
//
// geometryAssetId (M9b-E, optional): marks that the mesh geometry (both surface
// and volume blobs of one asset) may be resolved lazily via the services-layer
// GeometryResourceManager instead of being materialized into resident handles.
// Coexists with the resident-handle path; clone() always carries the assetId
// reference forward. The reader stamps it under lazyGeometry; services resolves.
class XQMeshPayload : public XQPayload {
public:
    explicit XQMeshPayload(XQMesh mesh)
        : mesh_(std::move(mesh))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::Mesh;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        XQMesh copy = mesh_; // value copy: faces/regions/quality + shallow geometry ptrs
        if (mesh_.hasSurfaceTriangles()) {
            auto surface = std::make_shared<XQTriangleSurfaceGeometryHandle>(
                *mesh_.surfaceTriangles());
            copy.setSurfaceTriangles(surface);
        }
        if (mesh_.hasVolumeTets()) {
            auto volume = std::make_shared<XQTetVolumeMeshHandle>(*mesh_.volumeTets());
            copy.setVolumeTets(volume);
        }
        auto out = std::make_shared<XQMeshPayload>(std::move(copy));
        if (hasGeometryAssetId_) {
            out->setGeometryAssetId(geometryAssetId_);
        }
        return out;
    }

    const XQMesh& mesh() const
    {
        return mesh_;
    }

    XQMesh& mesh()
    {
        return mesh_;
    }

    // Lazy-geometry asset binding (M9b-E). Optional; absent by default so the
    // resident-handle path is unchanged. One assetId covers the asset's surface
    // and volume geometry blobs.
    void setGeometryAssetId(const AssetId& id)
    {
        geometryAssetId_ = id;
        hasGeometryAssetId_ = true;
    }

    bool hasGeometryAssetId() const
    {
        return hasGeometryAssetId_;
    }

    const AssetId& geometryAssetId() const
    {
        return geometryAssetId_;
    }

private:
    XQMesh mesh_;
    AssetId geometryAssetId_;
    bool hasGeometryAssetId_ = false;
};

} // namespace xq

#endif // XQ_CORE_XQ_MESH_PAYLOAD_H
