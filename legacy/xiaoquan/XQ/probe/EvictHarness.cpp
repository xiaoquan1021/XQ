#include "EvictHarness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace xq {
namespace probe {

namespace {

struct RawMap {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = nullptr;
    void* base = nullptr;
    std::size_t size = 0;
};

RawMap rawMap(const std::string& utf8)
{
    RawMap m;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                   static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen <= 0) {
        return m;
    }
    std::wstring w(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &w[0], wlen);
    m.hFile = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m.hFile == INVALID_HANDLE_VALUE) {
        return m;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(m.hFile, &sz) || sz.QuadPart == 0) {
        CloseHandle(m.hFile);
        m.hFile = INVALID_HANDLE_VALUE;
        return m;
    }
    m.hMap = CreateFileMapping(m.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m.hMap) {
        CloseHandle(m.hFile);
        m.hFile = INVALID_HANDLE_VALUE;
        return m;
    }
    m.base = MapViewOfFile(m.hMap, FILE_MAP_READ, 0, 0, 0);
    if (!m.base) {
        CloseHandle(m.hMap);
        CloseHandle(m.hFile);
        m.hMap = nullptr;
        m.hFile = INVALID_HANDLE_VALUE;
        return m;
    }
    m.size = static_cast<std::size_t>(sz.QuadPart);
    return m;
}

// SEH-isolated dangling read. NO C++ objects requiring unwinding may live here
// (MSVC forbids __try in functions that need object unwinding). Returns true if
// the strided read completed; false if it faulted (access violation) — which is
// exactly the dangling-view defect we want to observe. *sumOut is the running
// checksum (meaningful only when it returns true).
bool sehStridedRead(const volatile unsigned char* p, std::size_t stride,
                    std::size_t count, unsigned long long* sumOut)
{
    unsigned long long sum = 0;
    __try {
        for (std::size_t i = 0; i < count; ++i) {
            sum += p[i * stride];
        }
        *sumOut = sum;
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

const std::size_t kPage = 4096;

} // namespace

EvictResult runEvictHarness(const std::string& blobPath, const int dims[3],
                            const std::string& /*expectedSha*/)
{
    EvictResult r;
    const std::size_t voxels = static_cast<std::size_t>(dims[0])
        * static_cast<std::size_t>(dims[1]) * static_cast<std::size_t>(dims[2]);

    // --- Experiment A: refcount-gated eviction (expected SAFE) --------------
    {
        RawMap m = rawMap(blobPath);
        if (!m.base) {
            r.experimentA = "A: map failed, skipped";
        } else {
            // shared_ptr whose deleter tears down the mapping — models the M8b-1
            // keepalive. The resource manager (evictor) holds `owner`.
            HANDLE hFile = m.hFile, hMap = m.hMap;
            void* base = m.base;
            auto owner = std::shared_ptr<void>(base, [hFile, hMap](void* b) {
                if (b) UnmapViewOfFile(b);
                if (hMap) CloseHandle(hMap);
                if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
            });

            std::atomic<bool> leaseReleased{false};
            std::atomic<bool> evicted{false};
            std::atomic<bool> pinned{false};
            std::atomic<unsigned long long> frontSum{0};

            // Front-end thread: holds a "lease" (a copy of owner => use_count 2),
            // reads the whole volume, then releases the lease. It signals `pinned`
            // only AFTER taking the lease, so the evictor never observes a
            // transient use_count==1 before the lease exists (that race is a
            // harness artifact, not the contract under test).
            std::thread front([&]() {
                std::shared_ptr<void> lease = owner; // pin
                pinned.store(true);
                const std::size_t stride = (voxels / 4096) ? (voxels / 4096) : 1;
                const std::size_t n = voxels / stride;
                unsigned long long sum = 0;
                const unsigned char* p = static_cast<const unsigned char*>(base);
                for (std::size_t i = 0; i < n; ++i) {
                    sum += p[i * stride];
                }
                frontSum.store(sum);
                lease.reset();                 // release the lease
                leaseReleased.store(true);
            });

            // Evictor thread: waits until the lease is pinned, then only unmaps
            // once use_count drops back to 1 (no live lease). It honors the
            // refcount, so it waits for the front-end to finish reading.
            std::thread evictor([&]() {
                while (!pinned.load()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
                for (;;) {
                    if (owner.use_count() == 1) {
                        owner.reset();         // last ref -> deleter unmaps
                        evicted.store(true);
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            });

            front.join();
            evictor.join();

            r.aSafe = leaseReleased.load() && evicted.load() && frontSum.load() != 0;
            r.experimentA =
                std::string("A (refcount-gated): front-end read the whole volume "
                            "under a live lease; evictor waited until use_count==1 "
                            "then unmapped. checksum=")
                + std::to_string(frontSum.load())
                + (r.aSafe ? "  => SAFE: shared_ptr use_count is sufficient to gate "
                             "eviction IF the evictor honors it."
                           : "  => UNEXPECTED: gating did not complete cleanly.");
        }
    }

    // --- Experiment B: forced unmap ignoring use_count (expected DANGLE) ----
    {
        RawMap m = rawMap(blobPath);
        if (!m.base) {
            r.experimentB = "B: map failed, skipped";
        } else {
            std::atomic<bool> frontReadyForFault{false};
            std::atomic<bool> unmapped{false};
            std::atomic<bool> faulted{false};
            std::atomic<bool> completed{false};

            void* base = m.base;
            HANDLE hMap = m.hMap, hFile = m.hFile;
            const std::size_t stride = kPage; // step page-by-page to hit a fault fast
            const std::size_t n = (m.size / stride) ? (m.size / stride) : 1;

            // Front-end: signals it is about to do the long strided read, waits
            // for the forced unmap, then reads through the (now dangling) base.
            std::thread front([&]() {
                frontReadyForFault.store(true);
                while (!unmapped.load()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
                unsigned long long sum = 0;
                const bool ok = sehStridedRead(
                    static_cast<const volatile unsigned char*>(base), stride, n, &sum);
                if (ok) {
                    completed.store(true);  // no fault (e.g. OS kept pages)
                } else {
                    faulted.store(true);    // dangling read AV — defect shown
                }
            });

            // Evictor: ignores any refcount and force-unmaps while the lease is
            // (conceptually) live.
            std::thread evictor([&]() {
                while (!frontReadyForFault.load()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
                UnmapViewOfFile(base);
                unmapped.store(true);
            });

            front.join();
            evictor.join();

            // View already unmapped; close the rest.
            if (hMap) CloseHandle(hMap);
            if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

            r.bDangled = faulted.load();
            if (r.bDangled) {
                r.experimentB =
                    "B (forced unmap): evictor called UnmapViewOfFile while the "
                    "front-end held the borrowed view; the next read faulted "
                    "(EXCEPTION_ACCESS_VIOLATION).  => DEFECT CONFIRMED: nothing in "
                    "the M8b-1 lease can stop a non-refcount-driven evictor; the "
                    "borrowed span dangles. M8b-2 needs a real pin / reader-lock.";
            } else if (completed.load()) {
                r.experimentB =
                    "B (forced unmap): read completed without a fault (OS did not "
                    "reclaim the address range in time). The defect is latent but "
                    "real — the pointer is formally invalid after UnmapViewOfFile.";
            } else {
                r.experimentB = "B: inconclusive (no fault, no completion signal).";
            }
        }
    }

    return r;
}

} // namespace probe
} // namespace xq
