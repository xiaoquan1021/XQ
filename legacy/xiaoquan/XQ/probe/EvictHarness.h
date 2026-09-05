#ifndef XQ_PROBE_EVICT_HARNESS_H
#define XQ_PROBE_EVICT_HARNESS_H

// scale-probe — concurrent evict harness (task 06-30-scale-probe, R3).
// Disposable spike code; only compiles under -DXQ_ENABLE_SCALE_PROBE=ON.
//
// Defect ③: M8b-1's ReadLease is a single-threaded move-only RAII handle whose
// borrowed view stays alive only because the keepalive shared_ptr<const void>
// keeps the backing storage referenced. There is NO evictor-visible pin / lock /
// try-evict hook. This harness runs two experiments against a real mmap source
// to find out where the contract holds and where it breaks under a background
// evictor:
//
//   Experiment A (refcount gating): the evictor only releases the mapping when
//   use_count of the keepalive is 1 (no live lease). Expectation: SAFE — the
//   front-end's live lease pins the mapping, evict waits. This validates that
//   shared_ptr use_count is *sufficient* to gate eviction IF the evictor honors it.
//
//   Experiment B (forced unmap): the evictor unmaps the view WHILE a lease is
//   live, ignoring use_count (models an LRU that force-evicts). Expectation:
//   the borrowed view dangles -> access violation. We catch it (SEH) and record
//   that the interface needs a real pin / reader-lock, because nothing in the
//   M8b-1 lease can stop a non-refcount-driven evictor.

#include <string>

namespace xq {
namespace probe {

struct EvictResult {
    std::string experimentA; // refcount-gated: conclusion text
    std::string experimentB; // forced-unmap: conclusion text
    bool aSafe = false;      // did A complete with the lease's data intact?
    bool bDangled = false;   // did B actually produce a dangling/AV (defect shown)?
};

// Runs both experiments against the voxel blob at `blobPath` (dims for the
// MappedVoxelSource). Self-contained: maps, acquires, spins a background evictor,
// reports. Never lets experiment B crash the process.
EvictResult runEvictHarness(const std::string& blobPath, const int dims[3],
                            const std::string& expectedSha);

} // namespace probe
} // namespace xq

#endif // XQ_PROBE_EVICT_HARNESS_H
