#ifndef XQ_CORE_SOURCE_SOURCE_VIEWS_H
#define XQ_CORE_SOURCE_SOURCE_VIEWS_H

#include "core/XQImageVolume.h"               // ScalarType
#include "core/XQMemoryImageBufferHandle.h"   // scalarSize (decode rule reuse)
#include "core/XQTetVolumeMeshHandle.h"       // Tet typedef
#include "core/XQTriangleSurfaceGeometryHandle.h" // Triangle typedef
#include "core/source/ReadSpan.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xq {

// Triangle / Tet are reused from the handles so the views speak the same value
// types as the producers (and reinterpret-to-flat-int stays well defined).
using SourceTriangle = XQTriangleSurfaceGeometryHandle::Triangle; // std::array<int,3>
using SourceTet = XQTetVolumeMeshHandle::Tet;                     // std::array<int,4>

// A read-only voxel block (whole volume, a region sub-block, or a single slab).
// Layout matches XQMemoryImageBufferHandle: x-fastest, components interleaved.
struct VoxelView {
    bool valid = false;
    ScalarType type = ScalarType::Unknown;
    int dims[3] = {0, 0, 0};
    int components = 1;
    ReadSpan<std::uint8_t> bytes;

    std::size_t voxelCount() const
    {
        return static_cast<std::size_t>(dims[0])
            * static_cast<std::size_t>(dims[1])
            * static_cast<std::size_t>(dims[2]);
    }

    std::size_t voxelIndex(int x, int y, int z) const
    {
        return static_cast<std::size_t>(x)
            + static_cast<std::size_t>(dims[0])
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(dims[1]) * static_cast<std::size_t>(z));
    }

    // Inline scalar decode, reusing XQMemoryImageBufferHandle's scalarSize rule
    // and native-byte-order memcpy semantics. No per-element virtual dispatch.
    double scalarAt(std::size_t voxelIndex, int comp = 0) const
    {
        if (!valid || comp < 0 || comp >= components) {
            return 0.0;
        }
        if (voxelIndex >= voxelCount()) {
            return 0.0;
        }
        const std::size_t scalar_size = XQMemoryImageBufferHandle::scalarSize(type);
        const std::size_t offset =
            (voxelIndex * static_cast<std::size_t>(components) + static_cast<std::size_t>(comp))
            * scalar_size;
        const std::uint8_t* data = bytes.data() + offset;
        return decode(data);
    }

private:
    double decode(const std::uint8_t* data) const
    {
        switch (type) {
        case ScalarType::Int8:
            return read_as<std::int8_t>(data);
        case ScalarType::UInt8:
            return read_as<std::uint8_t>(data);
        case ScalarType::Int16:
            return read_as<std::int16_t>(data);
        case ScalarType::UInt16:
            return read_as<std::uint16_t>(data);
        case ScalarType::Int32:
            return read_as<std::int32_t>(data);
        case ScalarType::UInt32:
            return read_as<std::uint32_t>(data);
        case ScalarType::Float32:
            return read_as<float>(data);
        case ScalarType::Float64:
            return read_as<double>(data);
        case ScalarType::Unknown:
            break;
        }
        return 0.0;
    }

    template <typename T>
    static double read_as(const std::uint8_t* data)
    {
        T value;
        std::memcpy(&value, data, sizeof(T));
        return static_cast<double>(value);
    }
};

// Triangles plus a parallel, equal-length face-id span (M8b-1: faceId is given
// alongside triangles in a single acquire).
struct TriangleView {
    ReadSpan<SourceTriangle> triangles;
    ReadSpan<int> faceIds;
};

// Metadata, derivable from handle count/dim accessors without touching data.
struct VoxelMeta {
    bool valid = false;
    ScalarType type = ScalarType::Unknown;
    int dims[3] = {0, 0, 0};
    int components = 1;
    std::size_t voxelCount = 0;
};

struct GeometryMeta {
    bool valid = false;
    std::size_t pointCount = 0;
    std::size_t triangleCount = 0;
    std::size_t tetCount = 0;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_SOURCE_VIEWS_H
