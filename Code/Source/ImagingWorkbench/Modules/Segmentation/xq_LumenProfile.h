// XQ LumenProfile: vessel cross-section representation using modern C++ value semantics
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkPoint.h>
#include <mitkVector.h>
#include <mitkPlaneGeometry.h>

#include <vtkSmartPointer.h>

#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <memory>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace xq_detail {

inline mitk::Vector3D normalizeVec(mitk::Vector3D v) {
    double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-10) { v[0] /= len; v[1] /= len; v[2] /= len; }
    return v;
}

inline double vecLength(const mitk::Point3D& a, const mitk::Point3D& b) {
    double dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

} // namespace xq_detail

class XQMODULESEGMENTATION_EXPORT xq_LumenProfile
{
public:
    enum ProfileKind { CIRCLE, ELLIPSE, POLYGON, TENSION_POLYGON, SPLINE_POLYGON, MODEL };

    xq_LumenProfile();
    virtual ~xq_LumenProfile() = default;

    // Pure virtual
    virtual void GenerateProfilePoints() = 0;
    virtual std::string GetProfileKind() const = 0;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    virtual std::unique_ptr<xq_LumenProfile> Duplicate() const = 0;

    // Control point management
    void SetAnchorPoint(int index, const mitk::Point3D& pt);
    mitk::Point3D GetAnchorPoint(int index) const;
    void InsertAnchorPoint(int index, const mitk::Point3D& pt);
    void RemoveAnchorPoint(int index);
    int GetAnchorPointCount() const;
    std::vector<mitk::Point3D>& GetAnchorPoints();

    // Contour point access
    std::vector<mitk::Point3D> GetProfilePoints();

    // Center/scaling
    mitk::Point3D GetProfileCenter() const;
    void SetProfileCenter(const mitk::Point3D& pt);
    mitk::Point3D GetScalingPoint() const;

    // Bounding box
    std::array<double, 6> GetBoundingBox() const;

    // Placement
    void PlaceProfile(const mitk::Point3D& pt);
    virtual bool IsProfilePlaced() const;

    // Plane geometry
    void SetSlicePlane(mitk::PlaneGeometry::Pointer plane);
    mitk::PlaneGeometry::Pointer GetSlicePlane() const;

    // Subdivision
    void SetSubdivisionCount(int count);
    int GetSubdivisionCount() const;

    // Path position index (which position along the path this contour is at)
    void SetPathPosIndex(int index) { m_PathPosIndex = index; }
    int GetPathPosIndex() const { return m_PathPosIndex; }

    // Method used to create this contour (e.g., "circle", "ellipse", "splinepolygon")
    void SetMethod(std::string_view method) { m_Method = std::string(method); }
    std::string GetMethod() const { return m_Method; }

    // Setter for control points (bulk set)
    void SetAnchorPoints(const std::vector<mitk::Point3D>& points);

protected:
    void transferBaseState(xq_LumenProfile& target) const;

    std::vector<mitk::Point3D> m_AnchorPoints;
    std::vector<mitk::Point3D> m_ProfilePoints;
    mitk::Point3D m_ProfileCenter = mitk::Point3D();
    mitk::PlaneGeometry::Pointer m_SlicePlane = nullptr;
    bool m_ProfilePlaced = false;
    int m_SubdivisionCount = 36;
    int m_PathPosIndex = -1;
    std::string m_Method;
};
