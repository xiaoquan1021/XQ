#ifndef XQ_CORE_GEOMETRY_TYPES_H
#define XQ_CORE_GEOMETRY_TYPES_H

#include <cmath>

namespace xq {

struct Point3 {
    double x;
    double y;
    double z;
};

using Vec3 = Point3;

inline Vec3 add(Point3 a, Point3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 sub(Point3 a, Point3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 scale(Vec3 value, double factor)
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

inline double dot(Vec3 a, Vec3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline Vec3 cross(Vec3 a, Vec3 b)
{
    return {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x),
    };
}

inline double norm(Vec3 value)
{
    return std::sqrt(dot(value, value));
}

inline Vec3 normalized(Vec3 value)
{
    const double length = norm(value);
    if (length == 0.0) {
        return {0.0, 0.0, 0.0};
    }
    return scale(value, 1.0 / length);
}

inline double distance(Point3 a, Point3 b)
{
    return norm(sub(a, b));
}

} // namespace xq

#endif // XQ_CORE_GEOMETRY_TYPES_H
