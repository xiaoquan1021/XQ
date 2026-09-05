#ifndef XQ_CORE_SOURCE_READ_SPAN_H
#define XQ_CORE_SOURCE_READ_SPAN_H

#include <cstddef>

namespace xq {

// Read-only contiguous view over a block of T. All access is non-virtual
// inline; the span never owns or mutates the underlying storage. The contiguous
// data() pointer lets callers feed external libraries directly (e.g.
// reinterpret_cast<const double*>(points.data()) for a TetGen-style REAL[3*n]).
template <class T>
class ReadSpan {
public:
    ReadSpan() = default;
    ReadSpan(const T* data, std::size_t size)
        : data_(data)
        , size_(size)
    {
    }

    const T* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    const T* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_READ_SPAN_H
