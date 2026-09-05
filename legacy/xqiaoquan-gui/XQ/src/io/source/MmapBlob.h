#ifndef XQ_IO_SOURCE_MMAP_BLOB_H
#define XQ_IO_SOURCE_MMAP_BLOB_H

#include <cstddef>
#include <memory>
#include <string>

namespace xq {

// Result of a Win32 read-only file mapping. While `keepalive` is held, `base`
// stays valid and points at the start of the mapped file; on the last
// shared_ptr release the view/map/file handles are torn down (view -> map ->
// file order) by the keepalive deleter.
//
// This header never includes <windows.h>; all Win32 detail lives in the .cpp.
struct MappedFile {
    bool ok = false;
    const void* base = nullptr;       // first byte of the mapped file
    std::size_t size = 0;             // file size in bytes
    std::shared_ptr<const void> keepalive; // owns the OS handles; unmaps on release
    unsigned long lastError = 0;      // GetLastError() on failure, 0 on success
};

// Maps an existing file read-only and shared-read into the process address
// space. A 0-byte file is reported as ok == false (Win32 cannot map an empty
// file). `path` is interpreted as UTF-8 and widened internally.
MappedFile map_file_readonly(const std::string& utf8Path);

} // namespace xq

#endif // XQ_IO_SOURCE_MMAP_BLOB_H
