#include "io/source/MmapBlob.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>

namespace xq {

namespace {

// RAII control block holding the three OS resources of one mapping. The
// destructor tears them down in the required order: view first, then the
// mapping object, then the file handle. A shared_ptr<MappingControl> aliased to
// `const void` is handed back as the lease keepalive.
struct MappingControl {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = nullptr;
    const void* base = nullptr;

    ~MappingControl()
    {
        if (base != nullptr) {
            ::UnmapViewOfFile(base);
            base = nullptr;
        }
        if (hMap != nullptr) {
            ::CloseHandle(hMap);
            hMap = nullptr;
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }
};

std::wstring widen(const std::string& utf8)
{
    if (utf8.empty()) {
        return std::wstring();
    }
    const int needed = ::MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          &wide[0], needed);
    return wide;
}

} // namespace

MappedFile map_file_readonly(const std::string& utf8Path)
{
    MappedFile result;

    const std::wstring widePath = widen(utf8Path);
    if (widePath.empty()) {
        result.lastError = ERROR_INVALID_NAME;
        return result;
    }

    auto ctl = std::make_shared<MappingControl>();

    ctl->hFile = ::CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                               nullptr);
    if (ctl->hFile == INVALID_HANDLE_VALUE) {
        result.lastError = ::GetLastError();
        return result;   // ctl destructor closes the handle
    }

    // Reject a leaf that is itself a reparse point (symlink/junction). The
    // FILE_FLAG_OPEN_REPARSE_POINT above opened the link, not its target;
    // mid-path junctions are caught by the caller's isPathWithinRoot check.
    BY_HANDLE_FILE_INFORMATION info;
    if (!::GetFileInformationByHandle(ctl->hFile, &info)) {
        result.lastError = ::GetLastError();
        return result;
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        result.lastError = ERROR_ACCESS_DENIED;
        return result;
    }

    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(ctl->hFile, &fileSize)) {
        result.lastError = ::GetLastError();
        return result;
    }

    // A 0-byte file cannot be mapped (CreateFileMapping with maxsize 0 maps the
    // whole file, but MapViewOfFile then fails). Report it explicitly.
    if (fileSize.QuadPart == 0) {
        result.lastError = ERROR_FILE_INVALID;
        return result;
    }

    ctl->hMap = ::CreateFileMappingW(ctl->hFile, nullptr, PAGE_READONLY, 0, 0,
                                     nullptr);
    if (ctl->hMap == nullptr) {
        result.lastError = ::GetLastError();
        return result;
    }

    ctl->base = ::MapViewOfFile(ctl->hMap, FILE_MAP_READ, 0, 0, 0);
    if (ctl->base == nullptr) {
        result.lastError = ::GetLastError();
        return result;
    }

    result.ok = true;
    result.base = ctl->base;
    result.size = static_cast<std::size_t>(fileSize.QuadPart);
    result.keepalive = std::static_pointer_cast<const void>(ctl);
    result.lastError = 0;
    return result;
}

} // namespace xq
