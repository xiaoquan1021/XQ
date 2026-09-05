#include "core/XQSurfaceModel.h"

namespace xq {

SurfaceGeometryHandle::SurfaceGeometryHandle()
    : pointCount_(0)
    , cellCount_(0)
    , valid_(false)
{
}

bool SurfaceGeometryHandle::is_valid() const
{
    return valid_;
}

std::size_t SurfaceGeometryHandle::pointCount() const
{
    return pointCount_;
}

std::size_t SurfaceGeometryHandle::cellCount() const
{
    return cellCount_;
}

void SurfaceGeometryHandle::setCounts(std::size_t points, std::size_t cells)
{
    pointCount_ = points;
    cellCount_ = cells;
    valid_ = true;
}

XQSurfaceModel::XQSurfaceModel()
    : id_(ModelId::invalid())
    , source_(ModelSource::Unknown)
    , hasSourceContour_(false)
    , sourceContourGroupNode_(NodeId::invalid())
{
}

void XQSurfaceModel::setId(ModelId id)
{
    id_ = id;
}

ModelId XQSurfaceModel::id() const
{
    return id_;
}

void XQSurfaceModel::setGeometry(std::shared_ptr<SurfaceGeometryHandle> g)
{
    geometry_ = g;
}

std::shared_ptr<SurfaceGeometryHandle> XQSurfaceModel::geometry() const
{
    return geometry_;
}

void XQSurfaceModel::setSource(ModelSource s)
{
    source_ = s;
}

ModelSource XQSurfaceModel::source() const
{
    return source_;
}

void XQSurfaceModel::setSourceContourGroupNode(const NodeId& n)
{
    sourceContourGroupNode_ = n;
    hasSourceContour_ = true;
}

bool XQSurfaceModel::hasSourceContourGroupNode() const
{
    return hasSourceContour_;
}

NodeId XQSurfaceModel::sourceContourGroupNode() const
{
    if (!hasSourceContour_) {
        return NodeId::invalid();
    }
    return sourceContourGroupNode_;
}

void XQSurfaceModel::addFace(const ModelFace& f)
{
    faces_.push_back(f);
}

const std::vector<ModelFace>& XQSurfaceModel::faces() const
{
    return faces_;
}

bool XQSurfaceModel::faceById(int faceId, ModelFace* out) const
{
    for (std::size_t i = 0; i < faces_.size(); ++i) {
        if (faces_[i].faceId == faceId) {
            if (out != 0) {
                *out = faces_[i];
            }
            return true;
        }
    }
    return false;
}

void XQSurfaceModel::setPreservedArrays(const PreservedVtpArrays& a)
{
    preservedArrays_ = a;
}

const PreservedVtpArrays& XQSurfaceModel::preservedArrays() const
{
    return preservedArrays_;
}

} // namespace xq
