#ifndef XQ_CORE_XQ_SURFACE_MODEL_PAYLOAD_H
#define XQ_CORE_XQ_SURFACE_MODEL_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQSurfaceModel.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/asset/AssetId.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's surface model (triangle geometry + ModelFace metadata +
// source contour-group binding). ModelingService-produced models and any future
// reader-loaded models share this one payload type, mirroring the path / mask
// payload design.
//
// clone() deep-copies the held XQSurfaceModel. The model's value members (faces,
// source binding) copy by value; its real triangle geometry is held through a
// shared_ptr, so clone() additionally copies that handle's contents into a fresh
// handle. This keeps edit-style commands (copy old payload, keep node id /
// provenance) from sharing mutable geometry between the original and the clone.
//
// geometryAssetId (M9b-E, optional): marks that the triangle geometry lives in
// an on-disk asset and may be resolved lazily (via the services-layer
// GeometryResourceManager) instead of being materialized into a resident handle.
// It coexists with the resident-handle path: a payload may carry a handle, an
// assetId, or both. clone() always carries the assetId reference forward (it is
// a lightweight value), independently of whether a resident handle is deep
// copied. The reader stamps it under lazyGeometry; services resolves it.
class XQSurfaceModelPayload : public XQPayload {
public:
    explicit XQSurfaceModelPayload(XQSurfaceModel model)
        : model_(std::move(model))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::SurfaceModel;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        XQSurfaceModel copy = model_; // value copy: faces + source (shallow geometry ptr)
        if (model_.hasTriangleGeometry()) {
            // Deep-copy the triangle geometry so the clone owns its own handle.
            auto geometry = std::make_shared<XQTriangleSurfaceGeometryHandle>(
                *model_.triangleGeometry());
            copy.setTriangleGeometry(geometry);
        }
        auto out = std::make_shared<XQSurfaceModelPayload>(std::move(copy));
        if (hasGeometryAssetId_) {
            out->setGeometryAssetId(geometryAssetId_);
        }
        return out;
    }

    const XQSurfaceModel& model() const
    {
        return model_;
    }

    XQSurfaceModel& model()
    {
        return model_;
    }

    // Lazy-geometry asset binding (M9b-E). Optional; absent by default so the
    // resident-handle path is unchanged.
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
    XQSurfaceModel model_;
    AssetId geometryAssetId_;
    bool hasGeometryAssetId_ = false;
};

} // namespace xq

#endif // XQ_CORE_XQ_SURFACE_MODEL_PAYLOAD_H
