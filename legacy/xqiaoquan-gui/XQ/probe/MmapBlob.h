// MmapBlob — read-only Win32 file mapping for the scale probe (task 06-30-scale-probe).
//
// Throwaway spike code. Maps a content-addressed blob file into memory with a
// single MapViewOfFile so the probe can hand the raw bytes straight to the
// M8b-1 leases (reinterpret geometry / ReadSpan<uint8_t> voxels) with zero copy.
//
// The header keeps a clean C++ surface only — no <windows.h> leaks here. The
// concrete RAII (HANDLE / view pointer) lives in the .cpp behind a
// shared_ptr<const void> keepalive whose deleter unmaps and closes everything.
#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace xq {
namespace probe {

// Result of mapping a file read-only.
//
//  - On success:  ok == true, base points at the first mapped byte, size is the
//                 full file size in bytes, and keepalive owns the mapping. The
//                 bytes stay valid for exactly as long as keepalive is alive;
//                 dropping the last reference unmaps the view and closes the
//                 handles.
//  - On failure:  ok == false, base == nullptr, size == 0, keepalive empty,
//                 and lastError carries the Win32 GetLastError() code (0 for the
//                 self-detected "empty file" case, which has no OS error).
struct MmapBlob {
    bool ok = false;
    const void* base = nullptr;
    std::size_t size = 0;
    std::shared_ptr<const void> keepalive;
    unsigned long lastError = 0; // DWORD; valid when ok == false.
};

// Map an existing file read-only (shared read). Never throws; inspect .ok.
//
// A 0-byte file is reported as a failure (ok == false, lastError == 0) because
// MapViewOfFile cannot map an empty file and we must not hand back a bogus view.
// Non-page-aligned tails are fine: the whole file is mapped.
MmapBlob map_file_readonly(const std::wstring& path);

// UTF-8 convenience overload (paths from std::string blob roots).
MmapBlob map_file_readonly(const std::string& utf8_path);

} // namespace probe
} // namespace xq
