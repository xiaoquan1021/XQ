#include "core/XQMesh.h"

namespace xq {

VolumeMeshHandle::VolumeMeshHandle()
    : pointCount_(0)
    , cellCount_(0)
    , valid_(false)
{
}

bool VolumeMeshHandle::is_valid() const
{
    return valid_;
}

std::size_t VolumeMeshHandle::pointCount() const
{
    return pointCount_;
}

std::size_t VolumeMeshHandle::cellCount() const
{
    return cellCount_;
}

void VolumeMeshHandle::setCounts(std::size_t points, std::size_t cells)
{
    pointCount_ = points;
    cellCount_ = cells;
    valid_ = true;
}

SurfaceMeshHandle::SurfaceMeshHandle()
    : pointCount_(0)
    , cellCount_(0)
    , valid_(false)
{
}

bool SurfaceMeshHandle::is_valid() const
{
    return valid_;
}

std::size_t SurfaceMeshHandle::pointCount() const
{
    return pointCount_;
}

std::size_t SurfaceMeshHandle::cellCount() const
{
    return cellCount_;
}

void SurfaceMeshHandle::setCounts(std::size_t points, std::size_t cells)
{
    pointCount_ = points;
    cellCount_ = cells;
    valid_ = true;
}

XQMesh::XQMesh()
    : id_(MeshId::invalid())
    , hasSourceModel_(false)
    , sourceModelNode_(NodeId::invalid())
{
}

void XQMesh::setId(MeshId id)
{
    id_ = id;
}

MeshId XQMesh::id() const
{
    return id_;
}

void XQMesh::setVolumeGrid(std::shared_ptr<VolumeMeshHandle> v)
{
    volumeGrid_ = v;
}

std::shared_ptr<VolumeMeshHandle> XQMesh::volumeGrid() const
{
    return volumeGrid_;
}

void XQMesh::setSurfaceMesh(std::shared_ptr<SurfaceMeshHandle> s)
{
    surfaceMesh_ = s;
}

std::shared_ptr<SurfaceMeshHandle> XQMesh::surfaceMesh() const
{
    return surfaceMesh_;
}

void XQMesh::addRegion(const MeshRegion& r)
{
    regions_.push_back(r);
}

const std::vector<MeshRegion>& XQMesh::regions() const
{
    return regions_;
}

void XQMesh::addBoundaryFace(const MeshBoundaryFace& f)
{
    boundaryFaces_.push_back(f);
}

const std::vector<MeshBoundaryFace>& XQMesh::boundaryFaces() const
{
    return boundaryFaces_;
}

bool XQMesh::boundaryFaceById(int faceId, MeshBoundaryFace* out) const
{
    for (std::size_t i = 0; i < boundaryFaces_.size(); ++i) {
        if (boundaryFaces_[i].faceId == faceId) {
            if (out != 0) {
                *out = boundaryFaces_[i];
            }
            return true;
        }
    }
    return false;
}

void XQMesh::setSourceModelNode(const NodeId& n)
{
    sourceModelNode_ = n;
    hasSourceModel_ = true;
}

bool XQMesh::hasSourceModelNode() const
{
    return hasSourceModel_;
}

NodeId XQMesh::sourceModelNode() const
{
    if (!hasSourceModel_) {
        return NodeId::invalid();
    }
    return sourceModelNode_;
}

void XQMesh::setQuality(const MeshQualitySummary& q)
{
    quality_ = q;
}

const MeshQualitySummary& XQMesh::quality() const
{
    return quality_;
}

void XQMesh::setPreservedArrays(const PreservedMeshArrays& a)
{
    preservedArrays_ = a;
}

const PreservedMeshArrays& XQMesh::preservedArrays() const
{
    return preservedArrays_;
}

} // namespace xq
