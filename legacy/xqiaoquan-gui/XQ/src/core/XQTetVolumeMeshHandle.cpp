#include "core/XQTetVolumeMeshHandle.h"

namespace xq {

XQTetVolumeMeshHandle::XQTetVolumeMeshHandle() = default;

int XQTetVolumeMeshHandle::addPoint(const Point3& p)
{
    points_.push_back(p);
    return static_cast<int>(points_.size()) - 1;
}

void XQTetVolumeMeshHandle::addTet(int a, int b, int c, int d)
{
    tets_.push_back(Tet{a, b, c, d});
}

std::size_t XQTetVolumeMeshHandle::pointCount() const
{
    return points_.size();
}

std::size_t XQTetVolumeMeshHandle::tetCount() const
{
    return tets_.size();
}

const Point3& XQTetVolumeMeshHandle::point(std::size_t i) const
{
    return points_[i];
}

const XQTetVolumeMeshHandle::Tet& XQTetVolumeMeshHandle::tet(std::size_t i) const
{
    return tets_[i];
}

const std::vector<Point3>& XQTetVolumeMeshHandle::points() const
{
    return points_;
}

const std::vector<XQTetVolumeMeshHandle::Tet>& XQTetVolumeMeshHandle::tets() const
{
    return tets_;
}

bool XQTetVolumeMeshHandle::is_valid() const
{
    if (points_.empty() || tets_.empty()) {
        return false;
    }
    const int pointCount = static_cast<int>(points_.size());
    for (const Tet& t : tets_) {
        for (int idx : t) {
            if (idx < 0 || idx >= pointCount) {
                return false;
            }
        }
        if (t[0] == t[1] || t[0] == t[2] || t[0] == t[3] || t[1] == t[2]
            || t[1] == t[3] || t[2] == t[3]) {
            return false;
        }
    }
    return true;
}

} // namespace xq
