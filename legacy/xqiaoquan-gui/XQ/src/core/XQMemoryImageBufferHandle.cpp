#include "core/XQMemoryImageBufferHandle.h"

#include <cstring>

namespace xq {

std::size_t XQMemoryImageBufferHandle::scalarSize(ScalarType type)
{
    switch (type) {
    case ScalarType::Int8:
    case ScalarType::UInt8:
        return 1;
    case ScalarType::Int16:
    case ScalarType::UInt16:
        return 2;
    case ScalarType::Int32:
    case ScalarType::UInt32:
    case ScalarType::Float32:
        return 4;
    case ScalarType::Float64:
        return 8;
    case ScalarType::Unknown:
        break;
    }
    return 0;
}

namespace {

// Reinterprets `size` bytes at `data` as a value of type T and returns it as
// double. The buffer stores native byte order (the adapter decodes into native
// order), so a plain memcpy is correct.
template <typename T>
double read_as(const std::uint8_t* data)
{
    T value;
    std::memcpy(&value, data, sizeof(T));
    return static_cast<double>(value);
}

} // namespace

XQMemoryImageBufferHandle::XQMemoryImageBufferHandle(ScalarType type,
                                                     const int dimensions[3],
                                                     int componentCount,
                                                     std::vector<std::uint8_t> bytes)
    : type_(type)
    , componentCount_(componentCount)
    , bytes_(std::move(bytes))
    , valid_(false)
{
    dimensions_[0] = dimensions[0];
    dimensions_[1] = dimensions[1];
    dimensions_[2] = dimensions[2];

    const std::size_t scalar_size = scalarSize(type);
    const bool dims_ok = dimensions_[0] >= 1 && dimensions_[1] >= 1 && dimensions_[2] >= 1;
    if (scalar_size == 0 || !dims_ok || componentCount_ < 1) {
        return;
    }

    const std::size_t voxels = static_cast<std::size_t>(dimensions_[0])
        * static_cast<std::size_t>(dimensions_[1])
        * static_cast<std::size_t>(dimensions_[2]);
    const std::size_t expected = voxels * static_cast<std::size_t>(componentCount_) * scalar_size;
    valid_ = bytes_.size() == expected;
}

bool XQMemoryImageBufferHandle::is_valid() const
{
    return valid_;
}

ScalarType XQMemoryImageBufferHandle::scalarType() const
{
    return type_;
}

int XQMemoryImageBufferHandle::dimensionX() const
{
    return dimensions_[0];
}

int XQMemoryImageBufferHandle::dimensionY() const
{
    return dimensions_[1];
}

int XQMemoryImageBufferHandle::dimensionZ() const
{
    return dimensions_[2];
}

int XQMemoryImageBufferHandle::componentCount() const
{
    return componentCount_;
}

std::size_t XQMemoryImageBufferHandle::voxelCount() const
{
    if (!valid_) {
        return 0;
    }
    return static_cast<std::size_t>(dimensions_[0])
        * static_cast<std::size_t>(dimensions_[1])
        * static_cast<std::size_t>(dimensions_[2]);
}

const std::vector<std::uint8_t>& XQMemoryImageBufferHandle::bytes() const
{
    return bytes_;
}

std::size_t XQMemoryImageBufferHandle::voxelIndex(int x, int y, int z) const
{
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(dimensions_[0])
        * (static_cast<std::size_t>(y)
           + static_cast<std::size_t>(dimensions_[1]) * static_cast<std::size_t>(z));
}

double XQMemoryImageBufferHandle::scalarAt(std::size_t voxelIndex, int component) const
{
    if (!valid_ || component < 0 || component >= componentCount_) {
        return 0.0;
    }
    if (voxelIndex >= voxelCount()) {
        return 0.0;
    }

    const std::size_t scalar_size = scalarSize(type_);
    const std::size_t offset =
        (voxelIndex * static_cast<std::size_t>(componentCount_) + static_cast<std::size_t>(component))
        * scalar_size;
    const std::uint8_t* data = bytes_.data() + offset;

    switch (type_) {
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

} // namespace xq
