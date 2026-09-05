// MmapBlob — read-only Win32 file mapping (impl). Throwaway probe code.
#include "MmapBlob.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace xq {
namespace probe {

namespace {

// Owns the live mapping. Used as the control block behind the
// shared_ptr<const void> keepalive: when the last reference drops, the deleter
// tears the mapping down in the correct order (view → mapping → file).
struct Mapping {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = nullptr;
    void* base = nullptr;

    ~Mapping()
    {
        if (base) {
            UnmapViewOfFile(base);
        }
        if (hMap) {
            CloseHandle(hMap);
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }
};

MmapBlob fail(unsigned long err)
{
    MmapBlob r;
    r.ok = false;
    r.lastError = err;
    return r;
}

MmapBlob map_handle(HANDLE hFile)
{
    // Own the file handle immediately so every early-return path closes it.
    auto m = std::make_shared<Mapping>();
    m->hFile = hFile;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        return fail(GetLastError());
    }

    // Empty file: CreateFileMapping with size 0 maps the whole file but
    // MapViewOfFile then fails. Special-case it so we never return a bogus
    // base; lastError stays 0 since there is no OS error to report.
    if (fileSize.QuadPart == 0) {
        return fail(0);
    }

    // PAGE_READONLY mapping sized to the whole file (0,0 => entire file).
    m->hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m->hMap) {
        return fail(GetLastError());
    }

    // Map the entire file; the OS rounds the mapping out to whole pages, so a
    // non-page-aligned tail is mapped fine (trailing slack is just unused).
    m->base = MapViewOfFile(m->hMap, FILE_MAP_READ, 0, 0, 0);
    if (!m->base) {
        return fail(GetLastError());
    }

    MmapBlob r;
    r.ok = true;
    r.base = m->base;
    r.size = static_cast<std::size_t>(fileSize.QuadPart);
    // Alias the const-void keepalive onto the Mapping control block: the lease
    // holds this, and the Mapping dtor does the Unmap/Close on last release.
    r.keepalive = std::static_pointer_cast<const void>(
        std::shared_ptr<const Mapping>(m));
    r.lastError = 0;
    return r;
}

} // namespace

MmapBlob map_file_readonly(const std::wstring& path)
{
    HANDLE hFile = CreateFileW(path.c_str(),
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               nullptr,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return fail(GetLastError());
    }
    return map_handle(hFile);
}

MmapBlob map_file_readonly(const std::string& utf8_path)
{
    if (utf8_path.empty()) {
        return fail(ERROR_FILE_NOT_FOUND);
    }
    int wlen = MultiByteToWideChar(CP_UTF8, 0,
                                   utf8_path.c_str(),
                                   static_cast<int>(utf8_path.size()),
                                   nullptr, 0);
    if (wlen <= 0) {
        return fail(GetLastError());
    }
    std::wstring wpath(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
                        utf8_path.c_str(),
                        static_cast<int>(utf8_path.size()),
                        &wpath[0], wlen);
    return map_file_readonly(wpath);
}

} // namespace probe
} // namespace xq
