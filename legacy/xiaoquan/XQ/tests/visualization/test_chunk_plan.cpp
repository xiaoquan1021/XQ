// M9b-C chunk-plan partition invariant (header-only, no VTK). plan_chunks is the
// single source of truth for how a progressive upload tiles its cells; a wrong
// stride / dropped / duplicated cell here mis-renders silently (sub-pixel holes
// the rendered-image tests' tolerance swallows), so the invariant is asserted at
// the source: the returned ranges must tile [0, cellCount) exactly -- contiguous,
// gap-free, overlap-free, every cell covered once. CHECK-macro style.

#include "visualization/ChunkPlan.h"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

// Asserts the full partition invariant for plan_chunks(cellCount, maxPerChunk):
// every cell in [0, cellCount) is covered exactly once by contiguous, non-empty
// ranges of at most maxPerChunk. This is what would have caught the earlier
// fake-green (a begin+=1 tamper leaves the last chunk's end < cellCount and a gap
// between chunks -- both flagged here).
void assertTiles(std::size_t cellCount, std::size_t maxPerChunk, const char* tag)
{
    const std::vector<xq::ChunkRange> chunks = xq::plan_chunks(cellCount, maxPerChunk);

    if (cellCount == 0) {
        check(chunks.empty(), tag); // empty input -> no chunks
        return;
    }

    check(!chunks.empty(), tag);
    if (chunks.empty()) {
        return;
    }

    // Starts at 0, ends at cellCount.
    check(chunks.front().begin == 0, tag);
    check(chunks.back().end == cellCount, tag);

    const std::size_t step = maxPerChunk > 0 ? maxPerChunk : cellCount;
    std::vector<int> covered(cellCount, 0);
    std::size_t prevEnd = 0;
    bool contiguous = true;
    bool sized = true;
    bool nonEmpty = true;
    for (const xq::ChunkRange& c : chunks) {
        if (c.begin != prevEnd) contiguous = false;      // no gap / no overlap
        if (c.end <= c.begin) nonEmpty = false;          // non-empty
        if (c.end - c.begin > step) sized = false;       // within max
        for (std::size_t i = c.begin; i < c.end && i < cellCount; ++i) {
            ++covered[i];
        }
        prevEnd = c.end;
    }
    check(contiguous, tag);
    check(nonEmpty, tag);
    check(sized, tag);

    bool eachOnce = true;
    for (std::size_t i = 0; i < cellCount; ++i) {
        if (covered[i] != 1) eachOnce = false;
    }
    check(eachOnce, tag); // every cell covered exactly once

    // Chunk count matches the ceil-division the renderer reports as chunkCount.
    const std::size_t expected = (cellCount + step - 1) / step;
    check(chunks.size() == expected, tag);
}

} // namespace

int main()
{
    assertTiles(3042, 500, "3042/500 (7 chunks)");      // surface test case
    assertTiles(3042, 400, "3042/400");                  // equivalence test case
    assertTiles(120, 30, "120/30 (4 chunks, exact)");    // tet test case, divides evenly
    assertTiles(1, 1, "1/1 (single cell)");
    assertTiles(1000, 1, "1000/1 (one cell per chunk)");
    assertTiles(7, 1000, "7/1000 (one chunk covers all)");
    assertTiles(1000, 0, "1000/0 (zero max -> one chunk)");
    assertTiles(0, 100, "0/100 (empty)");
    assertTiles(1000000, 1u << 20, "1M/1M default-ish (single chunk)");

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all chunk-plan invariant checks passed\n");
    return 0;
}
